// Punto de entrada del ejecutable PC: prepara la memoria de invitado y salta a _xstart.
#include "runtime.h"
#include <ppc_recomp_shared.h>
#include <windows.h>
#include <vector>
#include <fstream>
#include <string>

uint8_t* g_base = nullptr;
bool InitFileSystem(const char* isoPath, const char* dataDir);
void InitXma();
void RegisterThreadPcr(uint32_t kthread, uint32_t pcr, uint32_t cpu);
extern PPCFuncMapping PPCFuncMappings[];
// Lanzador (launcher.cpp)
struct LauncherConfig { std::string iso, dataDir, dlcDir; };
LauncherConfig LoadConfig();
void SaveConfig(const LauncherConfig& c);
bool EnsureIso(LauncherConfig& c);
void ImportDlc(const LauncherConfig& c);
bool LauncherWanted(int argc, char** argv);   // launcher_ui.cpp (sesion 25)
bool RunLauncherUI();
void UpdaterCleanup();   // updater.cpp (sesion 28)
bool XexpApply(const std::vector<uint8_t>& baseXex, const std::vector<uint8_t>& baseImage, const std::vector<uint8_t>& xexp,
               std::vector<uint8_t>& newHdr, std::vector<uint8_t>& newImage, std::string& err);   // xexp_patch.cpp (sesion 26)
std::string FindTitleUpdate(const std::string& dataDir);   // launcher.cpp (sesion 26)
bool XexToImage(const std::vector<uint8_t>& xex, std::vector<uint8_t>& img);
std::vector<uint8_t> FsReadIsoFile(const char* path);

static std::vector<uint8_t> ReadFile(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

#include <algorithm>
static std::vector<std::pair<uintptr_t, uint32_t>> g_hostToGuest; // (direccion host, funcion invitado) ordenado

static uint32_t GuestFunctionAt(uintptr_t host)
{
    auto it = std::upper_bound(g_hostToGuest.begin(), g_hostToGuest.end(), std::make_pair(host, 0xFFFFFFFFu));
    return it == g_hostToGuest.begin() ? 0 : std::prev(it)->second;
}

static LONG CALLBACK CrashHandler(EXCEPTION_POINTERS* e)
{
    auto* r = e->ExceptionRecord;
    if (r->ExceptionCode == 0x406D1388 || r->ExceptionCode == 0x40010006 || r->ExceptionCode == 0x4001000A || r->ExceptionCode == 0xE06D7363) return EXCEPTION_CONTINUE_SEARCH; // nombres de hilo / OutputDebugString
    LOG("[crash] funcion de invitado aproximada: sub_%08X (hilo %lu)", GuestFunctionAt((uintptr_t)r->ExceptionAddress), GetCurrentThreadId());
    uintptr_t mod = (uintptr_t)GetModuleHandleW(nullptr);
    LOG("[crash] excepcion 0x%08lX en NG2.exe+0x%llX (acceso a 0x%llX, offset invitado 0x%llX)", r->ExceptionCode,
        (unsigned long long)((uintptr_t)r->ExceptionAddress - mod),
        (unsigned long long)(r->NumberParameters > 1 ? r->ExceptionInformation[1] : 0),
        (unsigned long long)(r->NumberParameters > 1 ? r->ExceptionInformation[1] - (uintptr_t)g_base : 0));
    {   // NG2: registros y bytes de la instruccion
        const CONTEXT* k = e->ContextRecord; const uint8_t* ip = (const uint8_t*)k->Rip;
        LOG("[crash] rax=%llX rbx=%llX rcx=%llX rdx=%llX rsi=%llX rdi=%llX rbp=%llX rsp=%llX r8=%llX r9=%llX r10=%llX r11=%llX r12=%llX r13=%llX r14=%llX r15=%llX base=%p modulo=%llX",
            k->Rax, k->Rbx, k->Rcx, k->Rdx, k->Rsi, k->Rdi, k->Rbp, k->Rsp, k->R8, k->R9, k->R10, k->R11, k->R12, k->R13, k->R14, k->R15, (void*)g_base, (unsigned long long)mod);
        LOG("[crash] bytes en rip: %02X %02X %02X %02X %02X %02X %02X %02X", ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7]);
    }
    // Pila host (direcciones 0x14xxxxxxx para llvm-symbolizer --obj=NG2.exe)
    CONTEXT c = *e->ContextRecord;
    char line[1024]; int n = snprintf(line, sizeof(line), "[crash] pila:");
    for (int f = 0; f < 16 && c.Rip; f++)
    {
        n += snprintf(line + n, sizeof(line) - n, " %llX", (unsigned long long)(c.Rip - mod + 0x140000000ull));
        DWORD64 imageBase; auto* fe = RtlLookupFunctionEntry(c.Rip, &imageBase, nullptr);
        if (!fe) { c.Rip = *(DWORD64*)c.Rsp; c.Rsp += 8; continue; }
        void* hd; DWORD64 est;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, c.Rip, fe, &c, &hd, &est, nullptr);
    }
    LOG("%s", line);
    return EXCEPTION_CONTINUE_SEARCH;
}

