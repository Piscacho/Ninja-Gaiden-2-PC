// Imports de xboxkrnl/xam que usa Ninja Gaiden II y no usaba GH:WoR (sesion 3 de NG2).
// Comportamiento segun la documentacion publica del kernel de Xbox 360 y Xenia (BSD).
#include "objects.h"
#include <ppc_recomp_shared.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

bool g_importTrace = getenv("NG2_IMPTRACE") != nullptr;
void ImportTrace(const char* name, PPCContext& ctx)
{
    // Las llamadas muy frecuentes se omiten salvo NG2_IMPTRACE=2
    static const bool all = getenv("NG2_IMPTRACE") && atoi(getenv("NG2_IMPTRACE")) >= 2;
    static const char* noisy[] = { "RtlEnterCriticalSection", "RtlLeaveCriticalSection", "KeQueryPerformanceFrequency", "XamInputGetState",
        "KfAcquireSpinLock", "KfReleaseSpinLock", "KeAcquireSpinLockAtRaisedIrql", "KeReleaseSpinLockFromRaisedIrql", "RtlTryEnterCriticalSection",
        "KeTlsGetValue", "KeTlsSetValue", "KeRaiseIrqlToDpcLevel", "KfLowerIrql", "KeQuerySystemTime", "NtYieldExecution" };
    if (!all) for (const char* n : noisy) if (!strcmp(n, name)) return;
    LOG("d> %lu %s(%08X, %08X, %08X, %08X) lr=%08X", GetCurrentThreadId(), name, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, (uint32_t)ctx.lr);
}

