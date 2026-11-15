// XAM minimo (sesion 6): memoria, perfil, mandos, audio, enumeraciones, mensajes y red.
// Comportamiento segun Xenia (BSD). Mandos: de momento el mando 1 conectado y en reposo (la entrada real llega en la sesion de entrada).
#include "objects.h"
#include <ppc_recomp_shared.h>
#include <cstring>
#include <mutex>
#include <vector>
#include <atomic>
#include <thread>
#include <mmsystem.h>
#include <Xinput.h>
bool PsPadGetState(uint16_t& buttons, uint8_t& lt, uint8_t& rt, int16_t sticks[4]);   // dualsense.cpp
#include <algorithm>
#include <cmath>

namespace
{
constexpr uint32_t X_ERROR_SUCCESS = 0, X_ERROR_IO_PENDING = 0x3E5, X_ERROR_NO_MORE_FILES = 0x12,
    X_ERROR_DEVICE_NOT_CONNECTED = 0x48F, X_ERROR_NO_SUCH_USER = 0x525, X_ERROR_EMPTY = 0x10D2;
constexpr uint64_t kXuid = 0xB13EBABEBABEBABEull;

// X_OVERLAPPED: +0 result, +4 length, +8 context, +0xC evento, +0x10 rutina, +0x14 contexto, +0x18 error extendido
void CompleteOverlapped(uint32_t ov, uint32_t result, uint32_t length = 0)
{
    if (!ov) return;
    Store32(ov + 0x00, result);
    Store32(ov + 0x04, length);
    Store32(ov + 0x18, result);
    uint32_t ev = Load32(ov + 0x0C);
    if (ev) if (auto o = GetHandleObject(ev)) SetEvent(o->waitable);
}
}

