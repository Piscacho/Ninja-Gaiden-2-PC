// Fibras del XDK sobre fibras de Windows (NG2, sesion 4).
//
// El juego reparte su logica en tareas sobre fibras. SwitchToFiber del XDK (sub_8380E638 -> 0x83819200, ensamblador a mano)
// guarda r14-r31/f14-f31/v en la fibra actual ([KTHREAD+0x164]), carga los de la destino y cambia de pila con
// KeSetCurrentStackPointers. En codigo recompilado eso no se puede hacer (XenonRecomp lo dejo como "// ERROR 83819200; return",
// asi que todos los cambios de fibra eran vacios y el juego nunca avanzaba).
//
// Aqui cada fibra de invitado tiene una fibra de Windows con su propio PPCContext y su propia pila host: cambiar de fibra es
// SwitchToFiber de Windows. Estructura de fibra del juego (0xA50 bytes, CreateFiber = sub_8380E4F8):
//   +0x00 parametro, +0x04 base de reserva de pila, +0x08 base de pila, +0x0C limite de pila,
//   +0x1C lr inicial (0x838191F0 = arranque de fibra), +0x30 r1, +0x98.. r14..r31 (r31 = rutina), +0x1A8.. f14..f31.
#include "runtime.h"
#include <ppc_recomp_shared.h>
#include "tu_remap.h"   // sesion 26
#include <windows.h>
#include <mutex>
#include <string>
#include <atomic>
#include <unordered_map>

namespace
{
struct HostFiber
{
    void* host = nullptr;          // fibra de Windows
    PPCContext* ctx = nullptr;     // contexto de invitado de esta fibra
    uint32_t guest = 0;            // estructura de fibra del juego
    bool owned = false;            // creada aqui (CreateFiberEx + PPCContext propio): solo esas se liberan
};
std::mutex g_fiberMutex;
std::unordered_map<uint32_t, HostFiber*> g_fibers;
const bool g_fiberLog = getenv("NG2_FIBERLOG") != nullptr;

HostFiber* Find(uint32_t guest)
{
    std::lock_guard l(g_fiberMutex);
    auto it = g_fibers.find(guest);
    return it == g_fibers.end() ? nullptr : it->second;
}
void Register(HostFiber* f) { std::lock_guard l(g_fiberMutex); g_fibers[f->guest] = f; }

// Pila de invitado activa: PCR (+0x70 base, +0x74 limite) y KTHREAD (+0xD0 reserva, +0x5C base, +0x60 limite)
void ApplyStack(uint32_t fiber)
{
    uint32_t kthread = Load32(t_thread.pcr + 0x100);
    uint32_t alloc = Load32(fiber + 4), sbase = Load32(fiber + 8), slimit = Load32(fiber + 0xC);
    Store32(t_thread.pcr + 0x70, sbase); Store32(t_thread.pcr + 0x74, slimit);
    if (kthread) { Store32(kthread + 0xD0, alloc); Store32(kthread + 0x5C, sbase); Store32(kthread + 0x60, slimit); }
    t_thread.stackBase = sbase; t_thread.stackLimit = slimit;
}

void WINAPI FiberProc(void* param)
{
    auto* f = (HostFiber*)param;
    PPCContext& c = *f->ctx;
    c.r13.u64 = t_thread.pcr;   // la fibra puede arrancar en cualquier hilo
    uint32_t lr = Load32(f->guest + 0x1C);
    if (g_fiberLog) LOG("[fiber] arranca fibra %08X (rutina %08X, param %08X, pila %08X) en hilo %lu", f->guest,
                        (uint32_t)c.r31.u64, Load32(f->guest), (uint32_t)c.r1.u64, GetCurrentThreadId());
    PPCFunc* fn = (lr >= PPC_CODE_BASE && lr < PPC_CODE_BASE + PPC_CODE_SIZE && !(lr & 3)) ? PPC_LOOKUP_FUNC(g_base, lr) : nullptr;
    if (lr != NG2ADDR(0x838191F0u, 0x8383E8D0u)) LOG("[fiber] AVISO: fibra %08X con lr inicial %08X (no es el arranque de fibra)", f->guest, lr);
    if (!fn) { LOG("[fiber] lr inicial %08X sin funcion traducida", lr); ExitProcess(4); }
    fn(c, g_base);
    LOG("[fiber] la fibra %08X ha terminado (no deberia)", f->guest);
    ExitProcess(4);
}

HostFiber* CreateForGuest(uint32_t guest, const PPCContext& from)
{
    auto* f = new HostFiber;
    f->guest = guest;
    f->ctx = new PPCContext{};
    PPCContext& c = *f->ctx;
    c.fpscr = from.fpscr;
    c.r13 = from.r13;
    c.r1.u64 = __builtin_bswap64(*(uint64_t*)(g_base + guest + 0x30)) & 0xFFFFFFFFull;
    PPCRegister* gpr[] = { &c.r14, &c.r15, &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22,
                           &c.r23, &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31 };
    for (int i = 0; i < 18; i++) gpr[i]->u64 = __builtin_bswap64(*(uint64_t*)(g_base + guest + 0x98 + 8 * i));
    PPCRegister* fpr[] = { &c.f14, &c.f15, &c.f16, &c.f17, &c.f18, &c.f19, &c.f20, &c.f21, &c.f22,
                           &c.f23, &c.f24, &c.f25, &c.f26, &c.f27, &c.f28, &c.f29, &c.f30, &c.f31 };
    for (int i = 0; i < 18; i++) fpr[i]->u64 = __builtin_bswap64(*(uint64_t*)(g_base + guest + 0x1A8 + 8 * i));
    f->owned = true;
    f->host = CreateFiberEx(64 * 1024, 4 * 1024 * 1024, FIBER_FLAG_FLOAT_SWITCH, FiberProc, f);
    if (!f->host) { LOG("[fiber] CreateFiberEx fallo (%lu)", GetLastError()); ExitProcess(4); }
    Register(f);
    return f;
}

// Fibra de Windows del hilo actual (convierte el hilo si hace falta)
void* CurrentHostFiber()
{
    if (!IsThreadAFiber()) return ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
    return GetCurrentFiber();
}
}