// ---------------- printf del kernel (sprintf, _snprintf, _vsnprintf, swprintf...) ----------------
// Argumentos variables: los 8 primeros en r3..r10, el resto en la zona de parametros de la pila (r1 + 0x50 + 8*k).
// va_list: puntero a ranuras de 8 bytes (big-endian). Los double van en ranuras enteras (mismo patron de bits).
namespace
{
struct ArgSource
{
    PPCContext* ctx = nullptr; int reg = 0;  // argumentos directos: siguiente registro (3..10)
    uint32_t va = 0;                          // va_list (si != 0)
    int stackIdx = 0;
    uint64_t Next()
    {
        if (va) { uint64_t v = __builtin_bswap64(*(uint64_t*)(g_base + va)); va += 8; return v; }
        if (reg <= 10)
        {   // r3..r10 no son contiguos en PPCContext (r0..r2 van en medio)
            const PPCRegister* rs[] = { &ctx->r3, &ctx->r4, &ctx->r5, &ctx->r6, &ctx->r7, &ctx->r8, &ctx->r9, &ctx->r10 };
            return rs[(reg++) - 3]->u64;
        }
        uint64_t v = __builtin_bswap64(*(uint64_t*)(g_base + ctx->r1.u32 + 0x50 + 8 * stackIdx)); stackIdx++; return v;
    }
};

template<bool Wide> uint32_t GChar(uint32_t p) { return Wide ? __builtin_bswap16(*(uint16_t*)(g_base + p)) : g_base[p]; }

std::string NarrowFromGuest(uint32_t p, bool wide, int maxLen = -1)
{
    std::string s;
    if (!p) return "(null)";
    for (int i = 0; maxLen < 0 || i < maxLen; i++)
    {
        uint32_t c = wide ? __builtin_bswap16(*(uint16_t*)(g_base + p + 2 * i)) : g_base[p + i];
        if (!c) break;
        s += c < 0x80 ? (char)c : '?';
    }
    return s;
}

// Formatea con la convencion de MSVC (en las funciones anchas, %s = cadena ancha y %S = estrecha)
template<bool Wide>
std::u16string Format(uint32_t fmt, ArgSource& a)
{
    std::u16string out;
    auto put = [&](const std::string& s) { for (unsigned char c : s) out += (char16_t)c; };
    for (uint32_t p = fmt;; )
    {
        uint32_t c = GChar<Wide>(p); p += Wide ? 2 : 1;
        if (!c) break;
        if (c != '%') { out += (char16_t)c; continue; }
        std::string spec = "%";
        // flags
        for (;;) { c = GChar<Wide>(p); if (c && strchr("-+ #0", (int)c)) { spec += (char)c; p += Wide ? 2 : 1; } else break; }
        // ancho
        if (GChar<Wide>(p) == '*') { spec += std::to_string((int32_t)a.Next()); p += Wide ? 2 : 1; }
        else while ((c = GChar<Wide>(p)) >= '0' && c <= '9') { spec += (char)c; p += Wide ? 2 : 1; }
        int prec = -1;
        if (GChar<Wide>(p) == '.')
        {
            spec += '.'; p += Wide ? 2 : 1; prec = 0;
            if (GChar<Wide>(p) == '*') { prec = (int32_t)a.Next(); spec += std::to_string(prec); p += Wide ? 2 : 1; }
            else { std::string d; while ((c = GChar<Wide>(p)) >= '0' && c <= '9') { d += (char)c; p += Wide ? 2 : 1; } spec += d; prec = d.empty() ? 0 : atoi(d.c_str()); }
        }
        // longitud
        int lng = 0; bool forceWide = false, forceNarrow = false;
        for (;;)
        {
            c = GChar<Wide>(p);
            if (c == 'l') { lng++; forceWide = true; p += Wide ? 2 : 1; }
            else if (c == 'h') { forceNarrow = true; p += Wide ? 2 : 1; }
            else if (c == 'I' && GChar<Wide>(p + (Wide ? 2 : 1)) == '6' && GChar<Wide>(p + (Wide ? 4 : 2)) == '4') { lng = 2; p += Wide ? 6 : 3; }
            else if (c == 'I' && GChar<Wide>(p + (Wide ? 2 : 1)) == '3' && GChar<Wide>(p + (Wide ? 4 : 2)) == '2') { lng = 0; p += Wide ? 6 : 3; }
            else if (c == 'z' || c == 't' || c == 'j' || c == 'L' || c == 'q') { if (c == 'j' || c == 'q') lng = 2; p += Wide ? 2 : 1; }
            else break;
        }
        uint32_t conv = GChar<Wide>(p); p += Wide ? 2 : 1;
        char buf[512];
        switch (conv)
        {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o':
        {
            uint64_t v = a.Next();
            spec += "ll"; spec += (char)conv;
            if (lng >= 2) snprintf(buf, sizeof(buf), spec.c_str(), (long long)v);
            else if (conv == 'd' || conv == 'i') snprintf(buf, sizeof(buf), spec.c_str(), (long long)(int32_t)v);
            else snprintf(buf, sizeof(buf), spec.c_str(), (unsigned long long)(uint32_t)v);
            put(buf); break;
        }
        case 'p': snprintf(buf, sizeof(buf), "%08X", (uint32_t)a.Next()); put(buf); break;
        case 'c': case 'C':
        {
            uint32_t v = (uint32_t)a.Next();
            out += (char16_t)(((conv == 'c') == !Wide || forceNarrow) && !forceWide ? (v & 0xFF) : (v & 0xFFFF));
            break;
        }
        case 's': case 'S': case 'Z':
        {
            uint32_t sp = (uint32_t)a.Next();
            bool wideArg = conv == 'Z' ? false : forceWide ? true : forceNarrow ? false : ((conv == 's') == Wide);
            std::string s = NarrowFromGuest(sp, wideArg, prec);
            std::string f = spec; f += 's';
            if (spec.size() > 1 && sp)
            {   // ancho / alineacion: formatear la cadena ya extraida
                std::vector<char> b(s.size() + 600); snprintf(b.data(), b.size(), f.c_str(), s.c_str()); put(b.data());
            }
            else if (wideArg && sp)
            {   // copiar tal cual los caracteres anchos (no solo ASCII)
                for (int i = 0; prec < 0 || i < prec; i++) { uint16_t ch = __builtin_bswap16(*(uint16_t*)(g_base + sp + 2 * i)); if (!ch) break; out += (char16_t)ch; }
            }
            else put(s);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
        {
            uint64_t bits = a.Next(); double d; memcpy(&d, &bits, 8);
            spec += (char)conv; snprintf(buf, sizeof(buf), spec.c_str(), d); put(buf); break;
        }
        case 'n': a.Next(); break;
        case '%': out += u'%'; break;
        case 0: return out;
        default: out += u'%'; out += (char16_t)conv; break;
        }
    }
    return out;
}

// Escribe el resultado en el bufer de invitado (count = maximo de caracteres; ~0u = sin limite). Devuelve la longitud o -1.
template<bool Wide>
int32_t Emit(uint32_t buf, uint32_t count, const std::u16string& s, bool alwaysTerminate)
{
    size_t n = s.size();
    bool fits = count == ~0u || n < count;
    size_t w = fits ? n : count;
    if (buf)
    {
        for (size_t i = 0; i < w; i++)
        {
            if (Wide) *(uint16_t*)(g_base + buf + 2 * i) = __builtin_bswap16((uint16_t)s[i]);
            else g_base[buf + i] = (uint8_t)s[i];
        }
        if (fits || alwaysTerminate)
        {
            size_t z = fits ? n : (count ? count - 1 : 0);
            if (count != 0) { if (Wide) *(uint16_t*)(g_base + buf + 2 * z) = 0; else g_base[buf + z] = 0; }
        }
    }
    return fits ? (int32_t)n : -1;
}
}

GUEST_IMPL(sprintf) { ArgSource a{ &ctx, 5 }; auto s = Format<false>(ctx.r4.u32, a); ctx.r3.s64 = Emit<false>(ctx.r3.u32, ~0u, s, true); }
GUEST_IMPL(_snprintf) { ArgSource a{ &ctx, 6 }; auto s = Format<false>(ctx.r5.u32, a); ctx.r3.s64 = Emit<false>(ctx.r3.u32, ctx.r4.u32, s, false); }
GUEST_IMPL(_vsnprintf) { ArgSource a{ &ctx, 11, ctx.r6.u32 }; auto s = Format<false>(ctx.r5.u32, a); ctx.r3.s64 = Emit<false>(ctx.r3.u32, ctx.r4.u32, s, false); }
GUEST_IMPL(swprintf) { ArgSource a{ &ctx, 5 }; auto s = Format<true>(ctx.r4.u32, a); ctx.r3.s64 = Emit<true>(ctx.r3.u32, ~0u, s, true); }
GUEST_IMPL(_snwprintf) { ArgSource a{ &ctx, 6 }; auto s = Format<true>(ctx.r5.u32, a); ctx.r3.s64 = Emit<true>(ctx.r3.u32, ctx.r4.u32, s, false); }
GUEST_IMPL(vswprintf) { ArgSource a{ &ctx, 11, ctx.r5.u32 }; auto s = Format<true>(ctx.r4.u32, a); ctx.r3.s64 = Emit<true>(ctx.r3.u32, ~0u, s, true); }
GUEST_IMPL(_vscwprintf) { ArgSource a{ &ctx, 11, ctx.r4.u32 }; ctx.r3.s64 = (int64_t)Format<true>(ctx.r3.u32, a).size(); }

// ---------------- cadenas Rtl ----------------
// ANSI_STRING / UNICODE_STRING: { u16 Length, u16 MaximumLength, u32 Buffer }
GUEST_IMPL(RtlInitUnicodeString)
{
    uint32_t us = ctx.r3.u32, src = ctx.r4.u32, n = 0;
    if (src) while (*(uint16_t*)(g_base + src + 2 * n)) n++;
    Store16(us, (uint16_t)(n * 2)); Store16(us + 2, (uint16_t)(src ? n * 2 + 2 : 0)); Store32(us + 4, src);
}
GUEST_IMPL(RtlFreeAnsiString) { Store16(ctx.r3.u32, 0); Store16(ctx.r3.u32 + 2, 0); Store32(ctx.r3.u32 + 4, 0); }
// RtlUnicodeStringToAnsiString(ansi_out, unicode_in, allocate)
GUEST_IMPL(RtlUnicodeStringToAnsiString)
{
    uint32_t dst = ctx.r3.u32, src = ctx.r4.u32;
    uint32_t len = __builtin_bswap16(*(uint16_t*)(g_base + src)) / 2, sbuf = Load32(src + 4);
    uint32_t dbuf = Load32(dst + 4), cap = __builtin_bswap16(*(uint16_t*)(g_base + dst + 2));
    if (ctx.r5.u32) { cap = len + 1; dbuf = GuestAllocVirtual(cap, 0x10); }
    else if (len + 1 > cap) { ctx.r3.u64 = 0x80000005; return; }   // STATUS_BUFFER_OVERFLOW
    for (uint32_t i = 0; i < len; i++) { uint16_t c = __builtin_bswap16(*(uint16_t*)(g_base + sbuf + 2 * i)); g_base[dbuf + i] = c < 0x100 ? (uint8_t)c : '?'; }
    g_base[dbuf + len] = 0;
    Store16(dst, (uint16_t)len); Store16(dst + 2, (uint16_t)cap); Store32(dst + 4, dbuf);
    ctx.r3.u64 = 0;
}
// RtlUnicodeToMultiByteN(dst, dst_size, written_ptr, src, src_bytes)
GUEST_IMPL(RtlUnicodeToMultiByteN)
{
    uint32_t n = std::min(ctx.r4.u32, ctx.r7.u32 / 2);
    for (uint32_t i = 0; i < n; i++) { uint16_t c = __builtin_bswap16(*(uint16_t*)(g_base + ctx.r6.u32 + 2 * i)); g_base[ctx.r3.u32 + i] = c < 0x100 ? (uint8_t)c : '?'; }
    if (ctx.r5.u32) Store32(ctx.r5.u32, n);
    ctx.r3.u64 = 0;
}
// RtlMultiByteToUnicodeN(dst, dst_bytes, written_ptr, src, src_size)
GUEST_IMPL(RtlMultiByteToUnicodeN)
{
    uint32_t n = std::min(ctx.r4.u32 / 2, ctx.r7.u32);
    for (uint32_t i = 0; i < n; i++) *(uint16_t*)(g_base + ctx.r3.u32 + 2 * i) = __builtin_bswap16(g_base[ctx.r6.u32 + i]);
    if (ctx.r5.u32) Store32(ctx.r5.u32, n * 2);
    ctx.r3.u64 = 0;
}
GUEST_IMPL(RtlUpcaseUnicodeChar) { uint16_t c = (uint16_t)ctx.r3.u32; ctx.r3.u64 = (c >= 'a' && c <= 'z') ? c - 32 : c; }
// RtlCompareStringN(s1, s1_len, s2, s2_len, case_insensitive)
GUEST_IMPL(RtlCompareStringN)
{
    uint32_t a = ctx.r3.u32, b = ctx.r5.u32, n = std::min(ctx.r4.u32, ctx.r6.u32);
    for (uint32_t i = 0; i < n; i++)
    {
        int x = g_base[a + i], y = g_base[b + i];
        if (ctx.r7.u32) { x = toupper(x); y = toupper(y); }
        if (x != y) { ctx.r3.s64 = x - y; return; }
    }
    ctx.r3.s64 = (int32_t)ctx.r4.u32 - (int32_t)ctx.r6.u32;
}
// RtlTimeFieldsToTime(TIME_FIELDS*, LARGE_INTEGER* out): {Year, Month, Day, Hour, Minute, Second, Milliseconds, Weekday} u16
GUEST_IMPL(RtlTimeFieldsToTime)
{
    auto f = [&](int i) { return __builtin_bswap16(*(uint16_t*)(g_base + ctx.r3.u32 + 2 * i)); };
    SYSTEMTIME st{ f(0), f(1), f(7), f(2), f(3), f(4), f(5), f(6) };
    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) { ctx.r3.u64 = 0; return; }
    Store64(ctx.r4.u32, (uint64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime);
    ctx.r3.u64 = 1;
}

// ---------------- sincronizacion ----------------
// KeInitializeSemaphore(sem_ptr, count, limit): cabecera de dispatcher tipo 5 + limite en +0x10
GUEST_IMPL(KeInitializeSemaphore)
{
    uint32_t s = ctx.r3.u32;
    memset(g_base + s, 0, 0x14);
    g_base[s] = kObjSemaphore; g_base[s + 2] = 0x14 / 4;
    Store32(s + 4, ctx.r4.u32); Store32(s + 8, s + 8); Store32(s + 12, s + 8); Store32(s + 0x10, ctx.r5.u32);
    GetObjectByPointer(s);
}
// KeReleaseSemaphore(sem_ptr, increment, adjustment, wait) -> estado anterior
GUEST_IMPL(KeReleaseSemaphore)
{
    auto o = GetObjectByPointer(ctx.r3.u32);
    LONG prev = 0;
    ReleaseSemaphore(o->waitable, ctx.r5.s32, &prev);
    Store32(ctx.r3.u32 + 4, prev + ctx.r5.s32);
    ctx.r3.u64 = (uint32_t)prev;
}
// NtSignalAndWaitForSingleObjectEx(signal_handle, wait_handle, alertable, mode?, timeout_ptr)
bool DeliverIoApcs(PPCContext& ctx, uint8_t* base);   // fs.cpp
GUEST_IMPL(NtSignalAndWaitForSingleObjectEx)
{
    if (ctx.r5.u32 && DeliverIoApcs(ctx, base)) { ctx.r3.u64 = 0xC0; return; }   // STATUS_USER_APC
    auto s = GetHandleObject(ctx.r3.u32), w = GetHandleObject(ctx.r4.u32);
    if (!s || !w || !s->waitable || !w->waitable) { ctx.r3.u64 = XSTATUS_INVALID_HANDLE; return; }
    DWORD r = SignalObjectAndWait(s->waitable, w->waitable, GuestTimeoutToMs(ctx.r6.u32), ctx.r5.u32 != 0);
    ctx.r3.u64 = WaitResult(r);
}
// NtSuspendThread(handle, previous_count_ptr) / KeResumeThread(thread_ptr)
GUEST_IMPL(NtSuspendThread)
{
    auto o = GetHandleObject(ctx.r3.u32);
    DWORD prev = o && o->waitable ? SuspendThread(o->waitable) : 0;
    if (ctx.r4.u32) Store32(ctx.r4.u32, prev);
    ctx.r3.u64 = o ? XSTATUS_SUCCESS : XSTATUS_INVALID_HANDLE;
}
GUEST_IMPL(KeResumeThread)
{
    auto o = GetObjectByPointer(ctx.r3.u32);
    ctx.r3.u64 = o && o->waitable && o->type == kObjThread ? ResumeThread(o->waitable) : 0;
}
static thread_local uint8_t t_irqlNg2 = 0;
GUEST_IMPL(KeRaiseIrqlToDpcLevel) { ctx.r3.u64 = t_irqlNg2; t_irqlNg2 = 2; }
GUEST_IMPL(KfLowerIrql) { t_irqlNg2 = (uint8_t)ctx.r3.u32; }

// ---------------- memoria ----------------
static std::mutex g_sizeMutex;
static std::unordered_map<uint32_t, uint32_t> g_poolSizes;
GUEST_IMPL(ExAllocatePoolTypeWithTag)
{
    uint32_t a = GuestAllocVirtual(ctx.r3.u32 ? ctx.r3.u32 : 1, 0x10);
    { std::lock_guard l(g_sizeMutex); g_poolSizes[a] = ctx.r3.u32; }
    ctx.r3.u64 = a;
}
GUEST_IMPL(ExFreePool) { uint32_t a = ctx.r3.u32; if (GuestFree(a)) { std::lock_guard l(g_sizeMutex); g_poolSizes.erase(a); } }
GUEST_IMPL(MmQueryAllocationSize)
{
    std::lock_guard l(g_sizeMutex);
    auto it = g_poolSizes.find(ctx.r3.u32);
    ctx.r3.u64 = it != g_poolSizes.end() ? it->second : 0x10000;
}
// MmCreateKernelStack(size, flags) -> cima de la pila
GUEST_IMPL(MmCreateKernelStack)
{
    uint32_t size = (ctx.r3.u32 + 0xFFF) & ~0xFFFu;
    uint32_t a = GuestAllocVirtual(size ? size : 0x10000, 0x1000);
    ctx.r3.u64 = a ? a + size : 0;
}
GUEST_IMPL(MmDeleteKernelStack) { ctx.r3.u64 = 0; }
// NtQueryVirtualMemory(base, MEMORY_BASIC_INFORMATION* out, ...): todo comprometido y de lectura/escritura
GUEST_IMPL(NtQueryVirtualMemory)
{
    uint32_t b = ctx.r3.u32 & ~0xFFFu, o = ctx.r4.u32;
    Store32(o + 0x00, b); Store32(o + 0x04, b); Store32(o + 0x08, 4);
    Store32(o + 0x0C, 0x10000); Store32(o + 0x10, 0x1000); Store32(o + 0x14, 4); Store32(o + 0x18, 0x20000);
    ctx.r3.u64 = 0;
}
// MmSetAddressProtect(addr, size, protect) / MmQueryAddressProtect(addr): la proteccion se registra por paginas de 4 KB
// (no se aplica de verdad; el juego consulta lo que puso, p. ej. 0x402 = lectura/escritura + combinacion de escritura).
static std::mutex g_protMutex;
static std::unordered_map<uint32_t, uint32_t> g_protPages;
GUEST_IMPL(MmSetAddressProtect)
{
    std::lock_guard l(g_protMutex);
    for (uint32_t a = ctx.r3.u32 & ~0xFFFu; a < ctx.r3.u32 + ctx.r4.u32; a += 0x1000) g_protPages[a] = ctx.r5.u32;
}
uint32_t QueryGuestProtect(uint32_t addr)
{
    std::lock_guard l(g_protMutex);
    auto it = g_protPages.find(addr & ~0xFFFu);
    return it != g_protPages.end() ? it->second : 4;
}

// ---------------- criptografia ----------------
// XeCryptSha(in1, len1, in2, len2, in3, len3, out, out_len): SHA-1 de la concatenacion
GUEST_IMPL(XeCryptSha)
{
    BCRYPT_ALG_HANDLE alg; BCRYPT_HASH_HANDLE h;
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
    BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0);
    const uint32_t ps[3] = { ctx.r3.u32, ctx.r5.u32, ctx.r7.u32 }, ls[3] = { ctx.r4.u32, ctx.r6.u32, ctx.r8.u32 };
    for (int i = 0; i < 3; i++) if (ps[i] && ls[i]) BCryptHashData(h, g_base + ps[i], ls[i], 0);
    uint8_t dig[20]; BCryptFinishHash(h, dig, 20, 0);
    BCryptDestroyHash(h); BCryptCloseAlgorithmProvider(alg, 0);
    uint32_t outLen = ctx.r10.u32 < 20 ? ctx.r10.u32 : 20;
    if (ctx.r9.u32) memcpy(g_base + ctx.r9.u32, dig, outLen);
}

