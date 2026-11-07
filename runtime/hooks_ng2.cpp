// Ganchos de diagnostico sobre funciones del juego (NG2). Las funciones traducidas son "weak": definirlas aqui las envuelve.
// NG2_HOOKLOG=1 registra cada llamada (con hilo, r3..r5 y lr) de las funciones de la lista.
#include "runtime.h"
#include <ppc_recomp_shared.h>
#include <windows.h>
#include <atomic>
#include <cmath>
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

// Sesion 21 (diagnostico, NG2_HAIRLOG=1): pelo de Sonia. sub_836ED768(pelo r3, objeto r4) copia la matriz de anclaje
// (64 bytes desde *(objeto+4)) a pelo+32, simula (sub_836ECF20 x15) y escribe las tiras (sub_836ED380).
PPC_FUNC_IMPL(__imp__sub_836ED768);
PPC_FUNC(sub_836ED768)
{
    static const bool on = getenv("NG2_HAIRLOG") != nullptr;
    static std::atomic<int> n{ 0 };
    uint32_t hair = ctx.r3.u32, obj = ctx.r4.u32;
    int callNo = n.fetch_add(1);
    bool log = on && callNo < 24;
    if (on && callNo == 3000)   // vuelca los nodos del esqueleto (0x84E78000, 8 KB) para compararlos con Xenia
        if (FILE* f = fopen("nodos_84E78000.bin", "wb")) { fwrite(g_base + 0x84E78000, 1, 0x2000, f); fclose(f); LOG("[hair] nodos volcados (llamada %d)", callNo); }
    auto mat = [](uint32_t a) {
        std::string s; if (!a) return std::string("(nulo)");
        for (int i = 0; i < 16; i++) { uint32_t u = Load32(a + i * 4); float f; memcpy(&f, &u, 4); char b[24]; snprintf(b, sizeof(b), "%s%.3g", i % 4 ? " " : " | ", f); s += b; }
        return s; };
    if (log)
    {
        uint32_t src = Load32(obj + 4);
        LOG("[hair] pelo %08X obj %08X flag20 %u src %08X:%s", hair, obj, (unsigned)g_base[obj + 20], src, mat(src).c_str());
    }
    __imp__sub_836ED768(ctx, base);
    if (log)
    {
        auto vec = [](uint32_t a) { std::string s; for (int i = 0; i < 4; i++) { uint32_t u = Load32(a + i * 4); float f; memcpy(&f, &u, 4); char b[24]; snprintf(b, sizeof(b), " %.3g", f); s += b; } return s; };
        uint32_t t = Load32(obj + 108), pts = Load32(obj + 100);
        LOG("[hair]   tipo *(obj+108) = %04X %04X | obj+48:%s | obj+128:%s", t ? (unsigned)__builtin_bswap16(*(uint16_t*)(g_base + t)) : 0xDEAD, t ? (unsigned)__builtin_bswap16(*(uint16_t*)(g_base + t + 2)) : 0xDEAD,
            vec(obj + 48).c_str(), mat(obj + 128).c_str());
        { std::string hx; for (int i = 20; i < 100; i++) { char b[4]; snprintf(b, sizeof(b), "%02X", g_base[obj + i]); hx += b; } LOG("[hair]   bytes obj+20..99: %s", hx.c_str()); }
        { std::string hx; uint32_t o2 = Load32(obj + 112); for (int i = 0; i < 96 && o2; i++) { char b[4]; snprintf(b, sizeof(b), "%02X", g_base[o2 + i]); hx += b; } LOG("[hair]   *(obj+112) @%08X: %s", o2, hx.c_str()); }
        { uint32_t t = Load32(obj + 108); std::string l; for (int i = 0; t && i < 12; i++) { char b[12]; snprintf(b, sizeof(b), " %04X", (unsigned)__builtin_bswap16(*(uint16_t*)(g_base + t + i * 2))); l += b; } LOG("[hair]   lista *(obj+108) @%08X:%s", t, l.c_str()); }
        LOG("[hair]   obj+12 %08X +116 %08X +120 %08X +124 %08X, pelo+16 %08X", Load32(obj + 12), Load32(obj + 116), Load32(obj + 120), Load32(obj + 124), Load32(hair + 16));
        LOG("[hair]   puntos @%08X: p0%s | p1%s | p2%s | p0+16%s", pts, vec(pts).c_str(), vec(pts + 64).c_str(), vec(pts + 128).c_str(), vec(pts + 16).c_str());
    }
}

