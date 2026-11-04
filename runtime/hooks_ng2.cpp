// Ganchos de diagnostico sobre funciones del juego (NG2). Las funciones traducidas son "weak": definirlas aqui las envuelve.
// NG2_HOOKLOG=1 registra cada llamada (con hilo, r3..r5 y lr) de las funciones de la lista.
#include "runtime.h"
#include <ppc_recomp_shared.h>
#include <windows.h>
#include <atomic>
#include <cstdlib>

namespace
{
const bool g_hookLog = getenv("NG2_HOOKLOG") != nullptr;
void HookLog(const char* name, PPCContext& ctx, std::atomic<int>& n)
{
    if (!g_hookLog) return;
    int k = n++;
    if (k < 20 || (k % 1000) == 0)
        LOG("[hook] %s #%d tid %lu r3=%08X r4=%08X r5=%08X lr=%08X", name, k, GetCurrentThreadId(), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, (uint32_t)ctx.lr);
}
}

#define NG2_TRACE_HOOK(addr, label) \
    PPC_FUNC_IMPL(__imp__sub_##addr); \
    PPC_FUNC(sub_##addr) { static std::atomic<int> n{ 0 }; HookLog(label, ctx, n); __imp__sub_##addr(ctx, base); }

// Gancho que registra el valor devuelto (r3) cada vez que cambia
#define NG2_RET_HOOK(addr, label)     PPC_FUNC_IMPL(__imp__sub_##addr);     PPC_FUNC(sub_##addr) { static std::atomic<uint64_t> last{ ~0ull }; uint32_t a3 = ctx.r3.u32; __imp__sub_##addr(ctx, base);         if (g_hookLog && last.exchange(ctx.r3.u64) != ctx.r3.u64) LOG("[ret] %s(r3=%08X) -> %llX (t=%lu ms)", label, a3, (unsigned long long)ctx.r3.u64, GetTickCount()); }
NG2_TRACE_HOOK(823D6510, "fin de lectura (rutina de usuario)")
NG2_TRACE_HOOK(838199D8, "BasepIoCompletion")
NG2_RET_HOOK(8245A2C0, "estado escena 8245A2C0")
NG2_RET_HOOK(82462160, "82462160")
NG2_RET_HOOK(836676C0, "tipo6: listo? 836676C0")
NG2_RET_HOOK(8245F7A8, "8245F7A8")
NG2_RET_HOOK(82442EB8, "82442EB8")
NG2_TRACE_HOOK(8364A830, "estado crea tarea de transicion")
NG2_TRACE_HOOK(82442AD8, "tarea transicion de escena")
NG2_TRACE_HOOK(82444408, "82444408 destruye")
NG2_TRACE_HOOK(82444530, "82444530 destruye")
NG2_TRACE_HOOK(82462080, "82462080 destruye")
NG2_TRACE_HOOK(823D6268, "823D6268 destruye")
NG2_TRACE_HOOK(822F2CA0, "CreateTask")
NG2_TRACE_HOOK(83632B88, "cargar escena")
NG2_TRACE_HOOK(8364AB70, "vt83A97AE8+00")
NG2_TRACE_HOOK(8364ADB8, "vt83A97AE8+04")
NG2_TRACE_HOOK(8364AE58, "vt83A97AE8+0C")
NG2_TRACE_HOOK(8364B380, "vt83A97AE8+10")
NG2_TRACE_HOOK(83649A80, "vt83A97AE8+18")
NG2_TRACE_HOOK(83649B90, "vt83A97AE8+1C")
NG2_TRACE_HOOK(83649BD0, "vt83A97AE8+20")
NG2_TRACE_HOOK(8364C8A0, "vt83A97AE8+24")
NG2_TRACE_HOOK(8364C978, "vt83A97AE8+2C")
NG2_TRACE_HOOK(8364A7E0, "vt83A97AE8+30")
NG2_TRACE_HOOK(8364A8C0, "vt83A97AE8+38")
NG2_TRACE_HOOK(8364A658, "vt83A97AE8+40")
NG2_TRACE_HOOK(83650E20, "inicio de sonido (ng2_sound.xgs)")
NG2_TRACE_HOOK(83651158, "metodo que llama al inicio de sonido")
NG2_TRACE_HOOK(83653AF0, "registra 83651158 (a)")
NG2_TRACE_HOOK(8365C070, "registra 83651158 (b)")
NG2_TRACE_HOOK(83652250, "83652250")
NG2_TRACE_HOOK(824645D0, "PAUSA (a)")
NG2_TRACE_HOOK(82464658, "REANUDAR (a)")
NG2_TRACE_HOOK(8364F298, "PAUSA (b)")
NG2_TRACE_HOOK(8364F370, "REANUDAR (b)")

// NG2_DUMPMEM=<dir hex>:<bytes hex>:<segundos>[;...]: vuelca memoria de invitado (palabras BE) a esos segundos del arranque
#include <thread>
#include <string>
namespace
{
struct DumpMemInit
{
    DumpMemInit()
    {
        const char* s = getenv("NG2_DUMPMEM");
        if (!s) return;
        std::string spec = s;
        std::thread([spec] {
            size_t p = 0;
            while (p < spec.size())
            {
                size_t e = spec.find(';', p); std::string one = spec.substr(p, e == std::string::npos ? std::string::npos : e - p);
                p = e == std::string::npos ? spec.size() : e + 1;
                uint32_t addr = 0, len = 0; double secs = 0;
                if (sscanf(one.c_str(), "%x:%x:%lf", &addr, &len, &secs) != 3) continue;
                static const DWORD t0 = GetTickCount();
                while (GetTickCount() - t0 < secs * 1000) Sleep(50);
                for (uint32_t o = 0; o < len; o += 32)
                {
                    char line[160]; int k = snprintf(line, sizeof(line), "[mem] %08X:", addr + o);
                    for (uint32_t j = 0; j < 32 && o + j < len; j += 4) k += snprintf(line + k, sizeof(line) - k, " %08X", Load32(addr + o + j));
                    LOG("%s", line);
                }
            }
        }).detach();
    }
} g_dumpMemInit;
}

// Sesion 12 (activo por defecto; NG2_NOSAVEFIX=1 lo quita): con perfil, el gestor de partidas (sub_83661650, objeto 0x85309728) queda en la peticion
// 12 (+0x130) esperando a que el almacenamiento del usuario 0 (0x83B271B0, 0x6E0 bytes) este listo, pero esa estructura nunca se
// inicializa (palabra 0 = 0, +8 = -1). Se llama una vez a su inicializacion sub_8365EA70(estructura, usuario 0, 0, 1) como hace el
// propio gestor en 0x83661E38.
PPC_FUNC_IMPL(__imp__sub_83661650);
PPC_FUNC_IMPL(__imp__sub_83660658);
PPC_FUNC(sub_83661650)
{
    static const bool on = getenv("NG2_NOSAVEFIX") == nullptr;
    static bool done = false;
    static DWORD lastLog = 0;
    if (on && getenv("NG2_SAVELOG") && GetTickCount() - lastLog > 5000) { lastLog = GetTickCount(); LOG("[save] gestor: peticion %u paso %u, usuario0 palabra0 %08X", Load32(0x85309728 + 0x130), Load32(0x85309728 + 0x134), Load32(0x83B271B0)); }
    // La inicializacion del usuario llega despues de emitir la peticion y la deja inactiva (+0x6DC = 6, campo de operacion = 1):
    // se relanza la operacion con sub_83660658(usuario 0), como hace el gestor al emitir la peticion.
    uint32_t w0 = Load32(0x83B271B0);
    if (on && Load32(0x85309728 + 0x130) == 12 && w0 != 0 && ((w0 >> 8) & 0xF) == 1 && Load32(0x83B271B0 + 0x6DC) == 6)
    {
        static int retries = 0;
        if (retries++ < 3)
        {
            PPCContext c = ctx; c.r3.u64 = 0;
            __imp__sub_83660658(c, base);
            LOG("[save] operacion del usuario 0 relanzada -> %llX (palabra 0 %08X, estado %u)", (unsigned long long)c.r3.u64, Load32(0x83B271B0), Load32(0x83B271B0 + 0x6DC));
        }
    }
    (void)done;
    __imp__sub_83661650(ctx, base);
}