// ---------------- video (como Xenia) ----------------
// VdInitializeScalerCommandBuffer(12 argumentos; los 4 ultimos en la pila: unk8, unk9, dest_ptr, dest_count):
// la GPU ejecuta ese bufer; se rellena con NOPs (tipo 2) y se devuelve dest_count (como Xenia).
// Argumento k >= 8 en la pila: r1 + 0x50 + 8*(k-8), palabra baja (+4).
GUEST_IMPL(VdInitializeScalerCommandBuffer)
{
    uint32_t sp = ctx.r1.u32;
    uint32_t dest = Load32(sp + 0x64), count = Load32(sp + 0x6C);
    static int nlog = 0; if (nlog++ < 3) LOG("[gpu] VdInitializeScalerCommandBuffer: pila %08X %08X %08X %08X -> destino 0x%08X, %u dwords",
        Load32(sp + 0x54), Load32(sp + 0x5C), dest, count, dest, count);
    if (dest && count && count < 0x4000) for (uint32_t i = 0; i < count; i++) Store32(dest + i * 4, 0x80000000u);
    else count = 0;
    ctx.r3.u64 = count;
}
// VdPersistDisplay(unk, out_ptr): *out_ptr = bloque fisico que el juego liberara con MmFreePhysicalMemory
GUEST_IMPL(VdPersistDisplay)
{
    if (ctx.r4.u32) Store32(ctx.r4.u32, GuestAllocPhysical(0x1000, 0x1000));
    ctx.r3.u64 = 1;
}