// ---- perfil: ajustes y privilegios (sesion 12) ----
// XamUserReadProfileSettings(title_id, user_index, xuid_count, xuids, setting_count, setting_ids, buffer_size_ptr, buffer_ptr, [pila] overlapped)
// Resultado: {u32 setting_count, u32 settings_ptr} + setting_count x X_USER_PROFILE_SETTING (0x28 bytes):
//   +0x00 from (0 = no establecido -> el juego usa su valor por defecto), +0x08 user_index/xuid, +0x10 setting_id, +0x18 X_USER_DATA (tipo + valor)
GUEST_IMPL(XamUserReadProfileSettings)
{
    uint32_t userIndex = ctx.r4.u32, xuidCount = ctx.r5.u32, settingCount = ctx.r7.u32, settingIds = ctx.r8.u32;
    uint32_t sizePtr = ctx.r9.u32, buf = ctx.r10.u32, overlapped = Load32(ctx.r1.u32 + 0x54);
    uint32_t entries = settingCount * (xuidCount ? xuidCount : 1);
    uint32_t need = 8 + entries * 0x28;
    uint32_t have = sizePtr ? Load32(sizePtr) : 0;
    if (!buf || have < need)
    {
        if (sizePtr) Store32(sizePtr, need);
        ctx.r3.u64 = 0x7A; // ERROR_INSUFFICIENT_BUFFER
        return;
    }
    memset(g_base + buf, 0, need);
    Store32(buf, entries); Store32(buf + 4, buf + 8);
    for (uint32_t i = 0; i < entries; i++)
    {
        uint32_t e = buf + 8 + i * 0x28;
        Store32(e + 0x00, 0);                                  // from: no establecido
        Store32(e + 0x08, userIndex);
        uint32_t sid = settingIds ? Load32(settingIds + (i % settingCount) * 4) : 0;
        Store32(e + 0x10, sid);
        g_base[e + 0x18] = (uint8_t)((sid >> 28) & 0xF);       // X_USER_DATA.type = tipo codificado en el ID (valor 0 / binario vacio)
        static int logged = 0;
        if (logged < 16) { logged++; LOG("[xam] ReadProfileSettings: setting 0x%08X (tipo %u, tamano %u)", sid, (sid >> 28) & 0xF, (sid >> 16) & 0xFFF); }
    }
    if (overlapped) { CompleteOverlapped(overlapped, X_ERROR_SUCCESS); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = X_ERROR_SUCCESS;
}
// ---- notificaciones del sistema (sesion 12) ----
// XamNotifyCreateListener(mask, max_version) -> handle (0 = fallo). De momento sin notificaciones pendientes.
// Listener con cola de notificaciones (como Xenia: el primer listener con mascara de sistema recibe las de arranque)
struct NotifyListener : KObject
{
    uint64_t mask = 0;
    std::mutex m;
    std::vector<std::pair<uint32_t, uint32_t>> q;
    void Enqueue(uint32_t id, uint32_t data)
    {
        uint32_t maskIndex = (id >> 25) & 0x3F;
        if (!(mask & (1ull << maskIndex))) return;
        std::lock_guard l(m); q.emplace_back(id, data); SetEvent(waitable);
    }
};
std::mutex g_listenersMutex;
std::vector<std::weak_ptr<NotifyListener>> g_listeners;
static void BroadcastNotification(uint32_t id, uint32_t data)
{
    std::lock_guard l(g_listenersMutex);
    for (auto& w : g_listeners) if (auto o = w.lock()) o->Enqueue(id, data);
}
GUEST_IMPL(XamNotifyCreateListener)
{
    static bool startupDone = false;
    auto o = std::make_shared<NotifyListener>();
    o->type = 0; o->waitable = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    o->mask = ctx.r3.u64;
    static bool everyListener = getenv("NG2_NOTIFY_FIRSTONLY") == nullptr; // por defecto: a cada listener de sistema
    static bool noStartup = getenv("NG2_NOSTARTNOTIFY") != nullptr;   // NG2: diagnostico
    if (!noStartup && (!startupDone || everyListener) && (o->mask & 1))
    {
        startupDone = true;
        o->Enqueue(0x09, 1); o->Enqueue(0x09, 0);   // XN_SYS_UI on/off
        o->Enqueue(0x0A, 1); o->Enqueue(0x0A, 1);   // XN_SYS_SIGNINCHANGED (usuario 0)
        o->Enqueue(0x12, 0); o->Enqueue(0x12, 0);   // XN_SYS_INPUTDEVICESCHANGED
        o->Enqueue(0x13, 0); o->Enqueue(0x13, 0);   // XN_SYS_INPUTDEVICECONFIGCHANGED
    }
    // Sesion 23: avisos de almacenamiento y de contenido instalado, para que el juego busque el DLC (tipo 2)
    static bool noDlcNotify = getenv("NG2_NODLCNOTIFY") != nullptr;
    if (!noDlcNotify)
    {
        if (o->mask & 1) o->Enqueue(0x0B, 0);              // XN_SYS_STORAGEDEVICESCHANGED
        if (o->mask & 2) o->Enqueue(0x02000007, 0);        // XN_LIVE_CONTENT_INSTALLED
    }
    uint32_t h = InsertHandle(o);
    { std::lock_guard l(g_listenersMutex); g_listeners.push_back(o); }
    LOG("[xam] XamNotifyCreateListener(mask 0x%llX) -> 0x%08X", (unsigned long long)ctx.r3.u64, h);
    ctx.r3.u64 = h;
}
// XNotifyGetNext(handle, match_id, id_ptr, param_ptr) -> TRUE si habia notificacion
GUEST_IMPL(XNotifyGetNext)
{
    uint32_t matchId = ctx.r4.u32, idPtr = ctx.r5.u32, paramPtr = ctx.r6.u32;
    if (paramPtr) Store32(paramPtr, 0);
    if (!idPtr) { ctx.r3.u64 = 0; return; }
    Store32(idPtr, 0);
    auto l = std::dynamic_pointer_cast<NotifyListener>(GetHandleObject(ctx.r3.u32));
    static int calls = 0;
    if (calls++ < 6) LOG("[xam] XNotifyGetNext(handle 0x%08X, match 0x%X) listener=%d cola=%zu", ctx.r3.u32, matchId, l ? 1 : 0, l ? l->q.size() : 0);
    if (!l) { ctx.r3.u64 = 0; return; }
    std::lock_guard lk(l->m);
    for (size_t i = 0; i < l->q.size(); i++)
        if (!matchId || l->q[i].first == matchId)
        {
            Store32(idPtr, l->q[i].first);
            if (paramPtr) Store32(paramPtr, l->q[i].second);
            LOG("[xam] XNotifyGetNext -> 0x%X (param %u)", l->q[i].first, l->q[i].second);
            l->q.erase(l->q.begin() + i);
            if (l->q.empty()) ResetEvent(l->waitable);
            ctx.r3.u64 = 1;
            return;
        }
    ctx.r3.u64 = 0;
}
GUEST_IMPL(XNotifyPositionUI) { ctx.r3.u64 = 0; }
// ---- contenido (partidas/DLC) - sesion 13: enumerador vacio por ahora (el DLC llega en su sesion) ----
// XamContentCreateEnumerator(user_index, device_id, content_type, content_flags, items_per_enumerate, buffer_size_ptr, handle_out)
// Sesion 16: enumera las partidas de userdata\content (un <carpeta>.xcd con el XCONTENT_DATA de 0x134 bytes por cada una).
std::string FsContentDir();
struct ContentEnum : KObject { std::vector<std::vector<uint8_t>> items; size_t next = 0; uint32_t perCall = 1; uint32_t itemSize = 0x134;
    ~ContentEnum() override { if (guestPtr) GuestFree(guestPtr); } };   // sesion 23: devuelve su bloque al cerrarse
int FsMountContent(const std::string& root, const std::string& folder, bool create);   // fs.cpp
// Lista de XCONTENT_DATA (0x134 bytes) de un tipo de contenido en userdata\content
static std::vector<std::vector<uint8_t>> ListContent(uint32_t type)
{
    std::vector<std::vector<uint8_t>> out;
    char pattern[512]; snprintf(pattern, sizeof(pattern), "%s\\%08x_*.xcd", FsContentDir().c_str(), type);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do
    {
        std::vector<uint8_t> d(0x134);
        std::string path = FsContentDir() + "\\" + fd.cFileName;
        if (FILE* f = fopen(path.c_str(), "rb")) { fread(d.data(), 1, d.size(), f); fclose(f); out.push_back(d); }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return out;
}
// Enumeracion por mensajes de XAM del juego (sub_826C5BD0: handle, buffer, tamano 312, items_ptr, overlapped):
// se sustituye por una lectura directa del enumerador (el original pide los datos a la aplicacion de contenido por XMsg).
#if 0   // NG2: direccion de GH; buscar la funcion equivalente de la libreria XDK de NG2 (sesion 8, DLC)
PPC_FUNC_IMPL(__imp__sub_826C5BD0);
PPC_FUNC(sub_826C5BD0)
{
    auto e = std::dynamic_pointer_cast<ContentEnum>(GetHandleObject(ctx.r3.u32));
    if (!e) { __imp__sub_826C5BD0(ctx, base); return; }
    uint32_t buf = ctx.r4.u32, size = ctx.r5.u32, itemsPtr = ctx.r6.u32, ov = ctx.r7.u32;
    uint32_t n = 0;
    while (e->next < e->items.size() && (n + 1) * e->itemSize <= size && n < e->perCall)
    {
        memcpy(g_base + buf + n * e->itemSize, e->items[e->next].data(), e->itemSize);
        e->next++; n++;
    }
    if (itemsPtr) Store32(itemsPtr, n);
    uint32_t result = n ? X_ERROR_SUCCESS : X_ERROR_NO_MORE_FILES;
    LOG("[xam] enumeracion de contenido (aggregate): %u elementos (%zu/%zu)", n, e->next, e->items.size());
    if (ov) { CompleteOverlapped(ov, result, n); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = result;
}
#endif
// XamContentAggregateCreateEnumerator (ordinal 633, sesion 23; el juego la obtiene con XexGetProcedureAddress para buscar el DLC):
// (xuid, device_id, content_type, flags, handle_out). Elementos XCONTENT_AGGREGATE_DATA = XCONTENT_DATA + title_id (0x138 bytes).
PPC_FUNC(ng2_XamContentAggregateCreateEnumerator)
{
    uint32_t device = ctx.r4.u32, type = ctx.r5.u32, handleOut = ctx.r7.u32;
    auto o = std::make_shared<ContentEnum>();
    o->guestPtr = GuestAllocKernel(0x40, 0x10);
    o->itemSize = 0x138; o->perCall = 1;
    if (device == 0 || device == 1)
        for (auto& d : ListContent(type))
        {
            // El juego monta cada paquete como CONT<n>: por mensajes de XAM (no por XamContentCreate): se monta aqui en el mismo orden
            std::string file; for (int i = 0; i < 42 && d[0x108 + i]; i++) file += (char)d[0x108 + i];
            char folder[96]; snprintf(folder, sizeof(folder), "%08x_%s", type, file.c_str());
            std::string fl = folder; for (auto& c : fl) c = (char)tolower((unsigned char)c);
            char root[16]; snprintf(root, sizeof(root), "CONT%zu", o->items.size());
            FsMountContent(root, fl, false);
            d.resize(0x138);
            d[0x134] = 0x54; d[0x135] = 0x43; d[0x136] = 0x07; d[0x137] = 0xD5;   // title_id 544307D5 (BE)
            o->items.push_back(d);
        }
    if (handleOut) Store32(handleOut, InsertHandle(o));
    LOG("[xam] XamContentAggregateCreateEnumerator(dispositivo %u, tipo 0x%X) -> %zu elementos", device, type, o->items.size());
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XamUserCreateAchievementEnumerator(title_id, user, xuid, detail_flags, start, count, buffer_size_ptr, handle_ptr) (NG2):
// enumerador vacio. Con el stub el juego leia un tamano de bufer sin inicializar y pedia ~1 GB (bad_alloc).
GUEST_IMPL(XamUserCreateAchievementEnumerator)
{
    uint32_t count = ctx.r8.u32 ? ctx.r8.u32 : 1;
    if (ctx.r9.u32) Store32(ctx.r9.u32, count * (0x24 + 0x200));   // X_XACHIEVEMENT_DETAILS + cadenas
    auto o = std::make_shared<ContentEnum>();
    o->itemSize = 0x24;   // sesion 23: sin objeto en memoria de invitado (el juego lo pide ~1 vez por segundo y llenaba el monton)
    if (ctx.r10.u32) Store32(ctx.r10.u32, InsertHandle(o));
    { static int nlog = 0; if (nlog++ < 20) LOG("[xam] XamUserCreateAchievementEnumerator(titulo %08X, usuario %u, %u logros) -> vacio", ctx.r3.u32, ctx.r4.u32, ctx.r8.u32); }
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
GUEST_IMPL(XamContentCreateEnumerator)
{
    uint32_t device = ctx.r4.u32, type = ctx.r5.u32;
    if (ctx.r8.u32) Store32(ctx.r8.u32, 0x134 * ctx.r7.u32);   // sizeof(XCONTENT_DATA) * items
    auto o = std::make_shared<ContentEnum>();
    o->guestPtr = GuestAllocKernel(0x40, 0x10);
    o->perCall = ctx.r7.u32 ? ctx.r7.u32 : 1;
    if (device == 0 || device == 1)
    {
        char pattern[512]; snprintf(pattern, sizeof(pattern), "%s\\%08x_*.xcd", FsContentDir().c_str(), type);
        WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                std::vector<uint8_t> d(0x134);
                std::string path = FsContentDir() + "\\" + fd.cFileName;
                if (FILE* f = fopen(path.c_str(), "rb")) { fread(d.data(), 1, d.size(), f); fclose(f); o->items.push_back(d); }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
    if (ctx.r9.u32) Store32(ctx.r9.u32, InsertHandle(o));
    LOG("[xam] XamContentCreateEnumerator(usuario %u, dispositivo %u, tipo 0x%X) -> %zu elementos", ctx.r3.u32, device, type, o->items.size());
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
GUEST_IMPL(XNotifyDelayUI) { ctx.r3.u64 = 0; }
// XamGetExecutionId(info_ptr): puntero a la cabecera XEX_HEADER_EXECUTION_INFO (0x40006), como Xenia. Antes era un stub que no
// escribia *info_ptr: el juego leia basura de la pila y, con 7 o mas DLC, se quedaba en "Checking for downloadable content".
GUEST_IMPL(XamGetExecutionId)
{
    uint32_t info = XexHeaderField(0x40006);
    if (ctx.r3.u32 && info) Store32(ctx.r3.u32, info);
    static bool once = false;
    if (!once) { once = true; LOG("[xam] XamGetExecutionId -> %08X", info); }
    ctx.r3.u64 = info ? X_ERROR_SUCCESS : 0xC0000225;   // STATUS_NOT_FOUND
}

// XamContentGetDeviceData(device_id, device_data_ptr). X_CONTENT_DEVICE_DATA (0x50 bytes, como Xenia):
// +0 device_id, +4 device_type (1 = disco duro), +8 total_bytes (u64), +0x10 free_bytes (u64), +0x18 nombre (UTF-16 BE, 28 caracteres)
GUEST_IMPL(XamContentGetDeviceData)
{
    uint32_t id = ctx.r3.u32, p = ctx.r4.u32;
    if (id != 1) { ctx.r3.u64 = X_ERROR_DEVICE_NOT_CONNECTED; return; }
    if (p)
    {
        memset(g_base + p, 0, 0x50);
        Store32(p + 0, 1); Store32(p + 4, 1);
        Store64(p + 8, 20ull << 30); Store64(p + 0x10, 16ull << 30);   // 20 GB, 16 GB libres
        const char* name = "Hard Drive";
        for (int i = 0; name[i]; i++) Store16(p + 0x18 + i * 2, (uint16_t)name[i]);
    }
    ctx.r3.u64 = X_ERROR_SUCCESS;
}

// ---- partidas guardadas (sesion 16): cada contenido es una carpeta en userdata\content ----
int FsMountContent(const std::string& root, const std::string& folder, bool create);
void FsUnmountContent(const std::string& root);
static std::string GuestStr(uint32_t p, size_t max = 64)
{
    std::string s; if (!p) return s;
    for (size_t i = 0; i < max && g_base[p + i]; i++) s += (char)g_base[p + i];
    return s;
}
// XamContentCreateEx(user_index, root_name, content_data, flags, disposition_ptr, license_mask_ptr, cache_size, content_size, [pila] overlapped)
// XCONTENT_DATA: +0 device_id, +4 content_type, +8 display_name (UTF-16, 128), +0x108 file_name (char[42])
static void ContentCreate(PPCContext& ctx, uint32_t overlapped)
{
    std::string root = GuestStr(ctx.r4.u32), file = GuestStr(ctx.r5.u32 + 0x108, 42);
    uint32_t type = Load32(ctx.r5.u32 + 4), flags = ctx.r6.u32, dispPtr = ctx.r7.u32, licPtr = ctx.r8.u32;
    char folderBuf[96]; snprintf(folderBuf, sizeof(folderBuf), "%08x_%s", type, file.c_str());
    std::string folder = folderBuf;
    for (auto& c : folder) c = (char)tolower((unsigned char)c);
    // flags: 1 CREATE_NEW, 2 CREATE_ALWAYS, 3 OPEN_EXISTING, 4 OPEN_ALWAYS, 5 TRUNCATE_EXISTING
    uint32_t mode = flags & 0xF;
    bool create = mode == 1 || mode == 2 || mode == 4;
    int r = FsMountContent(root, folder, create);
    uint32_t result = X_ERROR_SUCCESS;
    if (r < 0) result = 3;                         // X_ERROR_PATH_NOT_FOUND
    else if (r == 1 && mode == 1) result = 0xB7;   // X_ERROR_ALREADY_EXISTS
    if (r >= 0)
    {
        // Guardar el XCONTENT_DATA junto a la carpeta para poder enumerarlo despues (<carpeta>.xcd)
        std::string meta = FsContentDir() + "\\" + folder + ".xcd";
        if (FILE* f = fopen(meta.c_str(), "wb")) { Store32(ctx.r5.u32, 1); fwrite(g_base + ctx.r5.u32, 1, 0x134, f); fclose(f); }
    }
    if (dispPtr) Store32(dispPtr, r == 0 ? 1 : 2); // 1 creado, 2 abierto
    if (licPtr) Store32(licPtr, type == 2 ? 0xFFFFFFFF : 0);   // DLC (Marketplace): con licencia completa
    LOG("[xam] XamContentCreate(raiz '%s', archivo '%s', tipo 0x%X, modo %u) -> 0x%X", root.c_str(), file.c_str(), type, mode, result);
    if (overlapped) { CompleteOverlapped(overlapped, result); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = result;
}
GUEST_IMPL(XamContentCreateEx) { ContentCreate(ctx, Load32(ctx.r1.u32 + 0x54)); }
// XamContentCreateInternal (ordinal 625, por XexGetProcedureAddress): (root_name, content_data, flags, disposition_ptr,
// license_mask_ptr, cache_size, content_size, overlapped) = XamContentCreateEx sin user_index (sesion 24, como Xenia)
PPC_FUNC(ng2_XamContentCreateInternal)
{
    PPCContext c = ctx;
    c.r4 = ctx.r3; c.r5 = ctx.r4; c.r6 = ctx.r5; c.r7 = ctx.r6; c.r8 = ctx.r7;
    ContentCreate(c, ctx.r10.u32);
    ctx.r3 = c.r3;
}
// XamContentGetCreator(user_index, content_data, is_creator_ptr, creator_xuid_ptr, overlapped): las partidas son siempre del usuario 0
GUEST_IMPL(XamContentGetCreator)
{
    if (ctx.r5.u32) Store32(ctx.r5.u32, 1);
    if (ctx.r6.u32) Store64(ctx.r6.u32, kXuid);
    if (ctx.r7.u32) { CompleteOverlapped(ctx.r7.u32, X_ERROR_SUCCESS); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XamContentDelete(user_index, content_data, overlapped): borra la carpeta de la partida en userdata\content
GUEST_IMPL(XamContentDelete)
{
    std::string file = GuestStr(ctx.r4.u32 + 0x108, 42);
    char folderBuf[96]; snprintf(folderBuf, sizeof(folderBuf), "%08x_%s", Load32(ctx.r4.u32 + 4), file.c_str());
    std::string folder = folderBuf;
    for (auto& c : folder) c = (char)tolower((unsigned char)c);
    uint32_t result = X_ERROR_SUCCESS;
    if (file.empty() || folder.find("..") != std::string::npos || folder.find_first_of("\\/:") != std::string::npos) result = 3;
    else
    {
        std::string dir = FsContentDir() + "\\" + folder;
        WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) DeleteFileA((dir + "\\" + fd.cFileName).c_str());
            while (FindNextFileA(h, &fd));
            FindClose(h);
        }
        RemoveDirectoryA(dir.c_str());
        DeleteFileA((dir + ".xcd").c_str());
    }
    LOG("[xam] XamContentDelete('%s') -> 0x%X", folder.c_str(), result);
    if (ctx.r5.u32) { CompleteOverlapped(ctx.r5.u32, result); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = result;
}
// XamContentClose(root_name, overlapped)
GUEST_IMPL(XamContentClose)
{
    FsUnmountContent(GuestStr(ctx.r3.u32));
    if (ctx.r4.u32) { CompleteOverlapped(ctx.r4.u32, X_ERROR_SUCCESS); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XamContentSetThumbnail(root_name, buffer, size, overlapped)
GUEST_IMPL(XamContentSetThumbnail)
{
    if (ctx.r6.u32) { CompleteOverlapped(ctx.r6.u32, X_ERROR_SUCCESS); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = X_ERROR_SUCCESS;
}

// ---- interfaces del sistema sin interfaz (sesion 23): se completan al instante para que el juego no se quede esperando ----
static void FinishUi(PPCContext& ctx, uint32_t ov, uint32_t result)
{
    // NG2 (sesion 6): por defecto NO se envian (al "cerrarse" el dialogo el juego volvia a preguntar sin fin). NG2_UINOTIFY=1 = antes.
    static const bool noUiNotify = getenv("NG2_UINOTIFY") == nullptr;
    if (!noUiNotify) { BroadcastNotification(0x09, 1); BroadcastNotification(0x09, 0); }   // XN_SYS_UI on/off
    if (ov) { CompleteOverlapped(ov, result); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = result;
}
// XamShowKeyboardUI(user, flags, default_text, title, description, buffer, buffer_length, overlapped): devuelve el texto por defecto
GUEST_IMPL(XamShowKeyboardUI)
{
    uint32_t def = ctx.r5.u32, buf = ctx.r8.u32, len = ctx.r9.u32, ov = ctx.r10.u32;
    if (buf && len)
    {
        uint32_t i = 0;
        if (def) for (; i + 1 < len; i++) { uint16_t c = __builtin_bswap16(*(uint16_t*)(g_base + def + i * 2)); if (!c) break; Store16(buf + i * 2, c); }
        else { const char* d = "Rocker"; for (; d[i] && i + 1 < len; i++) Store16(buf + i * 2, (uint16_t)d[i]); }
        Store16(buf + i * 2, 0);
    }
    LOG("[xam] XamShowKeyboardUI -> texto por defecto");
    FinishUi(ctx, ov, X_ERROR_SUCCESS);
}
// XamShowMessageBoxUI(user, title, text, button_count, buttons, active_button, flags, result_ptr, [pila] overlapped)
GUEST_IMPL(XamShowMessageBoxUI)
{
    uint32_t active = ctx.r8.u32, resultPtr = ctx.r10.u32, ov = Load32(ctx.r1.u32 + 0x54);
    // NG2_MSGBOX_BUTTON=<n>: boton elegido (por defecto el activo). En NG2 sin perfil: 0 = iniciar sesion, 1 = seguir sin guardar.
    if (const char* b = getenv("NG2_MSGBOX_BUTTON")) active = (uint32_t)atoi(b);
    else
    {   // NG2: "Do you want to sign in?" sin perfil -> "No, don't sign in" (el juego sigue sin poder guardar, como en Xenia)
        std::string t; for (int i = 0; ctx.r5.u32 && i < 200; i++) { uint16_t c = __builtin_bswap16(*(uint16_t*)(g_base + ctx.r5.u32 + 2 * i)); if (!c) break; t += c < 0x80 ? (char)c : '?'; }
        if (t.find("want to sign in") != std::string::npos && ctx.r6.u32 >= 2) active = 1;
    }
    if (resultPtr) Store32(resultPtr, active);
    {
        auto wstr = [](uint32_t p, int max) { std::string r; for (int i = 0; p && i < max; i++) { uint16_t c = __builtin_bswap16(*(uint16_t*)(g_base + p + 2 * i)); if (!c) break; r += c < 0x80 ? (char)c : '?'; } return r; };
        std::string btns; for (uint32_t i = 0; i < ctx.r6.u32 && i < 4; i++) btns += " [" + wstr(Load32(ctx.r7.u32 + 4 * i), 40) + "]";
        static int n = 0; static std::string lastText; std::string txt = wstr(ctx.r5.u32, 160);
        if (n++ < 6 || txt != lastText) LOG("[xam] mensaje: \"%s\" botones%s activo %u", txt.c_str(), btns.c_str(), ctx.r8.u32);
        lastText = txt;
    }
    LOG("[xam] XamShowMessageBoxUI -> boton %u", active);
    FinishUi(ctx, ov, X_ERROR_SUCCESS);
}
// XamShowMessageBoxUIEx(user, title, text, button_count, buttons, active_button, flags, unknown, [pila] result_ptr, overlapped)
GUEST_IMPL(XamShowMessageBoxUIEx)
{
    uint32_t active = ctx.r8.u32, resultPtr = Load32(ctx.r1.u32 + 0x54), ov = Load32(ctx.r1.u32 + 0x5C);
    if (resultPtr) Store32(resultPtr, active);
    LOG("[xam] XamShowMessageBoxUIEx -> boton %u", active);
    FinishUi(ctx, ov, X_ERROR_SUCCESS);
}
// XamUserWriteProfileSettings(title_id, user_index, setting_count, settings, overlapped)
GUEST_IMPL(XamUserWriteProfileSettings) { if (ctx.r7.u32) { CompleteOverlapped(ctx.r7.u32, X_ERROR_SUCCESS); ctx.r3.u64 = X_ERROR_IO_PENDING; } else ctx.r3.u64 = X_ERROR_SUCCESS; }
// XamContentGetDeviceState(device_id, overlapped)
GUEST_IMPL(XamContentGetDeviceState)
{
    uint32_t r = (ctx.r3.u32 == 1) ? X_ERROR_SUCCESS : X_ERROR_DEVICE_NOT_CONNECTED;
    if (ctx.r4.u32) { CompleteOverlapped(ctx.r4.u32, r); ctx.r3.u64 = X_ERROR_IO_PENDING; } else ctx.r3.u64 = r;
}
// XamGetOverlappedResult(overlapped, length_ptr, wait): nuestras operaciones se completan al instante
GUEST_IMPL(XamGetOverlappedResult)
{
    uint32_t ov = ctx.r3.u32;
    if (!ov) { ctx.r3.u64 = 87; return; }
    if (ctx.r4.u32) Store32(ctx.r4.u32, Load32(ov + 4));
    ctx.r3.u64 = Load32(ov);
}
// XamShowDirtyDiscErrorUI: error de lectura del disco (no deberia ocurrir con el ISO)
GUEST_IMPL(XamShowDirtyDiscErrorUI) { LOG("[xam] XamShowDirtyDiscErrorUI: el juego informa de un error de lectura del disco"); ctx.r3.u64 = 0; }

// XamReadTileToTexture(tile_type, title_id, tile_id, user_index, buffer, stride, height, overlapped) (sesion 23):
// Sesion 24: imagen de jugador. Antes se devolvia transparente y en los menus se veia el cuadrado negro de fondo (junto a
// "STARS TO RANK"). Para las imagenes de jugador (tipos 2-5) se usa gamerpic.bmp de la carpeta del juego (BMP de 24 o 32
// bits, cualquier tamano, se escala) o, si no existe, una imagen por defecto dibujada aqui (una pua sobre un degradado).
// Formato del destino: A8R8G8B8 lineal en big endian (bytes A R G B), stride en bytes. Otros tipos: transparente.
namespace
{
struct GamerPic { int w = 0, h = 0; std::vector<uint32_t> argb; };
const GamerPic& LoadGamerPic()
{
    static GamerPic pic;
    static bool done = false;
    if (done) return pic;
    done = true;
    if (FILE* f = fopen("gamerpic.bmp", "rb"))
    {
        std::vector<uint8_t> d;
        uint8_t buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.insert(d.end(), buf, buf + n);
        fclose(f);
        if (d.size() > 54 && d[0] == 'B' && d[1] == 'M')
        {
            uint32_t off = *(uint32_t*)&d[10];
            int32_t w = *(int32_t*)&d[18], h = *(int32_t*)&d[22];
            uint16_t bpp = *(uint16_t*)&d[28];
            bool topDown = h < 0; if (h < 0) h = -h;
            uint32_t rowBytes = ((uint32_t(w) * bpp / 8) + 3) & ~3u;
            if ((bpp == 24 || bpp == 32) && w > 0 && h > 0 && w <= 1024 && h <= 1024 && off + uint64_t(rowBytes) * h <= d.size())
            {
                pic.w = w; pic.h = h; pic.argb.resize(size_t(w) * h);
                for (int y = 0; y < h; y++)
                {
                    const uint8_t* row = &d[off + size_t(topDown ? y : h - 1 - y) * rowBytes];
                    for (int x = 0; x < w; x++)
                    {
                        const uint8_t* px = row + x * (bpp / 8);
                        uint32_t a = bpp == 32 ? px[3] : 255;
                        if (bpp == 32 && a == 0 && (px[0] | px[1] | px[2])) a = 255;   // BMP de 32 bits sin alfa real
                        pic.argb[size_t(y) * w + x] = (a << 24) | (uint32_t(px[2]) << 16) | (uint32_t(px[1]) << 8) | px[0];
                    }
                }
                LOG("[xam] imagen de jugador: gamerpic.bmp (%dx%d)", w, h);
                return pic;
            }
        }
        LOG("[xam] gamerpic.bmp no valido (se usa la imagen por defecto)");
    }
    // Imagen por defecto 64x64: degradado rojo-naranja con una pua blanca con borde oscuro
    const int N = 64;
    pic.w = pic.h = N; pic.argb.resize(N * N);
    auto inPick = [](float x, float y, float grow) {
        // Pua: triangulo redondeado (distancia al triangulo encogido <= radio de las esquinas)
        const float ax = 21, ay = 19, bx = 43, by = 19, cx = 32, cy = 43, rad = 7;
        auto seg = [](float px, float py, float x0, float y0, float x1, float y1) {
            float vx = x1 - x0, vy = y1 - y0, wx = px - x0, wy = py - y0;
            float t = std::clamp((wx * vx + wy * vy) / (vx * vx + vy * vy), 0.0f, 1.0f);
            float dx = wx - vx * t, dy = wy - vy * t; return std::sqrt(dx * dx + dy * dy);
        };
        auto side = [](float px, float py, float x0, float y0, float x1, float y1) { return (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0); };
        bool inside = side(x, y, ax, ay, bx, by) >= 0 && side(x, y, bx, by, cx, cy) >= 0 && side(x, y, cx, cy, ax, ay) >= 0;
        if (inside) return true;
        float d = std::min({ seg(x, y, ax, ay, bx, by), seg(x, y, bx, by, cx, cy), seg(x, y, cx, cy, ax, ay) });
        return d <= rad + grow;
    };
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
        {
            float fx = x + 0.5f, fy = y + 0.5f;
            float g = fy / N;
            uint32_t r = uint32_t(200 + 40 * (1 - g)), gg = uint32_t(40 + 70 * (1 - g)), b = 30;
            uint32_t c = 0xFF000000u | (r << 16) | (gg << 8) | b;
            if (inPick(fx, fy, 2.0f)) c = 0xFF201010u;                       // borde
            if (inPick(fx, fy, 0.0f))
            {
                float shade = 0.85f + 0.15f * (1 - fy / N);
                uint32_t v = uint32_t(255 * shade);
                c = 0xFF000000u | (v << 16) | (v << 8) | v;
            }
            pic.argb[size_t(y) * N + x] = c;
        }
    return pic;
}
} // namespace
GUEST_IMPL(XamReadTileToTexture)
{
    uint32_t type = ctx.r3.u32, buf = ctx.r7.u32, stride = ctx.r8.u32, height = ctx.r9.u32, ov = ctx.r10.u32;
    static int logged = 0;
    if (logged++ < 8) LOG("[xam] XamReadTileToTexture(tipo %u, titulo %08X, tile %08X, usuario %u, buffer %08X, stride %u, alto %u)",
                          type, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, buf, stride, height);
    if (buf && stride && height && uint64_t(stride) * height < (16u << 20))
    {
        memset(g_base + buf, 0, size_t(stride) * height);
        static const bool noPic = getenv("NG2_NOGAMERPIC") != nullptr;
        if (!noPic && type >= 2 && type <= 5)
        {
            const GamerPic& pic = LoadGamerPic();
            uint32_t w = stride / 4;
            if (pic.w > 0 && w > 0)
                for (uint32_t y = 0; y < height; y++)
                    for (uint32_t x = 0; x < w; x++)
                    {
                        uint32_t c = pic.argb[size_t(y * pic.h / height) * pic.w + (x * pic.w / w)];
                        Store32(buf + y * stride + x * 4, c);   // big endian: bytes A R G B
                    }
        }
    }
    if (ov) { CompleteOverlapped(ov, X_ERROR_SUCCESS); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = X_ERROR_SUCCESS;
}

// ---- selector de dispositivo de almacenamiento (sesion 16) ----
// XamShowDeviceSelectorUI(user_index, content_type, content_flags, total_requested, device_id_ptr, overlapped)
// Sin interfaz: se elige directamente el disco duro (ID 1, como el dispositivo ficticio de Xenia) y se avisa con XN_SYS_UI on/off.
GUEST_IMPL(XamShowDeviceSelectorUI)
{
    uint32_t devPtr = ctx.r7.u32, ov = ctx.r8.u32;
    LOG("[xam] XamShowDeviceSelectorUI(usuario %u, tipo 0x%X, flags 0x%X) -> disco duro", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    if (devPtr) Store32(devPtr, 1);
    BroadcastNotification(0x09, 1);
    BroadcastNotification(0x09, 0);
    if (ov) { CompleteOverlapped(ov, X_ERROR_SUCCESS); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = X_ERROR_SUCCESS;
}

// ---- chat de voz: no disponible (como Xenia) ----
// XamVoiceCreate(user_index, unk, voice_ptr_out)
GUEST_IMPL(XamVoiceCreate) { if (ctx.r5.u32) Store32(ctx.r5.u32, 0); ctx.r3.u64 = 5; } // ERROR_ACCESS_DENIED
// XamUserCheckPrivilege(user_index, privilege_mask, out_result)
GUEST_IMPL(XamUserCheckPrivilege) { if (ctx.r5.u32) Store32(ctx.r5.u32, 1); ctx.r3.u64 = X_ERROR_SUCCESS; }
GUEST_IMPL(XMsgCancelIORequest) { ctx.r3.u64 = X_ERROR_SUCCESS; }

// ---- memoria ----
// XamAlloc(flags, size, out_ptr)
GUEST_IMPL(XamAlloc)
{
    uint32_t p = GuestAllocVirtual(ctx.r4.u32 ? ctx.r4.u32 : 1, 0x10);
    if (ctx.r5.u32) Store32(ctx.r5.u32, p);
    ctx.r3.u64 = p ? X_ERROR_SUCCESS : 0x8007000E; // E_OUTOFMEMORY
}
GUEST_IMPL(XamFree) { ctx.r3.u64 = X_ERROR_SUCCESS; }

// ---- perfil ----
// 1 = sesion local iniciada, 2 = sesion en Live. NG2_SIGNIN=<n> fuerza el estado del usuario 0 (diagnostico, sesion 24)
GUEST_IMPL(XamUserGetSigninState)
{
    static int st = getenv("NG2_SIGNIN") ? atoi(getenv("NG2_SIGNIN")) : 1;   // sesion 12: con perfil por defecto (guardado arreglado); NG2_SIGNIN=0 = sin perfil
    static int logged = 0;
    if (getenv("NG2_CFUNCLOG") && logged < 10) { logged++; LOG("[xam] XamUserGetSigninState(%u)", ctx.r3.u32); }
    ctx.r3.u64 = ctx.r3.u32 == 0 ? st : 0;
}
// XamUserGetSigninInfo(user, flags, X_USER_SIGNIN_INFO*) {xuid u64, unk08, signin_state, unk10, unk14, name[16]}
GUEST_IMPL(XamUserGetSigninInfo)
{
    if (ctx.r3.u32 != 0) { ctx.r3.u64 = X_ERROR_NO_SUCH_USER; return; }
    uint32_t p = ctx.r5.u32;
    memset(g_base + p, 0, 0x28);
    Store64(p, kXuid); Store32(p + 0x0C, 1);
    memcpy(g_base + p + 0x18, "Player", 7);
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XamUserGetXUID(user, type_mask, xuid_ptr)
GUEST_IMPL(XamUserGetXUID)
{
    if (ctx.r3.u32 != 0) { ctx.r3.u64 = X_ERROR_NO_SUCH_USER; return; }
    if (ctx.r5.u32) Store64(ctx.r5.u32, kXuid);
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XamUserGetName(user, buffer, size)
GUEST_IMPL(XamUserGetName)
{
    if (ctx.r3.u32 != 0) { ctx.r3.u64 = X_ERROR_NO_SUCH_USER; return; }
    if (ctx.r4.u32 && ctx.r5.u32) { uint32_t n = ctx.r5.u32 < 7 ? ctx.r5.u32 : 7; memcpy(g_base + ctx.r4.u32, "Player", n); g_base[ctx.r4.u32 + n - 1] = 0; }
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
GUEST_IMPL(XamUserGetDeviceContext) { ctx.r3.u64 = 0; }

// ---- mandos ----
// Sesion 20: estado comun de botones = teclado + guion (NG2_INPUTSCRIPT) + NG2_AUTOSTART.
// Teclado (sesion 11, NG2): ver CurrentButtons (WASD = moverse, IJKL = camara, Espacio/F/R/E = A/X/Y/B, Q = RT, Z = LT, C/V = LB/RB).
// Guion: archivo de texto con lineas "<segundos> <BOTON>[+BOTON...] [ms]" o "<segundos> DUMP" (captura shot_<segundos>.bmp).
// Botones: UP DOWN LEFT RIGHT START BACK A B X Y LB RB LT RT, sticks LUP LDOWN LLEFT LRIGHT RUP RDOWN RLEFT RRIGHT. Los tiempos cuentan desde el arranque del proceso.
extern std::atomic<int> g_dumpRequest;   // gpu_window.cpp
extern std::atomic<int> g_dumpSeq;       // gpu_window.cpp: frames seguidos que quedan por volcar
namespace
{
const DWORD g_processStart = GetTickCount();
struct ScriptStep { DWORD ms; uint16_t mask; DWORD hold; bool dump; bool fired = false; uint8_t sticks = 0; uint8_t trig = 0; };
std::vector<ScriptStep> LoadInputScript()
{
    std::vector<ScriptStep> v;
    const char* path = getenv("NG2_INPUTSCRIPT");
    if (!path) return v;
    FILE* f = fopen(path, "r");
    if (!f) { LOG("[input] no se pudo abrir el guion %s", path); return v; }
    static const struct { const char* n; uint16_t m; } names[] = {
        { "UP", 0x0001 }, { "DOWN", 0x0002 }, { "LEFT", 0x0004 }, { "RIGHT", 0x0008 }, { "START", 0x0010 }, { "BACK", 0x0020 },
        { "LB", 0x0100 }, { "RB", 0x0200 }, { "A", 0x1000 }, { "B", 0x2000 }, { "X", 0x4000 }, { "Y", 0x8000 },
        { "GREEN", 0x1000 }, { "RED", 0x2000 }, { "YELLOW", 0x8000 }, { "BLUE", 0x4000 }, { "ORANGE", 0x0100 } };
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        float secs; char btn[128] = {}; unsigned hold = 150;
        if (line[0] == '#' || sscanf(line, "%f %127s %u", &secs, btn, &hold) < 2) continue;
        ScriptStep s{ (DWORD)(secs * 1000), 0, hold, false };
        if (!strcmp(btn, "DUMP")) s.dump = true;
        else if (!strcmp(btn, "DUMP8")) { s.dump = true; s.hold = 0xFFFF; }   // 8 frames seguidos (sesion 23)
        else
            for (char* tok = strtok(btn, "+"); tok; tok = strtok(nullptr, "+"))
            {
                for (auto& n : names) if (!strcmp(tok, n.n)) s.mask |= n.m;
                static const char* st[] = { "LUP", "LDOWN", "LLEFT", "LRIGHT", "RUP", "RDOWN", "RLEFT", "RRIGHT" };
                for (int i = 0; i < 8; i++) if (!strcmp(tok, st[i])) s.sticks |= 1 << i;
                if (!strcmp(tok, "LT")) s.trig |= 1; if (!strcmp(tok, "RT")) s.trig |= 2;
            }
        v.push_back(s);
    }
    fclose(f);
    LOG("[input] guion %s: %zu pasos", path, v.size());
    return v;
}
// ---- mando real por XInput (sesion 23) ----
// El esquema de bits de XInput es el mismo que el de la 360; una guitarra de Xbox 360 en PC llega con su subtipo (6/7/8...)
// y la palanca de tremolo/inclinacion en el stick derecho, igual que en la consola.
struct PadState { bool connected = false; XINPUT_STATE st{}; BYTE subtype = 0; };
PadState PollPad()
{
    static PadState s;
    static DWORD lastTry = 0;
    static bool disabled = getenv("NG2_NOXINPUT") != nullptr;
    if (disabled) return s;
    DWORD now = GetTickCount();
    if (!s.connected && lastTry && now - lastTry < 2000) return s;   // sin mando: reintentar cada 2 s (la consulta es lenta)
    lastTry = now;
    bool was = s.connected;
    s.connected = XInputGetState(0, &s.st) == ERROR_SUCCESS;
    if (s.connected && !was)
    {
        XINPUT_CAPABILITIES caps{};
        if (XInputGetCapabilities(0, 0, &caps) == ERROR_SUCCESS) s.subtype = caps.SubType;
        LOG("[input] mando XInput conectado (subtipo %u)", s.subtype);
    }
    else if (!s.connected && was) LOG("[input] mando XInput desconectado");
    return s;
}
// sticks (opcional): lx, ly, rx, ry desde el teclado (WASD / IJKL) y el guion (LUP/LDOWN/LLEFT/LRIGHT, RUP/RDOWN/RLEFT/RRIGHT)
uint16_t CurrentButtons(uint8_t* lt = nullptr, uint8_t* rt = nullptr, int16_t* sticks = nullptr)
{
    uint16_t buttons = 0;
    uint8_t stickMask = 0;   // bits: 0 LU, 1 LD, 2 LL, 3 LR, 4 RU, 5 RD, 6 RL, 7 RR
    {
        PadState pad = PollPad();
        if (pad.connected)
        {
            buttons |= pad.st.Gamepad.wButtons;
            if (lt) *lt = std::max<uint8_t>(*lt, pad.st.Gamepad.bLeftTrigger);
            if (rt) *rt = std::max<uint8_t>(*rt, pad.st.Gamepad.bRightTrigger);
        }
        // Mando de PlayStation por HID (dualsense.cpp)
        uint16_t pb; uint8_t plt, prt; int16_t ps[4];
        if (PsPadGetState(pb, plt, prt, ps))
        {
            buttons |= pb;
            if (lt) *lt = std::max<uint8_t>(*lt, plt);
            if (rt) *rt = std::max<uint8_t>(*rt, prt);
        }
    }
    DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (pid == GetCurrentProcessId())
    {
        // Sesion 11: distribucion de NG2 (juego de accion). Menus: flechas = cruceta, Enter = START, Retroceso = BACK, Esc = B.
        // Juego: WASD = stick izquierdo (moverse), IJKL = stick derecho (camara), Espacio = A (saltar), F = X (golpe rapido),
        // R = Y (golpe fuerte), E = B (proyectil), Q = RT (bloquear), Z = LT, C = LB, V = RB (ninpo).
        auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
        if (down(VK_UP)) buttons |= 0x0001; if (down(VK_DOWN)) buttons |= 0x0002;
        if (down(VK_LEFT)) buttons |= 0x0004; if (down(VK_RIGHT)) buttons |= 0x0008;
        if (down(VK_RETURN)) buttons |= 0x0010; if (down(VK_BACK)) buttons |= 0x0020;
        if (down(VK_SPACE)) buttons |= 0x1000; if (down(VK_ESCAPE) || down('E')) buttons |= 0x2000;
        if (down('F')) buttons |= 0x4000; if (down('R')) buttons |= 0x8000;
        if (down('C')) buttons |= 0x0100; if (down('V')) buttons |= 0x0200;
        if (rt && down('Q')) *rt = 255; if (lt && down('Z')) *lt = 255;
        if (down('W')) stickMask |= 1; if (down('S')) stickMask |= 2; if (down('A')) stickMask |= 4; if (down('D')) stickMask |= 8;
        if (down('I')) stickMask |= 16; if (down('K')) stickMask |= 32; if (down('J')) stickMask |= 64; if (down('L')) stickMask |= 128;
    }
    static bool autoStart = getenv("NG2_AUTOSTART") != nullptr;
    if (autoStart) { DWORD t = GetTickCount() / 250 % 24; if (t == 0) buttons |= 0x0010; else if (t == 12) buttons |= 0x1000; }
    static std::mutex m;
    static std::vector<ScriptStep> steps = LoadInputScript();
    if (!steps.empty())
    {
        std::lock_guard l(m);
        DWORD now = GetTickCount() - g_processStart;
        for (auto& s : steps)
        {
            if (now < s.ms) continue;
            if (s.dump) { if (!s.fired) { s.fired = true; if (s.hold == 0xFFFF) g_dumpSeq = 8; else g_dumpRequest = (int)(s.ms / 1000); } continue; }
            if (now < s.ms + s.hold) { buttons |= s.mask; stickMask |= s.sticks; if ((s.trig & 1) && lt) *lt = 255; if ((s.trig & 2) && rt) *rt = 255; }
            if (!s.fired) { s.fired = true; LOG("[input] guion t=%.1f s: botones 0x%04X", s.ms / 1000.0, s.mask); }
        }
    }
    if (sticks && stickMask)
    {
        auto ax = [](bool neg, bool pos) -> int16_t { return pos && !neg ? 32767 : neg && !pos ? -32768 : 0; };
        int16_t k[4] = { ax(stickMask & 4, stickMask & 8), ax(stickMask & 2, stickMask & 1), ax(stickMask & 64, stickMask & 128), ax(stickMask & 32, stickMask & 16) };
        for (int i = 0; i < 4; i++) if (k[i]) sticks[i] = k[i];   // el teclado/guion manda sobre el stick fisico en ese eje
    }
    return buttons;
}
} // namespace
// XamInputGetCapabilities(user, flags, X_INPUT_CAPABILITIES*) {type u8, sub_type u8, flags u16, gamepad[12], vibration[4]}
GUEST_IMPL(XamInputGetCapabilities)
{
    uint32_t user = ctx.r3.u32;
    static std::atomic<uint64_t> calls{ 0 };
    uint64_t c = ++calls;
    if (c == 1 || c % 2000 == 0) LOG("[input] XamInputGetState llamada %llu (usuario %u, flags %u)", (unsigned long long)c, user, ctx.r4.u32);
    if (user != 0 && user != 0xFF) { ctx.r3.u64 = X_ERROR_DEVICE_NOT_CONNECTED; return; }
    uint32_t p = ctx.r5.u32;
    memset(g_base + p, 0, 0x14);
    // XINPUT_DEVTYPE_GAMEPAD, subtipo 1 = mando normal (en GH era 6 = guitarra)
    PadState real = PollPad();
    g_base[p] = 1; g_base[p + 1] = real.connected && real.subtype ? real.subtype : 1;   // con mando real, su subtipo
    Store16(p + 4, 0xFFFF); g_base[p + 6] = 0xFF; g_base[p + 7] = 0xFF;
    Store16(p + 8, 0xFFC0); Store16(p + 10, 0xFFC0); Store16(p + 12, 0xFFC0); Store16(p + 14, 0xFFC0);
    Store16(p + 16, 0xFFFF); Store16(p + 18, 0xFFFF);
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XamInputGetState(user, flags, X_INPUT_STATE*) {packet u32, buttons u16, lt u8, rt u8, lx, ly, rx, ry s16}
GUEST_IMPL(XamInputGetState)
{
    uint32_t user = ctx.r3.u32;
    static std::atomic<uint64_t> calls{ 0 };
    uint64_t c = ++calls;
    if (c == 1 || c % 2000 == 0) LOG("[input] XamInputGetState llamada %llu (usuario %u, flags %u)", (unsigned long long)c, user, ctx.r4.u32);
    if (user != 0 && user != 0xFF) { ctx.r3.u64 = X_ERROR_DEVICE_NOT_CONNECTED; return; }
    uint32_t p = ctx.r5.u32;
    if (!p) { ctx.r3.u64 = X_ERROR_SUCCESS; return; }
    // Teclado -> mando 1 (sesion 10). Solo si la ventana del juego tiene el foco.
    // Sticks: mando XInput, si no el de PlayStation; el teclado y el guion mandan en los ejes que usen (sesion 11)
    int16_t sticks[4] = {};
    PadState real = PollPad();
    if (real.connected) { sticks[0] = real.st.Gamepad.sThumbLX; sticks[1] = real.st.Gamepad.sThumbLY; sticks[2] = real.st.Gamepad.sThumbRX; sticks[3] = real.st.Gamepad.sThumbRY; }
    else { uint16_t pb; uint8_t plt, prt; int16_t ps[4]; if (PsPadGetState(pb, plt, prt, ps)) memcpy(sticks, ps, sizeof(ps)); }
    uint8_t lt = 0, rt = 0;
    uint16_t buttons = CurrentButtons(&lt, &rt, sticks);
    static uint32_t packet = 0; static uint16_t lastButtons = 0xFFFF; static int16_t lastSticks[4] = {}; static uint16_t lastTrig = 0;
    uint16_t trig = uint16_t(lt) | (uint16_t(rt) << 8);
    if (buttons != lastButtons || memcmp(lastSticks, sticks, sizeof(sticks)) || trig != lastTrig)
    { packet++; lastButtons = buttons; memcpy(lastSticks, sticks, sizeof(sticks)); lastTrig = trig; }
    memset(g_base + p, 0, 0x10);
    Store32(p, packet); Store16(p + 4, buttons); g_base[p + 6] = lt; g_base[p + 7] = rt;
    Store16(p + 8, (uint16_t)sticks[0]); Store16(p + 10, (uint16_t)sticks[1]); Store16(p + 12, (uint16_t)sticks[2]); Store16(p + 14, (uint16_t)sticks[3]);
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XamInputSetState(user, flags, X_INPUT_VIBRATION* {u16 izquierdo, u16 derecho}): vibracion al mando XInput (sesion 11)
GUEST_IMPL(XamInputSetState)
{
    uint32_t user = ctx.r3.u32, v = ctx.r5.u32;
    if (user != 0 && user != 0xFF) { ctx.r3.u64 = X_ERROR_DEVICE_NOT_CONNECTED; return; }
    static const bool noRumble = getenv("NG2_NOVIBRACION") != nullptr;
    if (v && !noRumble && PollPad().connected) { XINPUT_VIBRATION xv{ __builtin_bswap16(*(const uint16_t*)(g_base + v)), __builtin_bswap16(*(const uint16_t*)(g_base + v + 2)) }; XInputSetState(0, &xv); }
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XamInputGetKeystrokeEx(user_index_ptr, flags, X_INPUT_KEYSTROKE*) {u16 vk, u16 unicode, u16 flags (1 down, 2 up), u8 user, u8 hid}
// Sesion 13: eventos generados comparando el estado de los botones (mismo origen que XamInputGetState: teclado + NG2_AUTOSTART)
GUEST_IMPL(XamInputGetKeystrokeEx)
{
    uint16_t buttons = CurrentButtons();
    static const struct { uint16_t mask, vk; } map[] = {
        { 0x0010, 0x5814 }, { 0x1000, 0x5800 }, { 0x2000, 0x5801 }, { 0x4000, 0x5802 }, { 0x8000, 0x5803 },
        { 0x0001, 0x5810 }, { 0x0002, 0x5811 }, { 0x0004, 0x5812 }, { 0x0008, 0x5813 }, { 0x0020, 0x5815 },
        { 0x0100, 0x5804 }, { 0x0200, 0x5805 } };   // LB / RB (VK_PAD_LSHOULDER / RSHOULDER)
    static uint16_t last = 0;
    uint32_t userPtr = ctx.r3.u32, p = ctx.r5.u32;
    uint32_t user = userPtr ? Load32(userPtr) : 0;
    if (user != 0 && user != 0xFF) { ctx.r3.u64 = X_ERROR_DEVICE_NOT_CONNECTED; return; }
    uint16_t changed = buttons ^ last;
    for (auto& m : map)
        if (changed & m.mask)
        {
            bool isDown = (buttons & m.mask) != 0;
            last ^= m.mask;
            if (p) { memset(g_base + p, 0, 8); Store16(p, m.vk); Store16(p + 4, isDown ? 1 : 2); g_base[p + 6] = 0; }
            if (userPtr) Store32(userPtr, 0);
            ctx.r3.u64 = X_ERROR_SUCCESS;
            return;
        }
    ctx.r3.u64 = X_ERROR_EMPTY;
}

// ---- audio ----
GUEST_IMPL(XAudioGetSpeakerConfig) { if (ctx.r3.u32) Store32(ctx.r3.u32, 0x00010001); ctx.r3.u64 = X_ERROR_SUCCESS; }
// ---- motor de audio (sesion 10): el "hardware" pide un frame de 256 muestras cada ~5,33 ms llamando al callback del juego ----
// Las muestras enviadas con XAudioSubmitRenderDriverFrame se descartan por ahora (la salida por WASAPI llega en la sesion de audio).
namespace
{
std::atomic<uint32_t> g_audioCb{ 0 }, g_audioArg{ 0 };
std::atomic<uint64_t> g_audioFrames{ 0 }, g_audioSubmits{ 0 };
std::atomic<bool> g_audioThread{ false };
}
size_t AudioQueuedFrames();     // audio_wasapi.cpp
bool AudioOutputActive();
namespace
{
void AudioPump()
{
    auto* actx = new PPCContext{};
    InitGuestThread(*actx, 0x10000);
    RegisterGuestThread(actx, 0xA0D10000);   // NG2: visible en NG2_SAMPLE ("entrada" ficticia del audio)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);   // el "hardware" de audio no debe esperar a los demas hilos (sesion 23)
    LARGE_INTEGER freq, t0, now; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&t0);
    timeBeginPeriod(1);
    static bool clocked = getenv("NG2_NOAUDIO") != nullptr;   // sin salida de audio: ritmo por reloj
    for (uint64_t n = 1;; n++)
    {
        if (clocked || !AudioOutputActive())
        {
            // siguiente instante: n * 256 / 48000 s
            LONGLONG target = t0.QuadPart + (LONGLONG)(n * 256 * freq.QuadPart / 48000);
            for (;;) { QueryPerformanceCounter(&now); LONGLONG left = target - now.QuadPart; if (left <= 0) break; if (left > freq.QuadPart / 500) Sleep(1); else YieldProcessor(); }
        }
        else
        {
            // Sesion 21: ritmo marcado por la tarjeta de sonido; se genera mientras la cola tenga menos de ~50 ms
            // Sesion 24 (retraso): margen NG2_AUDIOMS (por defecto 40 ms). Antes 120 ms fijos (sesion 23: con 50 ms habia huecos,
            // cuando el juego iba a ~30 fps con el hilo de la GPU saturado).
            static const size_t queueTarget = 48 * size_t(std::clamp(getenv("NG2_AUDIOMS") ? atoi(getenv("NG2_AUDIOMS")) : 40, 10, 250));
            while (AudioQueuedFrames() >= queueTarget) Sleep(1);
            // Sesion 27: ritmo como el del hardware (un frame cada 5,33 ms). WASAPI saca 10 ms de golpe y antes se pedian dos frames
            // seguidos: el callback del juego entrega el frame que mezclaron sus hilos de XAudio y les pide el siguiente; si se le
            // vuelve a llamar antes de que terminen, entrega un frame vacio (silencios de 5-70 ms en combate, "el audio se traba").
            // Separacion minima de 4,8 ms salvo con la cola casi vacia (< 15 ms). NG2_AUDIOBURST=1 = antes.
            static const bool burst = getenv("NG2_AUDIOBURST") != nullptr;
            static LARGE_INTEGER lastCb{};
            if (!burst && lastCb.QuadPart)
            {
                const LONGLONG minGap = freq.QuadPart * 48 / 10000;   // 4,8 ms
                for (;;)
                {
                    QueryPerformanceCounter(&now);
                    LONGLONG left = lastCb.QuadPart + minGap - now.QuadPart;
                    if (left <= 0 || AudioQueuedFrames() < 720) break;
                    if (left > freq.QuadPart / 500) Sleep(1); else YieldProcessor();
                }
            }
            QueryPerformanceCounter(&lastCb);
            QueryPerformanceCounter(&t0); n = 0;
        }
        uint32_t cb = g_audioCb.load();
        if (!cb) continue;
        PPCFunc* fn = PPC_LOOKUP_FUNC(g_base, cb);
        if (!fn) continue;
        uint64_t r1 = actx->r1.u64, r13 = actx->r13.u64;
        uint64_t before = g_audioSubmits.load();
        actx->r3.u64 = g_audioArg.load();
        LARGE_INTEGER cbStart; QueryPerformanceCounter(&cbStart);
        fn(*actx, g_base);
        actx->r1.u64 = r1; actx->r13.u64 = r13;
        {   // Sesion 28: diagnostico de los huecos de salida durante los tirones del juego: callback lento o sin frame
            LARGE_INTEGER cbEnd; QueryPerformanceCounter(&cbEnd);
            const double ms = (cbEnd.QuadPart - cbStart.QuadPart) * 1000.0 / freq.QuadPart;
            static int nslow = 0, nmiss = 0;
            const bool miss = g_audioSubmits.load() == before;
            if (ms > 8.0 && nslow < 200) { nslow++; LOG("[audio] callback lento t=%.2fs: %.1f ms (en cola %zu frames)%s", GetTickCount64() / 1000.0, ms, AudioQueuedFrames(), miss ? ", sin frame" : ""); }
            else if (miss && nmiss < 200) { nmiss++; LOG("[audio] callback sin frame t=%.2fs (%.1f ms, en cola %zu frames)", GetTickCount64() / 1000.0, ms, AudioQueuedFrames()); }
        }
        if (g_audioSubmits.load() == before) Sleep(5);   // el juego no envio frame: no girar en vacio
        if (g_audioFrames.load() < 3) LOG("[audio] callback %llu devuelto (envios %llu)", (unsigned long long)g_audioFrames.load(), (unsigned long long)g_audioSubmits.load());
        if (++g_audioFrames % 1875 == 0) LOG("[audio] %llu frames pedidos, %llu enviados por el juego", (unsigned long long)g_audioFrames.load(), (unsigned long long)g_audioSubmits.load());
    }
}
}
// XAudioRegisterRenderDriverClient(callback_ptr -> {callback, arg}, driver_ptr)
GUEST_IMPL(XAudioRegisterRenderDriverClient)
{
    uint32_t cb = ctx.r3.u32 ? Load32(ctx.r3.u32) : 0, arg = ctx.r3.u32 ? Load32(ctx.r3.u32 + 4) : 0;
    LOG("[audio] XAudioRegisterRenderDriverClient callback=0x%08X arg=0x%08X", cb, arg);
    // Como Xenia: el callback recibe un PUNTERO a una palabra de invitado que contiene el argumento
    uint32_t wrapped = GuestAllocKernel(4, 4);
    Store32(wrapped, arg);
    g_audioArg = wrapped; g_audioCb = cb;
    bool expected = false;
    if (g_audioThread.compare_exchange_strong(expected, true)) std::thread(AudioPump).detach();
    if (ctx.r4.u32) Store32(ctx.r4.u32, 0x41550000);
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
GUEST_IMPL(XAudioUnregisterRenderDriverClient) { g_audioCb = 0; ctx.r3.u64 = X_ERROR_SUCCESS; }
// XAudioSubmitRenderDriverFrame(driver, samples_ptr): 256 muestras x 6 canales float BE -> WASAPI (audio_wasapi.cpp, sesion 21)
void AudioSubmitXenonFrame(const uint8_t* src);
GUEST_IMPL(XAudioSubmitRenderDriverFrame)
{
    g_audioSubmits++;
    if (ctx.r4.u32) AudioSubmitXenonFrame(g_base + ctx.r4.u32);
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
GUEST_IMPL(XAudioOverrideSpeakerConfig) { ctx.r3.u64 = X_ERROR_SUCCESS; }
// XMACreateContext / XMAReleaseContext: en xma.cpp
uint64_t AudioFramesRequested() { return g_audioFrames.load(); }
uint64_t AudioFramesSubmitted() { return g_audioSubmits.load(); }
GUEST_IMPL(XAudioGetVoiceCategoryVolume)
{
    float v = 1.0f; uint32_t b; memcpy(&b, &v, 4);
    if (ctx.r4.u32) Store32(ctx.r4.u32, b);
    ctx.r3.u64 = X_ERROR_SUCCESS;
}

// ---- enumeraciones (contenido, partidas): de momento vacias ----
// XamCreateEnumeratorHandle(user, app_id, open_msg, close_msg, extra_size, item_count, flags, out_handle)
GUEST_IMPL(XamCreateEnumeratorHandle)
{
    auto o = std::make_shared<KObject>();
    o->guestPtr = GuestAllocKernel((ctx.r7.u32 ? ctx.r7.u32 : 4) + 0x40, 0x10);
    if (ctx.r10.u32) Store32(ctx.r10.u32, InsertHandle(o));
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XamGetPrivateEnumStructureFromHandle(handle, out_ptr)
GUEST_IMPL(XamGetPrivateEnumStructureFromHandle)
{
    auto o = GetHandleObject(ctx.r3.u32);
    if (ctx.r4.u32) Store32(ctx.r4.u32, o ? o->guestPtr : 0);
    ctx.r3.u64 = o ? X_ERROR_SUCCESS : 6; // ERROR_INVALID_HANDLE
}
// XamEnumerate(handle, flags, buffer, buffer_size, items_returned_ptr, overlapped)
GUEST_IMPL(XamEnumerate)
{
    if (ctx.r7.u32) Store32(ctx.r7.u32, 0);
    if (auto e = std::dynamic_pointer_cast<ContentEnum>(GetHandleObject(ctx.r3.u32)))
    {
        uint32_t buf = ctx.r5.u32, cap = ctx.r6.u32 / e->itemSize, n = 0;
        { static int nlog = 0; if (nlog++ < 40) LOG("[xam] XamEnumerate(handle %08X, buffer %u bytes, overlapped %08X): %zu/%zu elementos", ctx.r3.u32, ctx.r6.u32, ctx.r8.u32, e->next, e->items.size()); }
        while (e->next < e->items.size() && n < cap && n < e->perCall)
        {
            memcpy(g_base + buf + n * e->itemSize, e->items[e->next].data(), e->itemSize);
            e->next++; n++;
        }
        if (n)
        {
            if (ctx.r7.u32) Store32(ctx.r7.u32, n);
            if (ctx.r8.u32) { CompleteOverlapped(ctx.r8.u32, X_ERROR_SUCCESS, n); ctx.r3.u64 = X_ERROR_IO_PENDING; }
            else ctx.r3.u64 = X_ERROR_SUCCESS;
            return;
        }
    }
    if (ctx.r8.u32) { CompleteOverlapped(ctx.r8.u32, X_ERROR_NO_MORE_FILES); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = X_ERROR_NO_MORE_FILES;
}

// ---- mensajes XMsg ----
// XMsgInProcessCall(app, message, param1, param2). Sesion 11: app 0xFA (reproductor XMP) mensaje 0x70009 = XMPGetStatus
// {u32 status_ptr}: estado 0 = parado (como Xenia). Antes no se escribia y el juego leia basura cada frame.
GUEST_IMPL(XMsgInProcessCall)
{
    uint32_t app = ctx.r3.u32, msg = ctx.r4.u32, a = ctx.r5.u32;
    if (app == 0xFA && msg == 0x00070009 && a) { uint32_t st = Load32(a); if (st) Store32(st, 0); }
    else { static int n = 0; if (n++ < 20) LOG("[xmsg] XMsgInProcessCall(app %X, mensaje %08X, %08X, %08X) sin implementar", app, msg, a, ctx.r6.u32); }
    ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XMsgStartIORequestEx(app, message, overlapped, buffer, buffer_size, unk)
GUEST_IMPL(XMsgStartIORequestEx)
{
    if (ctx.r5.u32) { CompleteOverlapped(ctx.r5.u32, X_ERROR_SUCCESS); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = X_ERROR_SUCCESS;
}
// XMsgStartIORequest(app, message, overlapped, buffer, buffer_size)
GUEST_IMPL(XMsgStartIORequest)
{
    if (ctx.r5.u32) { CompleteOverlapped(ctx.r5.u32, X_ERROR_SUCCESS); ctx.r3.u64 = X_ERROR_IO_PENDING; }
    else ctx.r3.u64 = X_ERROR_SUCCESS;
}

// NetDll_XNetGetTitleXnAddr(caller, XNADDR*): como Xenia, loopback y XNET_GET_XNADDR_STATIC (antes devolvia 0 = PENDIENTE para siempre)
// XNADDR: ina(4) inaOnline(4) wPortOnline(2) abEnet[6] abOnline[20]
GUEST_IMPL(NetDll_XNetGetTitleXnAddr)
{
    uint32_t a = ctx.r4.u32;
    if (a)
    {
        memset(g_base + a, 0, 36);
        Store32(a, 0x7F000001);
        memset(g_base + a + 10, 0xCC, 6);
    }
    ctx.r3.u64 = 0x04; // XNET_GET_XNADDR_STATIC
}
GUEST_IMPL(XCustomRegisterDynamicActions) { ctx.r3.u64 = 0; }
// Avatares: como Xenia, XamAvatarInitialize FALLA (~0) para que el juego no los use (con "exito" se quedaba esperando
// XamAvatarGetManifestLocalUser / XamAvatarGetMetadataRandom, que nadie completa).
GUEST_IMPL(XamAvatarInitialize) { ctx.r3.u64 = 0xFFFFFFFF; }
GUEST_IMPL(NetDll_XNetUnregisterInAddr) { ctx.r3.u64 = 0; }

// ---- red: sin conexion ----
GUEST_IMPL(NetDll_socket) { ctx.r3.u64 = 0xFFFFFFFF; }        // INVALID_SOCKET
GUEST_IMPL(NetDll_bind) { ctx.r3.u64 = 0xFFFFFFFF; }          // SOCKET_ERROR
GUEST_IMPL(NetDll_ioctlsocket) { ctx.r3.u64 = 0xFFFFFFFF; }
GUEST_IMPL(NetDll_XNetGetEthernetLinkStatus) { ctx.r3.u64 = 0; } // sin enlace

// (Los ganchos de diagnostico de funciones de script de GH se han quitado: eran direcciones de GH.)