// Sesion 21 (diagnostico, NG2_HAIRLOG=1): conductores del pelo. sub_836EDAF0 crea un conductor (vtable 0x82282CD0) y lo cuelga
// del nodo del hueso raiz; sub_836EC7F0 (entrada de esa vtable) deberia aplicar la simulacion a los huesos hijos.
PPC_FUNC_IMPL(__imp__sub_836EDAF0);
PPC_FUNC(sub_836EDAF0)
{
    static const bool on = getenv("NG2_HAIRLOG") != nullptr; static std::atomic<int> n{ 0 };
    uint32_t obj = ctx.r3.u32, a4 = ctx.r4.u32;
    __imp__sub_836EDAF0(ctx, base);
    if (on && n.fetch_add(1) < 30) LOG("[hair] EDAF0 obj %08X r4 %08X obj+12 %d nodo %08X lista +112 %08X +116 %08X", obj, a4, (int)Load32(obj + 12), Load32(obj + 4), Load32(obj + 4) ? Load32(Load32(obj + 4) + 112) : 0, Load32(obj + 4) ? Load32(Load32(obj + 4) + 116) : 0);
}
PPC_FUNC_IMPL(__imp__sub_836EC7F0);
PPC_FUNC(sub_836EC7F0)
{
    static const bool on = getenv("NG2_HAIRLOG") != nullptr; static std::atomic<int> n{ 0 };
    int k = n.fetch_add(1);
    if (on && (k < 10 || k % 5000 == 0)) LOG("[hair] EC7F0 llamada %d r3 %08X r4 %08X r5 %08X lr %08X", k, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, (uint32_t)ctx.lr);
    __imp__sub_836EC7F0(ctx, base);
}
// Sesion 21 (diagnostico, NG2_HAIRLOG=1): aplicador de fisica por modelo (desde la actualizacion del esqueleto sub_8247B2A8).
// Sale sin hacer nada si *(r3+12) == 0 o si la global 0x84BA235C != 0.
PPC_FUNC_IMPL(__imp__sub_836FA630);
PPC_FUNC(sub_836FA630)
{
    static const bool on = getenv("NG2_HAIRLOG") != nullptr; static std::atomic<int> n{ 0 };
    uint32_t a3 = ctx.r3.u32, a4 = ctx.r4.u32; int k = n.fetch_add(1);
    static const bool force = getenv("NG2_FORCEPHYS") != nullptr;   // prueba: estado 2 (activo) si esta parado
    if (force && Load32(a3 + 12) == 0) Store32(a3 + 12, 2);
    __imp__sub_836FA630(ctx, base);
    if (on && (k < 12 || k % 3000 == 0)) LOG("[hair] FA630 llamada %d obj %08X modelo %08X obj+12 %08X global %08X -> %llX", k, a3, a4, Load32(a3 + 12), Load32(0x84BA235C), (unsigned long long)ctx.r3.u64);
}

// Sesion 21 (diagnostico, NG2_HAIRLOG=1): quien llama a los metodos de los conductores del pelo (objetos en 0x40283500-0x40284700)
static void DriverCallLog(const char* slot, PPCContext& ctx)
{
    static const bool on = getenv("NG2_HAIRLOG") != nullptr;
    if (!on || ctx.r3.u32 < 0x40283500 || ctx.r3.u32 >= 0x40284700) return;
    static std::atomic<int> n{ 0 }; int k = n.fetch_add(1);
    if (k < 40 || k % 2000 == 0) LOG("[hair] conductor %s r3 %08X r4 %08X r5 %08X lr %08X (llamada %d)", slot, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, (uint32_t)ctx.lr, k);
}
#define NG2_DRIVER_HOOK(addr, slot) PPC_FUNC_IMPL(__imp__sub_##addr); PPC_FUNC(sub_##addr) { DriverCallLog(slot, ctx); __imp__sub_##addr(ctx, base); }
NG2_DRIVER_HOOK(82318CB8, "[1]")
NG2_DRIVER_HOOK(82831588, "[3]")
NG2_DRIVER_HOOK(82318D10, "[4]")
NG2_DRIVER_HOOK(828315B0, "[5]")
NG2_DRIVER_HOOK(82318D38, "[6]")
NG2_DRIVER_HOOK(82318D60, "[7]")
NG2_DRIVER_HOOK(82318D88, "[8]")
NG2_DRIVER_HOOK(82318DB0, "[9]")
NG2_DRIVER_HOOK(836EDBA0, "[10]")

