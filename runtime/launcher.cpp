// Lanzador (sesion 23): configuracion, seleccion del ISO, descifrado del XEX en memoria e importacion de DLC.
// Asi el ejecutable solo necesita el ISO del usuario: ni basefile.bin ni herramientas externas.
#include "runtime.h"
#include <windows.h>
#include <commdlg.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>

std::vector<uint8_t> FsReadIsoFile(const char* path);

namespace
{
std::string ExeDir()
{
    char p[MAX_PATH]; GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s = p; size_t k = s.find_last_of("\\/");
    return k == std::string::npos ? "." : s.substr(0, k);
}
inline uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
inline uint16_t Be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }

// AES-128 (ECB o CBC con IV cero), descifrado en el sitio, con BCrypt de Windows
bool AesDecrypt(const uint8_t key[16], uint8_t* data, size_t len, bool cbc)
{
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_KEY_HANDLE k = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
    const wchar_t* mode = cbc ? BCRYPT_CHAIN_MODE_CBC : BCRYPT_CHAIN_MODE_ECB;
    BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (PUCHAR)mode, (ULONG)((wcslen(mode) + 1) * 2), 0);
    bool ok = BCryptGenerateSymmetricKey(alg, &k, nullptr, 0, (PUCHAR)key, 16, 0) == 0;
    if (ok)
    {
        uint8_t iv[16] = {}; ULONG out = 0;
        ok = BCryptDecrypt(k, data, (ULONG)len, nullptr, cbc ? iv : nullptr, cbc ? 16 : 0, data, (ULONG)len, &out, 0) == 0;
        BCryptDestroyKey(k);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}
} // namespace

// Imagen base (PE) del XEX2: cifrado AES con la clave retail y compresion basica (bloques de datos + ceros), como tools\xex2.py
bool XexToImage(const std::vector<uint8_t>& xex, std::vector<uint8_t>& img)
{
    static const uint8_t kRetailKey[16] = { 0x20, 0xB1, 0x85, 0xA5, 0x9D, 0x28, 0xFD, 0xC3, 0x40, 0x58, 0x3F, 0xBB, 0x08, 0x96, 0xBF, 0x91 };
    if (xex.size() < 0x18 || memcmp(xex.data(), "XEX2", 4) != 0) return false;
    const uint8_t* x = xex.data();
    uint32_t peOff = Be32(x + 8), secOff = Be32(x + 0x10), nhdr = Be32(x + 0x14);
    uint32_t ffi = 0;
    for (uint32_t i = 0; i < nhdr; i++) if (Be32(x + 0x18 + i * 8) == 0x000003FF) ffi = Be32(x + 0x18 + i * 8 + 4);
    if (!ffi || ffi + 8 > xex.size()) return false;
    uint32_t ffiSize = Be32(x + ffi);
    uint16_t encType = Be16(x + ffi + 4), compType = Be16(x + ffi + 6);
    std::vector<uint8_t> data(xex.begin() + peOff, xex.end());
    if (encType == 1)
    {
        uint8_t session[16]; memcpy(session, x + secOff + 0x150, 16);
        if (!AesDecrypt(kRetailKey, session, 16, false)) return false;
        if (!AesDecrypt(session, data.data(), data.size() & ~size_t(15), true)) return false;
    }
    if (compType == 1)
    {
        img.clear();
        uint32_t nblocks = (ffiSize - 8) / 8; size_t p = 0;
        for (uint32_t i = 0; i < nblocks; i++)
        {
            uint32_t ds = Be32(x + ffi + 8 + i * 8), zs = Be32(x + ffi + 12 + i * 8);
            if (p + ds > data.size()) return false;
            img.insert(img.end(), data.begin() + p, data.begin() + p + ds);
            img.insert(img.end(), zs, 0);
            p += ds;
        }
    }
    else if (compType == 0) img = data;
    else { LOG("[launcher] XEX con compresion %u sin soporte", compType); return false; }
    return true;
}