// SwitchToFiber(fiber)
PPC_FUNC(sub_8380E638)
{
    uint32_t target = ctx.r3.u32;
    uint32_t kthread = Load32(t_thread.pcr + 0x100);
    uint32_t cur = Load32(kthread + 0x164);
    if (target == cur || !target) return;

    HostFiber* me = cur ? Find(cur) : nullptr;
    if (!me && cur)
    {   // fibra creada por ConvertThreadToFiber: es la fibra de Windows de este hilo
        me = new HostFiber; me->guest = cur; Register(me);
    }
    if (me) { me->host = CurrentHostFiber(); me->ctx = &ctx; }
    else CurrentHostFiber();

    HostFiber* t = Find(target);
    if (!t) t = CreateForGuest(target, ctx);

    static std::atomic<uint64_t> switches{ 0 };
    if (g_fiberLog)
    {   // cada 20000 cambios: fibras vivas con su rutina/parametro y cuantas veces se ha saltado a cada una
        static std::mutex hm; static std::unordered_map<uint32_t, uint64_t> hits; static std::unordered_map<uint32_t, uint32_t> lastLr; std::lock_guard l(hm); hits[target]++; lastLr[cur] = (uint32_t)ctx.lr;
        if (switches.load() % 20000 == 19999)
        {
            std::string line; char b2[80];
            for (auto& kv : hits) { snprintf(b2, sizeof(b2), " %08X(p%08X lr%08X)x%llu", kv.first, Load32(kv.first), lastLr[kv.first], (unsigned long long)kv.second); line += b2; }
            LOG("[fiber] destinos:%s", line.c_str()); hits.clear();
        }
    }
    uint64_t n = ++switches;
    if (g_fiberLog && (n <= 40 || n % 5000 == 0)) LOG("[fiber] #%llu %08X -> %08X (hilo %lu)", (unsigned long long)n, cur, target, GetCurrentThreadId());

    Store32(kthread + 0x164, target);
    ApplyStack(target);
    SwitchToFiber(t->host);

    // De vuelta en esta fibra (quiza en otro hilo): r13 = PCR del hilo actual
    ctx.r13.u64 = t_thread.pcr;
}

// DeleteFiber(fiber): libera tambien la fibra de Windows (salvo la del hilo actual, que el juego cierra con ExitThread)
PPC_FUNC_IMPL(__imp__sub_8380E5D8);
PPC_FUNC(sub_8380E5D8)
{
    uint32_t fiber = ctx.r3.u32;
    uint32_t cur = Load32(Load32(t_thread.pcr + 0x100) + 0x164);
    HostFiber* f = Find(fiber);
    if (g_fiberLog) LOG("[fiber] borrar fibra %08X (rutina %08X, param %08X)", fiber, f && f->ctx ? (uint32_t)0 : 0, Load32(fiber));
    __imp__sub_8380E5D8(ctx, base);
    if (f && fiber != cur)
    {
        { std::lock_guard l(g_fiberMutex); g_fibers.erase(fiber); }
        // Las fibras de hilo (ConvertThreadToFiber) no se liberan: su contexto y su fibra de Windows son los del hilo
        if (f->owned && f->host && f->ctx && f->ctx != &ctx) { DeleteFiber(f->host); delete f->ctx; }
        if (f->owned) delete f;
    }
}