// Sesion 21 (diagnostico, NG2_HAIRLOG=1): funciones que recorren la lista de conductores de un nodo y preguntan su tipo
static void TypeWalkLog(const char* name, PPCContext& ctx)
{
    static const bool on = getenv("NG2_HAIRLOG") != nullptr; if (!on) return;
    static std::atomic<int> n{ 0 }; int k = n.fetch_add(1);
    if (k < 30 || k % 5000 == 0) LOG("[hair] %s r3 %08X r4 %08X r5 %08X lr %08X (llamada %d)", name, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, (uint32_t)ctx.lr, k);
}
#define NG2_TYPEWALK_HOOK(addr) PPC_FUNC_IMPL(__imp__sub_##addr); PPC_FUNC(sub_##addr) { TypeWalkLog("sub_" #addr, ctx); __imp__sub_##addr(ctx, base); }
NG2_TYPEWALK_HOOK(837F9B30)
NG2_TYPEWALK_HOOK(838024F8)
NG2_TYPEWALK_HOOK(838026D8)
NG2_TYPEWALK_HOOK(838029C8)
NG2_TYPEWALK_HOOK(83803220)
NG2_TYPEWALK_HOOK(83869370)

// Sesion 24 (diagnostico, NG2_EFFLOG=1): sub_8365D730 resuelve nombres -> indice en la tabla fija de 5099 nombres (0x83AEBC98)
// para 4 listas de registros de 12 bytes (punteros en 0x83A97120). Registra cuantos registros quedan resueltos.
PPC_FUNC_IMPL(__imp__sub_8365D730);
PPC_FUNC(sub_8365D730)
{
    __imp__sub_8365D730(ctx, base);
    static const bool on = getenv("NG2_EFFLOG") != nullptr;
    if (!on) return;
    auto str = [](uint32_t a) { std::string s; for (int i = 0; a && i < 80; i++) { char c = (char)g_base[a + i]; if (!c) break; s += c; } return s; };
    for (int l = 0; l < 4; l++)
    {
        uint32_t lst = Load32(0x83A97120 + l * 4); int n = 0, ok = 0; std::string ej;
        for (uint32_t r = lst; r && Load32(r) && n < 4000; r += 12, n++)
        {
            uint32_t idx = Load32(r + 4);
            if (idx < 5099 && str(Load32(0x83AEBC98 + idx * 4)) == str(Load32(r))) ok++;
            else if (ej.size() < 300) { char b[160]; snprintf(b, sizeof(b), " [%s -> %u]", str(Load32(r)).c_str(), idx); ej += b; }
        }
        LOG("[eff] lista %d @%08X: %d registros, %d resueltos;%s", l, lst, n, ok, ej.c_str());
    }
}
// Sesion 24 (diagnostico, NG2_EFFLOG=1): carga del paquete de efectos comun (indice 4791 = effect\effpk_common\effpk_common.emc).
// 0x82443498: si !sub_822FF5A8(4791, obj+0x134) -> sub_823D6F00(4791, *(obj+0x134), r5)
PPC_FUNC_IMPL(__imp__sub_822FF5A8);
PPC_FUNC(sub_822FF5A8)
{
    static const bool on = getenv("NG2_EFFLOG") != nullptr; static std::atomic<int> n{ 0 };
    uint32_t a3 = ctx.r3.u32, a4 = ctx.r4.u32;
    __imp__sub_822FF5A8(ctx, base);
    if (on && a3 >= 4700 && a3 < 5099 && n.fetch_add(1) < 60) LOG("[eff] sub_822FF5A8(%u, %08X [%08X]) -> %llX", a3, a4, a4 ? Load32(a4) : 0, (unsigned long long)ctx.r3.u64);
}
PPC_FUNC_IMPL(__imp__sub_823D6F00);
PPC_FUNC(sub_823D6F00)
{
    static const bool on = getenv("NG2_EFFLOG") != nullptr; static std::atomic<int> n{ 0 };
    uint32_t a3 = ctx.r3.u32, a4 = ctx.r4.u32, a5 = ctx.r5.u32;
    __imp__sub_823D6F00(ctx, base);
    if (on && n.fetch_add(1) < 60) LOG("[eff] sub_823D6F00(%u, %08X, %08X) -> %llX", a3, a4, a5, (unsigned long long)ctx.r3.u64);
}
// Sesion 24 (diagnostico, NG2_EFFLOG=1): sub_8370AB48(indice) busca el bloque de tipo 33 de un recurso (tabla en
// *(0x83BBC090 + 112)); lo llama el codigo de golpes (sub_82418130). Cuenta aciertos y fallos.
PPC_FUNC_IMPL(__imp__sub_8370AB48);
PPC_FUNC(sub_8370AB48)
{
    static const bool on = getenv("NG2_EFFLOG") != nullptr; static std::atomic<int> n{ 0 }, nz{ 0 };
    uint32_t a3 = ctx.r3.u32;
    __imp__sub_8370AB48(ctx, base);
    if (!on) return;
    int k = n.fetch_add(1); if (ctx.r3.u32 == 0) nz++;
    if (k < 30 || k % 2000 == 0) LOG("[eff] sub_8370AB48(%u) -> %08X (llamadas %d, nulas %d, lr %08X, tabla %08X, limite %u)", a3 & 0xFFFF, ctx.r3.u32, k + 1, nz.load(), (uint32_t)ctx.lr, Load32(0x83BBC090 + 112), Load32(0x84C236A4));
}