int RunShaderTest(const char* dir);
// Reserva los 4 GB de invitado con 0xC0000000 como espejo de 0xA0000000 (vistas de una seccion de 512 MB sobre placeholders).
static uint8_t* ReserveGuestMemory()
{
    auto flat = [] { return (uint8_t*)VirtualAlloc(nullptr, PPC_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); };
    if (getenv("NG2_NOMIRROR")) return flat();
    using VA2 = PVOID(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
    using MV3 = PVOID(WINAPI*)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    auto va2 = kb ? (VA2)GetProcAddress(kb, "VirtualAlloc2") : nullptr;
    auto mv3 = kb ? (MV3)GetProcAddress(kb, "MapViewOfFile3") : nullptr;
    if (!va2 || !mv3) { LOG("[mem] sin VirtualAlloc2/MapViewOfFile3: memoria plana (sin espejo 0xC0000000)"); return flat(); }
    HANDLE proc = GetCurrentProcess();
    constexpr SIZE_T kPhys = 0x20000000;
    uint8_t* b = (uint8_t*)va2(proc, nullptr, PPC_MEMORY_SIZE, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
    if (!b) return flat();
    bool ok = VirtualFree(b, 0xA0000000, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)
           && VirtualFree(b + 0xA0000000, kPhys, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)
           && VirtualFree(b + 0xC0000000, kPhys, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
    HANDLE sec = ok ? CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT, 0, (DWORD)kPhys, nullptr) : nullptr;
    ok = ok && sec
        && va2(proc, b, 0xA0000000, MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0)
        && va2(proc, b + 0xE0000000, kPhys, MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0)
        && mv3(sec, proc, b + 0xA0000000, 0, kPhys, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0)
        && mv3(sec, proc, b + 0xC0000000, 0, kPhys, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
    if (!ok) { LOG("[mem] fallo al crear el espejo 0xC0000000 (error %lu): memoria plana", GetLastError()); return flat(); }   // (se pierde la reserva parcial)
    volatile uint8_t* vb = b; vb[0xA0000000] = 0x5A; bool mirrored = vb[0xC0000000] == 0x5A; vb[0xA0000000] = 0;
    LOG("[mem] memoria de invitado con espejo fisico 0xC0000000 -> 0xA0000000 (%s)", mirrored ? "comprobado" : "NO FUNCIONA");
    return b;
}

int main(int argc, char** argv)
{
    if (const char* st = getenv("NG2_SHADERTEST")) return RunShaderTest(st);
    AddVectoredExceptionHandler(1, CrashHandler);
    // Modo desarrollo: NG2.exe basefile.bin default.xex [iso] [datos]. Modo usuario (sin argumentos): ng2.ini + ISO (sesion 23).
    bool devMode = argc > 2;
    std::vector<uint8_t> image, xex;
    std::string isoPath, dataDir;
    if (devMode)
    {
        image = ReadFile(argv[1]);
        xex = ReadFile(argv[2]);
        isoPath = argc > 3 ? argv[3] :
            "G:/ninja gaiden 2/Base/juego/Ninja Gaiden II (World) (En,Ja,Fr,De,Es,It,Zh,Ko,Pl,Ru)/"
            "Ninja Gaiden II (World) (En,Ja,Fr,De,Es,It,Zh,Ko,Pl,Ru).iso";
        dataDir = argc > 4 ? argv[4] : "userdata";
    }
    else
    {
        char exe[MAX_PATH]; GetModuleFileNameA(nullptr, exe, MAX_PATH);
        std::string dir = exe; dir = dir.substr(0, dir.find_last_of("\\/"));
        SetCurrentDirectoryA(dir.c_str());
        if (!getenv("NG2_STDERR")) freopen((dir + "\\ng2.log").c_str(), "w", stderr);
        UpdaterCleanup();   // sesion 28: restos de una actualizacion anterior (*.old)
        if (LauncherWanted(argc, argv) && !RunLauncherUI()) return 0;   // sesion 25: launcher (JUGAR guarda ng2.ini)
        LauncherConfig cfg = LoadConfig();
        if (!EnsureIso(cfg)) return 1;
        isoPath = cfg.iso; dataDir = cfg.dataDir;
        if (!InitFileSystem(isoPath.c_str(), dataDir.c_str()))
        {
            MessageBoxA(nullptr, "No se pudo abrir el ISO. Comprueba que es el de Ninja Gaiden II (Xbox 360).", "Ninja Gaiden II", MB_ICONERROR);
            cfg.iso.clear(); SaveConfig(cfg); return 1;
        }
        ImportDlc(cfg);   // sesion 26: antes de leer el XEX (la actualizacion del titulo se importa desde la carpeta dlc)
        xex = FsReadIsoFile("default.xex");
        if (xex.empty() || !XexToImage(xex, image))
        {
            MessageBoxA(nullptr, "No se pudo leer o descifrar default.xex del ISO.", "Ninja Gaiden II", MB_ICONERROR);
            return 1;
        }
        // Sesion 26: actualizacion del titulo #2 (Mission Mode). Su paquete (LIVE, tipo 000B0000) se importa como el DLC y deja
        // datos\content\000b0000_*\default.xexp. NG2TU.exe la aplica en memoria; NG2.exe le cede el paso si esta instalada.
        std::string tuPath = FindTitleUpdate(cfg.dataDir);
#ifdef NG2_TU
        if (tuPath.empty())
        {
            MessageBoxA(nullptr, "Esta version necesita la actualizacion del titulo #2 de Ninja Gaiden II.\n\n"
                                 "Copia su paquete (archivo TU_... de tu consola) en la carpeta dlc y vuelve a abrir el juego.", "Ninja Gaiden II", MB_ICONINFORMATION);
            return 1;
        }
        {
            std::vector<uint8_t> xp = ReadFile(tuPath.c_str()), nh, ni; std::string err;
            if (!XexpApply(xex, image, xp, nh, ni, err))
            {
                MessageBoxA(nullptr, ("No se pudo aplicar la actualizacion del titulo: " + err).c_str(), "Ninja Gaiden II", MB_ICONERROR);
                return 1;
            }
            xex.swap(nh); image.swap(ni);
        }
#else
        if (!tuPath.empty() && !getenv("NG2_NOTU"))
        {
            std::string tuExe = dir + "\\NG2TU.exe";
            if (GetFileAttributesA(tuExe.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                LOG("[tu] actualizacion del titulo instalada: se abre NG2TU.exe");
                fflush(stderr); freopen("NUL", "w", stderr);   // NG2TU.exe escribe su propio ng2.log
                STARTUPINFOA si{ sizeof(si) }; PROCESS_INFORMATION pi{};
                std::string cmd = "\"" + tuExe + "\" --jugar";
                if (CreateProcessA(tuExe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi))
                { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return 0; }
                freopen((dir + "\\ng2.log").c_str(), "a", stderr);
                LOG("[tu] no se pudo abrir NG2TU.exe (%lu): se sigue sin la actualizacion", GetLastError());
            }
        }
#endif
    }
    // NG2: la imagen descifrada (0x3A78000) es algo menor que la declarada (0x3A80000); el resto queda a cero
    if (image.size() > PPC_IMAGE_SIZE || image.size() + 0x10000 < PPC_IMAGE_SIZE)
    {
        LOG("Imagen del juego invalida (%zu bytes, se esperaban %u): el ISO no es la version para la que se recompilo", image.size(), (unsigned)PPC_IMAGE_SIZE);
        if (!devMode) MessageBoxA(nullptr, "Esta version del juego no coincide con la recompilada (World).", "Ninja Gaiden II", MB_ICONERROR);
        return 1;
    }
    if (xex.empty()) { LOG("No se encontro default.xex"); return 1; }

    // 4 GB de espacio de invitado. Se reserva y compromete de una vez; Windows solo asigna las paginas usadas.
    // Sesion 24: 0xC0000000-0xDFFFFFFF es un espejo de la memoria fisica 0xA0000000-0xBFFFFFFF (como en la consola y en Xenia).
    // D3D copia el microcodigo de los shaders parcheados leyendo por el alias 0xC0000000: sin espejo copiaba ceros
    // (personajes sin cabeza en Edit Rocker). NG2_NOMIRROR=1 vuelve a la memoria plana.
    g_base = ReserveGuestMemory();
    if (!g_base) { LOG("No se pudieron reservar 4 GB de memoria de invitado"); return 1; }
    memcpy(g_base + PPC_IMAGE_BASE, image.data(), image.size());
    InitXexHeader(xex.data(), xex.size());

    // Tabla de llamadas indirectas: guest -> host
    size_t n = 0;
    for (auto* m = PPCFuncMappings; m->host != nullptr; ++m, ++n)
        PPC_LOOKUP_FUNC(g_base, m->guest) = m->host;
    LOG("[runtime] %zu funciones registradas", n);
    void RegisterExtraXamFunctions(); RegisterExtraXamFunctions();   // funciones de XAM pedidas por ordinal (sesion 23)
    for (auto* m = PPCFuncMappings; m->host != nullptr; ++m) g_hostToGuest.emplace_back((uintptr_t)m->host, (uint32_t)m->guest);
    std::sort(g_hostToGuest.begin(), g_hostToGuest.end());

    InitKernelVariables();
    if (devMode && !InitFileSystem(isoPath.c_str(), dataDir.c_str())) return 1;
    InitXma();

    static PPCContext ctx{};
    InitGuestThread(ctx, 0x40000); // tamano de pila por defecto del XEX (hdr 0x00020200)
    RegisterGuestThread(&ctx, kEntryPoint);
    RegisterThreadPcr(t_thread.thread, t_thread.pcr, 0);
    StartDiagnostics();

    LOG("[runtime] saltando a _xstart (0x%08X)", kEntryPoint);
    _xstart(ctx, g_base);
    LOG("[runtime] _xstart ha vuelto (r3=0x%X)", ctx.r3.u32);
    return 0;
}