// ng2.ini junto al exe: iso=..., datos=..., dlc=...
struct LauncherConfig { std::string iso, dataDir, dlcDir; };
static std::string IniPath() { return ExeDir() + "\\ng2.ini"; }
LauncherConfig LoadConfig()
{
    LauncherConfig c; c.dataDir = ExeDir() + "\\userdata"; c.dlcDir = ExeDir() + "\\dlc";
    if (FILE* f = fopen(IniPath().c_str(), "r"))
    {
        char line[1024];
        while (fgets(line, sizeof(line), f))
        {
            std::string s = line; while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            size_t eq = s.find('='); if (eq == std::string::npos || s[0] == ';' || s[0] == '#') continue;
            std::string k = s.substr(0, eq), v = s.substr(eq + 1);
            if (k == "iso") c.iso = v; else if (k == "datos") c.dataDir = v; else if (k == "dlc") c.dlcDir = v;
            // Sesion 14 de NG2: opciones del juego (las de GH no aplican) y variables de diagnostico NG2_*
            else if (k == "perfil") _putenv_s("NG2_SIGNIN", v == "0" ? "0" : "1");
            else if (k == "vibracion") { if (v == "0") _putenv_s("NG2_NOVIBRACION", "1"); }
            else if (k == "anisotropico" && !v.empty()) _putenv_s("NG2_ANISO", v.c_str());   // sesion 23
            else if (k == "mostrar_fps") _putenv_s("NG2_FPS", v == "1" ? "1" : "0");   // sesion 25: contador en pantalla (F3)
            // Sesion 25: opciones del launcher
            else if (k == "saltar_intro") _putenv_s("NG2_SKIPINTRO", v == "1" ? "1" : "0");
            else if (k == "grano_pelicula") _putenv_s("NG2_NOGRAIN", v == "1" ? "0" : "1");
            else if (k == "desenfoque_movimiento") { if (v == "0") _putenv_s("NG2_NOMOTIONBLUR", "1"); }
            else if (k == "vsync") _putenv_s("NG2_VSYNC", v == "0" ? "0" : "1");
            else if (k == "escalado") { if (v == "bilineal") _putenv_s("NG2_UPSCALE", "bilineal"); }
            else if (k == "latencia_audio") _putenv_s("NG2_AUDIOMS", v == "baja" ? "25" : v == "segura" ? "80" : "40");
            else if (k == "pantalla_completa") _putenv_s("NG2_FULLSCREEN", v == "1" ? "1" : "0");
            else if (k == "ventana" && !v.empty()) _putenv_s("NG2_WINSIZE", v.c_str());
            else if (k == "panoramico" && !v.empty()) _putenv_s("NG2_PANO", v.c_str());
            else if (k == "actualizacion") { if (v == "0") _putenv_s("NG2_NOTU", "1"); }   // sesion 26: usar la TU si esta
            else if (k.rfind("NG2_", 0) == 0 && !v.empty()) _putenv_s(k.c_str(), v.c_str());
        }
        fclose(f);
    }
    return c;
}
void SaveConfig(const LauncherConfig& c)
{   // Sesion 25: solo cambia iso/datos/dlc y conserva el resto (comentarios y opciones del launcher)
    std::vector<std::string> lines; bool has[3] = {};
    if (FILE* f = fopen(IniPath().c_str(), "r")) { char b[2048]; while (fgets(b, sizeof(b), f)) { std::string s = b; while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back(); lines.push_back(s); } fclose(f); }
    const char* keys[3] = { "iso", "datos", "dlc" }; const std::string* vals[3] = { &c.iso, &c.dataDir, &c.dlcDir };
    for (auto& l : lines) for (int k = 0; k < 3; k++) if (l.rfind(std::string(keys[k]) + "=", 0) == 0) { l = std::string(keys[k]) + "=" + *vals[k]; has[k] = true; }
    for (int k = 0; k < 3; k++) if (!has[k]) lines.push_back(std::string(keys[k]) + "=" + *vals[k]);
    if (FILE* f = fopen(IniPath().c_str(), "w")) { for (auto& l : lines) fprintf(f, "%s\n", l.c_str()); fclose(f); }
}
// Pide el ISO con el dialogo de Windows si no esta configurado o no existe
bool EnsureIso(LauncherConfig& c)
{
    if (!c.iso.empty() && GetFileAttributesA(c.iso.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    char file[MAX_PATH] = {};
    OPENFILENAMEA ofn{}; ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "ISO de Xbox 360 (*.iso)\0*.iso\0Todos los archivos\0*.*\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Selecciona el ISO de Ninja Gaiden II (Xbox 360)";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return false;
    c.iso = file;
    SaveConfig(c);
    return true;
}

// Importa los paquetes STFS de la carpeta dlc (cualquier nombre) a datos\content, una sola vez por paquete
void ImportDlc(const LauncherConfig& c);

// ---- importacion de DLC (STFS) ----
namespace
{
struct StfsPkg
{
    HANDLE f = INVALID_HANDLE_VALUE; uint32_t headerSize = 0, contentType = 0, ftBlocks = 0, ftStart = 0, sex = 0; uint64_t firstHash = 0;
    uint32_t step0 = 0; std::wstring displayName;
    bool Read(uint64_t off, void* dst, uint32_t len) { OVERLAPPED ov{}; ov.Offset = (DWORD)off; ov.OffsetHigh = (DWORD)(off >> 32); DWORD got = 0; return ReadFile(f, dst, len, &got, &ov) && got == len; }
    uint64_t Backing(uint32_t n)
    {
        uint64_t r = (uint64_t((n + 0xAA) / 0xAA) << sex) + n;
        if (n < 0xAA) return r;
        if (n < 0x70E4) return r + (uint64_t((n + 0x70E4) / 0x70E4) << sex);
        return (1ull << sex) + r + (uint64_t((n + 0x70E4) / 0x70E4) << sex);
    }
    uint64_t BlockOffset(uint32_t n) { return (Backing(n) << 12) + firstHash; }
    uint32_t Level0Hash(uint32_t n)
    {
        if (n < 0xAA) return 0;
        uint32_t num = (n / 0xAA) * step0 + (((n / 0x70E4) + 1) << sex);
        return n / 0x70E4 == 0 ? num : num + (1u << sex);
    }
    uint32_t Next(uint32_t n)
    {
        uint8_t e[0x18]; Read((uint64_t(Level0Hash(n)) << 12) + firstHash + (n % 0xAA) * 0x18, e, 0x18);
        return (uint32_t(e[0x15]) << 16) | (uint32_t(e[0x16]) << 8) | e[0x17];
    }
};
bool ExtractStfs(const std::string& pkgPath, const std::string& contentDir)
{
    StfsPkg s; s.f = CreateFileA(pkgPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (s.f == INVALID_HANDLE_VALUE) return false;
    std::vector<uint8_t> h(0xA000);
    bool ok = s.Read(0, h.data(), (uint32_t)h.size()) && (!memcmp(h.data(), "LIVE", 4) || !memcmp(h.data(), "PIRS", 4) || !memcmp(h.data(), "CON ", 4));
    if (!ok) { CloseHandle(s.f); return false; }
    s.headerSize = Be32(&h[0x340]); s.contentType = Be32(&h[0x344]);
    uint32_t vd = 0x379; uint8_t blockSep = h[vd + 2];
    s.ftBlocks = h[vd + 3] | (h[vd + 4] << 8); s.ftStart = h[vd + 5] | (h[vd + 6] << 8) | (h[vd + 7] << 16);
    s.sex = (~blockSep) & 1; s.firstHash = (s.headerSize + 0xFFF) & 0xFFFFF000ull; s.step0 = s.sex == 0 ? 0xAB : 0xAC;
    for (int i = 0; i < 128; i++) { wchar_t c = (wchar_t)((h[0x411 + i * 2] << 8) | h[0x412 + i * 2]); if (!c) break; s.displayName += c; }
    std::string fname = pkgPath.substr(pkgPath.find_last_of("\\/") + 1); if (fname.size() > 42) fname.resize(42);
    char folderBuf[96]; snprintf(folderBuf, sizeof(folderBuf), "%08x_%s", s.contentType, fname.c_str());
    std::string folder = folderBuf; for (auto& c : folder) c = (char)tolower((unsigned char)c);
    std::string xcd = contentDir + "\\" + folder + ".xcd";
    if (GetFileAttributesA(xcd.c_str()) != INVALID_FILE_ATTRIBUTES) { CloseHandle(s.f); return true; }   // ya importado
    LOG("[launcher] importando DLC %s (tipo 0x%X)...", fname.c_str(), s.contentType);
    CreateDirectoryA(contentDir.c_str(), nullptr);
    std::string out = contentDir + "\\" + folder; CreateDirectoryA(out.c_str(), nullptr);
    // Tabla de archivos (bloques consecutivos) y extraccion
    std::vector<uint8_t> table(size_t(s.ftBlocks) * 0x1000);
    for (uint32_t i = 0; i < s.ftBlocks; i++) s.Read(s.BlockOffset(s.ftStart + i), &table[size_t(i) * 0x1000], 0x1000);
    struct Ent { std::string name; bool dir, consec; uint32_t blocks, start, size; int16_t parent; };
    std::vector<Ent> ents;
    for (size_t o = 0; o + 0x40 <= table.size(); o += 0x40)
    {
        const uint8_t* e = &table[o]; uint8_t fl = e[0x28]; uint32_t nl = fl & 0x3F;
        if (!nl) continue;
        ents.push_back({ std::string((const char*)e, nl), (fl & 0x80) != 0, (fl & 0x40) != 0,
            uint32_t(e[0x29] | (e[0x2A] << 8) | (e[0x2B] << 16)), uint32_t(e[0x2F] | (e[0x30] << 8) | (e[0x31] << 16)),
            Be32(e + 0x34), (int16_t)Be16(e + 0x32) });
    }
    std::vector<std::string> paths(ents.size());
    for (size_t i = 0; i < ents.size(); i++)
    {
        std::string p = ents[i].name; int16_t par = ents[i].parent; int guard = 0;
        while (par >= 0 && par < (int)ents.size() && guard++ < 32) { p = ents[par].name + "\\" + p; par = ents[par].parent; }
        paths[i] = out + "\\" + p;
    }
    for (size_t i = 0; i < ents.size(); i++) if (ents[i].dir) CreateDirectoryA(paths[i].c_str(), nullptr);
    std::vector<uint8_t> blk(0x1000);
    for (size_t i = 0; i < ents.size(); i++)
    {
        if (ents[i].dir) continue;
        FILE* of = fopen(paths[i].c_str(), "wb"); if (!of) continue;
        uint32_t n = ents[i].start, left = ents[i].size;
        for (uint32_t b = 0; b < ents[i].blocks && left; b++)
        {
            s.Read(s.BlockOffset(n), blk.data(), 0x1000);
            uint32_t w = left < 0x1000 ? left : 0x1000; fwrite(blk.data(), 1, w, of); left -= w;
            n = ents[i].consec ? n + 1 : s.Next(n);
        }
        fclose(of);
    }
    // XCONTENT_DATA (0x134): device 1, tipo, nombre UTF-16BE, archivo
    std::vector<uint8_t> x(0x134);
    x[3] = 1; x[4] = uint8_t(s.contentType >> 24); x[5] = uint8_t(s.contentType >> 16); x[6] = uint8_t(s.contentType >> 8); x[7] = uint8_t(s.contentType);
    for (size_t i = 0; i < s.displayName.size() && i < 127; i++) { x[8 + i * 2] = uint8_t(s.displayName[i] >> 8); x[9 + i * 2] = uint8_t(s.displayName[i]); }
    memcpy(&x[0x108], fname.c_str(), fname.size());
    if (FILE* xf = fopen(xcd.c_str(), "wb")) { fwrite(x.data(), 1, x.size(), xf); fclose(xf); }
    CloseHandle(s.f);
    LOG("[launcher] DLC importado: %zu archivos", ents.size());
    return true;
}
void ScanDlcDir(const std::string& dir, const std::string& contentDir, int depth)
{
    if (depth > 6) return;
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do
    {
        std::string n = fd.cFileName; if (n == "." || n == "..") continue;
        std::string p = dir + "\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ScanDlcDir(p, contentDir, depth + 1);
        else if (fd.nFileSizeLow > 0xA000 || fd.nFileSizeHigh) ExtractStfs(p, contentDir);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
} // namespace
// Sesion 27: ademas de la carpeta dlc= del ini se mira la carpeta dlc junto al ejecutable y la carpeta "Update" hermana de la
// de dlc= (p. ej. Base\DLC y Base\Update). Con dlc= en otra carpeta, la actualizacion copiada en dlc\ no se importaba y el
// juego arrancaba sin Mission Mode.
void ImportDlc(const LauncherConfig& c)
{
    std::vector<std::string> dirs{ c.dlcDir, ExeDir() + "\\dlc" };
    std::string d = c.dlcDir; while (!d.empty() && (d.back() == '\\' || d.back() == '/')) d.pop_back();
    size_t sl = d.find_last_of("\\/"); if (sl != std::string::npos) dirs.push_back(d.substr(0, sl) + "\\Update");
    std::vector<std::string> done;
    for (auto& dir : dirs)
    {
        std::string k = dir; for (auto& ch : k) ch = (char)tolower((unsigned char)ch);
        while (!k.empty() && (k.back() == '\\' || k.back() == '/')) k.pop_back();
        bool seen = false; for (auto& x : done) if (x == k) seen = true;
        if (k.empty() || seen) continue;
        done.push_back(k);
        if (GetFileAttributesA(dir.c_str()) != INVALID_FILE_ATTRIBUTES) ScanDlcDir(dir, c.dataDir + "\\content", 0);
    }
}

// Sesion 26: ruta de default.xexp de la actualizacion del titulo importada (datos\content\000b0000_*\default.xexp), o vacia
std::string FindTitleUpdate(const std::string& dataDir)
{
    std::string content = dataDir + "\\content";
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA((content + "\\000b0000_*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return "";
    std::string found;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        std::string x = content + "\\" + fd.cFileName + "\\default.xexp";
        if (GetFileAttributesA(x.c_str()) != INVALID_FILE_ATTRIBUTES) { found = x; break; }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return found;
}