// Sesion 25 (diagnostico, NG2_PATHLOG=1): tiempo de sub_82385A00 (busqueda en un grafo de 256 nodos durante las cargas de zona;
// aparece en los tirones de 130 ms del hilo principal).
PPC_FUNC_IMPL(__imp__sub_82385A00);
PPC_FUNC(sub_82385A00)
{
    static const bool on = getenv("NG2_PATHLOG") != nullptr;
    if (!on) { __imp__sub_82385A00(ctx, base); return; }
    LARGE_INTEGER f, a, b; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&a);
    uint32_t r3 = ctx.r3.u32, r4 = ctx.r4.u32, r5 = ctx.r5.u32, lr = (uint32_t)ctx.lr;
    __imp__sub_82385A00(ctx, base);
    QueryPerformanceCounter(&b);
    static int n = 0; static double tot = 0; double ms = (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart; tot += ms; n++;
    if (ms > 1.0 || n % 500 == 0) LOG("[path] sub_82385A00(%08X, %08X, %08X) lr %08X: %.2f ms -> %08X (llamadas %d, total %.0f ms)", r3, r4, r5, lr, ms, ctx.r3.u32, n, tot);
}

// Sesion 25: barrera de los hilos de XAudio (sub_8374F078). Cada hilo pone a 1 el byte de su CPU (KPCR+0x10C) en una palabra de
// 64 bits y espera a que esten todos los de la mascara (campos +0x134..+0x148 del objeto); el que la ve completa la pone a 0. Si el
// que completa vuelve a entrar en la ronda siguiente antes de que el otro vea el 0 (en el PC, con el otro hilo desplanificado unos
// ms), los dos se quedan esperando para siempre: el audio se paraba a los 1-8 minutos (todo, musica y efectos). Ahora es una
// barrera con contador de generacion en el host (sin esa carrera); la palabra del juego se sigue actualizando igual.
// NG2_OLDAUDIOBARRIER=1 = la del juego.
#include <unordered_map>
#include <mutex>
#include <intrin.h>
PPC_FUNC_IMPL(__imp__sub_8374F078);
PPC_FUNC(sub_8374F078)
{
    static const bool old = getenv("NG2_OLDAUDIOBARRIER") != nullptr;
    if (old) { __imp__sub_8374F078(ctx, base); return; }
    uint32_t obj = ctx.r3.u32, word = ctx.r4.u32;
    if (Load32(obj + 0x130) == 0) return;
    uint32_t cpu = base[ctx.r13.u32 + 0x10C] & 7;
    uint64_t expected = 0;
    for (int k = 0; k < 6; k++) if (Load32(obj + 0x134 + 4 * k)) expected |= 1ull << (56 - 8 * k);
    uint64_t mine = cpu < 8 ? 1ull << (56 - 8 * cpu) : 0;
    struct Bar { uint64_t arrived = 0; std::atomic<uint64_t> gen{ 0 }; };
    static std::mutex m; static std::unordered_map<uint32_t, Bar*> bars;
    Bar* b; uint64_t g;
    {
        std::lock_guard lk(m);
        Bar*& slot = bars[word]; if (!slot) slot = new Bar; b = slot;
        base[word + cpu] = 1;
        b->arrived |= mine;
        if (expected && (b->arrived & expected) == expected)
        {
            b->arrived = 0; memset(base + word, 0, 8);
            b->gen.fetch_add(1);
            return;
        }
        g = b->gen.load();
    }
    LARGE_INTEGER f, t0, t1; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
    bool warned = false;
    for (uint32_t spins = 0; b->gen.load() == g; spins++)
    {
        if (spins < 4000) _mm_pause(); else SwitchToThread();
        if ((spins & 0xFFF) == 0xFFF && !warned)
        {
            QueryPerformanceCounter(&t1);
            if ((t1.QuadPart - t0.QuadPart) > f.QuadPart / 4)
            {
                warned = true; static std::atomic<int> nw{ 0 };
                if (nw++ < 20) { std::lock_guard lk(m); LOG("[audio] barrera de XAudio %08X lleva >250 ms: cpu %u, llegados %016llX, mascara %016llX", word, cpu, (unsigned long long)b->arrived, (unsigned long long)expected); }
            }
        }
    }
}