// ---------------- E/S de dispositivo ----------------
// NtDeviceIoControlFile(handle, event, apc, apc_ctx, io_status, ioctl, in, in_size, out, out_size): el juego pide la geometria y
// el tamano de las particiones de cache (\Device\Harddisk0\Cache0/1). Con el stub (todo a 0) la cache "media" 0 bytes.
// Particion de cache de 2 GB y sectores de 512 bytes, como en la consola.
GUEST_IMPL(NtDeviceIoControlFile)
{
    uint32_t io = ctx.r7.u32, code = ctx.r8.u32;
    uint32_t out = Load32(ctx.r1.u32 + 0x54), outSize = Load32(ctx.r1.u32 + 0x5C);   // argumentos 9 y 10 en la pila
    const uint64_t kSize = 0x80000000ull;
    uint32_t st = 0, info = 0;
    if (code == 0x70000 && out && outSize >= 8) { Store32(out, (uint32_t)(kSize / 512)); Store32(out + 4, 512); info = 8; }
    else if (code == 0x74004 && out && outSize >= 0x10) { Store64(out, 0); Store64(out + 8, kSize); info = 0x10; }
    else { static int n = 0; if (n++ < 10) LOG("[kernel] NtDeviceIoControlFile ioctl 0x%X (salida %08X, %u bytes) no implementado", code, out, outSize); }
    if (io) { Store32(io, st); Store32(io + 4, info); }
    ctx.r3.u64 = st;
}

// Claves de la consola (cabecera de la cache del disco duro): firma siempre valida / firma vacia (como Xenia)
GUEST_IMPL(XeKeysConsoleSignatureVerification) { ctx.r3.u64 = 1; }
GUEST_IMPL(XeKeysConsolePrivateKeySign) { if (ctx.r4.u32) memset(g_base + ctx.r4.u32, 0, 0x228); ctx.r3.u64 = 1; }