// Sesion 25 (diagnostico, NG2_CAMLOG=1): argumentos de sub_822F9FB8 (camara: f1 campo de vision, f3 proporcion, f4/f5 planos)
PPC_FUNC_IMPL(__imp__sub_822F9FB8);
PPC_FUNC(sub_822F9FB8)
{
    static const bool on = getenv("NG2_CAMLOG") != nullptr;
    if (on) { static std::atomic<int> n{ 0 }; int k = n++; if (k < 40 || k % 3000 == 0) LOG("[cam] sub_822F9FB8(r3 %08X) f1 %.4f f2 %.4f f3 %.4f f4 %.4f f5 %.4f lr %08X (%d)", ctx.r3.u32, ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64, ctx.f5.f64, (uint32_t)ctx.lr, k); }
    __imp__sub_822F9FB8(ctx, base);
}

// Sesion 25 (panoramico Hor+): sub_837CC920 construye el frustum de la camara con f1 = campo * proporcion y f2 = 1 / proporcion:
// tanX = tan(f1 / 2), tanY = tanX * f2 (+0x20..+0x2C), planos f3/f4. Con la proporcion panoramica (constante 16/9 sustituida en
// ApplyPanoCamera) el angulo horizontal crecia en lineal con la proporcion (a 32:9 casi 180 grados). Aqui se conserva el tanY de
// 16:9 y tanX = tanY * proporcion: misma vista vertical que en la consola y mas vista a los lados.
float PanoAspect();
PPC_FUNC_IMPL(__imp__sub_837CC920);
PPC_FUNC(sub_837CC920)
{
    static const float pa = PanoAspect();
    { static const bool camlog = getenv("NG2_CAMLOG") != nullptr; static std::atomic<int> n{ 0 }; if (camlog && n++ < 40) LOG("[cam] sub_837CC920 f1 %.5f f2 %.5f f3 %.4f f4 %.4f lr %08X pa %.4f", ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64, (uint32_t)ctx.lr, pa); }
    static const bool horCam = getenv("NG2_PANOCAM") != nullptr;   // experimento: Hor+ en la camara del juego (por defecto se hace en el VS)
    if (horCam && pa > 16.0f / 9.0f + 1e-3f && ctx.f2.f64 > 0 && std::fabs(ctx.f2.f64 - 1.0 / pa) < 1e-4)
    {
        double fov = ctx.f1.f64 * ctx.f2.f64;                       // campo de vision del juego (radianes)
        double ty = std::tan(fov * (16.0 / 9.0) * 0.5) / (16.0 / 9.0); // vertical de 16:9
        double tx = ty * pa;
        ctx.f1.f64 = (double)(float)(2.0 * std::atan(tx));
    }
    __imp__sub_837CC920(ctx, base);
}
