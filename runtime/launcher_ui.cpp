// Launcher (sesion 25 de NG2): ventana de configuracion que se abre antes del juego. Adaptado del launcher del proyecto de
// Guitar Hero (mismo autor): paneles con esquinas cortadas, filas con descripcion, selectores < valor > y conmutadores SI/NO,
// con los colores de Ninja Gaiden (negro, rojo sangre y acero). Lee y escribe ng2.ini conservando comentarios y claves.
// portada.png y logo.png junto al exe (imagenes de SteamGridDB) son la cabecera de la pagina Inicio y el logo de la barra lateral.
// "mostrar_launcher=0" entra directo al juego (mantener Mayus al abrir lo vuelve a mostrar); NG2.exe --jugar lo salta y --launcher
// lo fuerza.
#include "runtime.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <xinput.h>
#include <wincodec.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

bool PsPadGetState(uint16_t& buttons, uint8_t& lt, uint8_t& rt, int16_t sticks[4]);   // dualsense.cpp
std::string FindTitleUpdate(const std::string& dataDir);   // launcher.cpp (sesion 26)

namespace
{
std::string ExeDirL()
{
    char p[MAX_PATH]; GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s = p; return s.substr(0, s.find_last_of("\\/"));
}
// ---- ng2.ini como lista de lineas (se conservan comentarios y claves desconocidas) ----
struct IniDoc
{
    std::vector<std::string> lines;
    bool appendedHeader = false;
    static std::string Trim(const std::string& s) { size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t\r\n"); return a == std::string::npos ? "" : s.substr(a, b - a + 1); }
    void Load(const std::string& path)
    {
        lines.clear();
        if (FILE* f = fopen(path.c_str(), "r"))
        {
            char buf[2048];
            while (fgets(buf, sizeof(buf), f)) { std::string s = buf; while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back(); lines.push_back(s); }
            fclose(f);
        }
    }
    bool Save(const std::string& path) const
    {
        FILE* f = fopen(path.c_str(), "w"); if (!f) return false;
        for (auto& l : lines) fprintf(f, "%s\n", l.c_str());
        fclose(f); return true;
    }
    int Find(const std::string& k) const
    {
        for (size_t i = 0; i < lines.size(); i++)
        {
            std::string t = Trim(lines[i]);
            if (t.empty() || t[0] == ';' || t[0] == '#') continue;
            size_t eq = t.find('='); if (eq == std::string::npos) continue;
            if (Trim(t.substr(0, eq)) == k) return (int)i;
        }
        return -1;
    }
    std::string Get(const std::string& k, const std::string& def = "") const
    {
        int i = Find(k); if (i < 0) return def;
        return Trim(lines[i].substr(lines[i].find('=') + 1));
    }
    void Set(const std::string& k, const std::string& v)
    {
        int i = Find(k);
        if (i >= 0) { lines[i] = k + "=" + v; return; }
        if (!appendedHeader && Find("mostrar_launcher") < 0) { lines.push_back("; ---- Opciones del launcher ----"); appendedHeader = true; }
        lines.push_back(k + "=" + v);
    }
};

std::wstring W(const std::string& s) { if (s.empty()) return L""; int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0); std::wstring w(n - 1, 0); MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, w.data(), n); return w; }
std::string A(const std::wstring& w) { if (w.empty()) return ""; int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr); std::string s(n - 1, 0); WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr); return s; }
std::wstring GetText(HWND h) { int n = GetWindowTextLengthW(h); std::wstring s(n, 0); GetWindowTextW(h, s.data(), n + 1); return s; }

// ---- colores (Ninja Gaiden: negro, rojo sangre y acero) ----
const COLORREF kBg = RGB(9, 9, 11), kScan = RGB(14, 14, 17), kSide = RGB(7, 7, 9), kPanel = RGB(19, 19, 23), kPanelHi = RGB(54, 16, 20),
               kBar = RGB(8, 8, 10), kSel = RGB(48, 18, 22), kInput = RGB(28, 28, 33), kText = RGB(236, 236, 240), kDim = RGB(150, 150, 160),
               kAccent = RGB(206, 24, 38), kAccent2 = RGB(255, 92, 96), kLine = RGB(86, 34, 40), kAccentDim = RGB(150, 40, 48),
               kSteel = RGB(170, 176, 188), kPlayText = RGB(250, 240, 240);
enum
{
    ID_TAB0 = 100, ID_PLAY = 200, ID_SAVE, ID_SHOWLAUNCHER, ID_SAVED,
    ID_ISO = 300, ID_ISO_BROWSE, ID_DLC, ID_DLC_BROWSE, ID_DLCCOUNT, ID_SKIPINTRO, ID_PROFILE, ID_SAVEINFO, ID_SAVEOPEN, ID_TU, ID_TUINFO,
    ID_FULLSCREEN = 400, ID_WINSIZE, ID_SCALE, ID_VSYNC, ID_MBLUR, ID_GRAIN, ID_AUDIOLAT, ID_ANISO, ID_FPS, ID_PANO,
    ID_VIBRA = 500, ID_PADSTATUS, ID_KEYS,
    ID_SEC0 = 900, ID_BANNER = 990,
    ID_Q_PANO = 1100, ID_Q_FULL, ID_Q_FPS, ID_HOME_ISO, ID_HOME_KEYS,
};
// Paginas: 0 Juego, 1 Graficos, 2 Controles, 3 Audio, 4 Ayuda, 5 Inicio
constexpr int kTabs = 6, kHome = 5;
const int kTabOrder[kTabs] = { kHome, 0, 1, 2, 3, 4 };
int TabPos(int i) { for (int k = 0; k < kTabs; k++) if (kTabOrder[k] == i) return k; return 0; }
constexpr int kW = 1120, kH = 720, kSideW = 250, kX0 = 280, kXR = 1086, kY0 = 30, kPanelBottom = 634, kBarTop = 648;
const wchar_t* kWinSizes[] = { L"1280 x 720", L"1600 x 900", L"1920 x 1080", L"2560 x 1440", L"3840 x 2160",
                               L"2560 x 1080 (21:9)", L"3440 x 1440 (21:9)", L"3840 x 1080 (32:9)", L"5120 x 1440 (32:9)" };
const char* kWinSizesIni[] = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160", "2560x1080", "3440x1440", "3840x1080", "5120x1440" };
constexpr int kNumWinSizes = 9;

struct Ui
{
    HWND wnd = nullptr; HINSTANCE inst = nullptr; int dpi = 96;
    HFONT font = nullptr, fontBold = nullptr, fontTitle = nullptr, fontSmall = nullptr, fontHead = nullptr, fontTab = nullptr, fontPlay = nullptr;
    HBRUSH brBg = nullptr, brInput = nullptr, brPanel = nullptr, brBar = nullptr;
    int tab = kHome; std::vector<HWND> pages[kTabs];
    std::vector<std::pair<int, RECT>> frames;
    int nextSec = ID_SEC0;
    HBITMAP banner = nullptr; int bannerW = 0, bannerH = 0;          // portada.png (32 bits)
    std::vector<uint32_t> logo; int logoW = 0, logoH = 0;            // logo.png (alfa premultiplicado)
    HBITMAP logoBmp[2] = {}; int logoBw[2] = {}, logoBh[2] = {};      // escalado: 0 barra lateral, 1 portada
    bool isoMissing = false;
    std::map<HWND, bool> toggles; std::set<HWND> dimLabels;
    IniDoc ini;
    DWORD savedT = 0; int ticks = 0;
    bool result = false, done = false;
} g;
int S(int v) { return MulDiv(v, g.dpi, 96); }
HWND Item(int id) { return GetDlgItem(g.wnd, id); }
bool Toggled(int id) { auto it = g.toggles.find(Item(id)); return it != g.toggles.end() && it->second; }
int Sel(int id) { return (int)SendMessageW(Item(id), CB_GETCURSEL, 0, 0); }

HWND Make(int page, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id, DWORD ex = 0)
{
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | style | ((page < 0 || page == g.tab) ? WS_VISIBLE : 0), S(x), S(y), S(w), S(h), g.wnd,
                             (HMENU)(INT_PTR)id, g.inst, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)g.font, TRUE);
    if (page >= 0) g.pages[page].push_back(c);
    return c;
}
HWND Label(int page, const wchar_t* t, int x, int y, int w, int h = 22, int id = 0, bool dim = false, HFONT f = nullptr)
{
    HWND c = Make(page, L"STATIC", t, SS_LEFT | SS_NOPREFIX, x, y, w, h, id);
    if (dim) g.dimLabels.insert(c);
    if (f) SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
    return c;
}
int Section(int page, int y, const wchar_t* t) { Make(page, L"STATIC", t, SS_OWNERDRAW, kX0, y, kXR - kX0, 32, g.nextSec++); return y + 40; }
void Row(int page, int y, const wchar_t* label, const wchar_t* hint, int labelW = 520, int hintId = 0)
{
    Label(page, label, kX0, y, labelW, 21);
    if ((hint && *hint) || hintId) Label(page, hint ? hint : L"", kX0, y + 21, labelW + 40, 18, hintId, true, g.fontSmall);
}
HWND Toggle(int page, int x, int y, int id, bool on) { HWND c = Make(page, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, x, y, 84, 26, id); g.toggles[c] = on; return c; }
HWND ToggleRight(int page, int y, int id, bool on) { return Toggle(page, kXR - 84, y + 6, id, on); }
HWND Combo(int page, int x, int y, int w, int id, const std::vector<std::wstring>& items, int sel)
{
    HWND c = Make(page, L"NG2Sel", L"", WS_TABSTOP, x, y, w, 30, id);
    for (auto& s : items) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s.c_str());
    SendMessageW(c, CB_SETCURSEL, std::clamp(sel, 0, (int)items.size() - 1), 0);
    return c;
}
HWND ComboRight(int page, int y, int id, const std::vector<std::wstring>& items, int sel, int w = 280) { return Combo(page, kXR - w, y + 4, w, id, items, sel); }
HWND Button(int page, const wchar_t* t, int x, int y, int w, int h, int id) { return Make(page, L"BUTTON", t, BS_OWNERDRAW | WS_TABSTOP, x, y, w, h, id); }
HWND Edit(int page, const std::wstring& t, int x, int y, int w, int id)
{
    g.frames.push_back({ page, RECT{ x, y, x + w, y + 30 } });
    return Make(page, L"EDIT", t.c_str(), ES_AUTOHSCROLL | WS_TABSTOP, x + 10, y + 6, w - 20, 20, id);
}

// ---- dibujo ----
void FillR(HDC dc, RECT r, COLORREF c) { HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }
void Chamfer(HDC dc, RECT r, int cut, COLORREF fill, COLORREF border, int width = 1)
{
    POINT pts[6] = { { r.left + cut, r.top }, { r.right, r.top }, { r.right, r.bottom - cut }, { r.right - cut, r.bottom }, { r.left, r.bottom }, { r.left, r.top + cut } };
    HBRUSH b = fill == CLR_NONE ? (HBRUSH)GetStockObject(NULL_BRUSH) : CreateSolidBrush(fill);
    HPEN p = border == CLR_NONE ? (HPEN)GetStockObject(NULL_PEN) : CreatePen(PS_SOLID, width, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Polygon(dc, pts, 6);
    SelectObject(dc, ob); SelectObject(dc, op);
    if (fill != CLR_NONE) DeleteObject(b);
    if (border != CLR_NONE) DeleteObject(p);
}
void LineC(HDC dc, int x0, int y0, int x1, int y1, COLORREF c, int width = 1)
{
    HPEN p = CreatePen(PS_SOLID, width, c); HGDIOBJ op = SelectObject(dc, p);
    MoveToEx(dc, x0, y0, nullptr); LineTo(dc, x1, y1);
    SelectObject(dc, op); DeleteObject(p);
}
COLORREF Mix(COLORREF a, COLORREF b, float t) { return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t), (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t), (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t)); }
void Text(HDC dc, const wchar_t* t, RECT r, COLORREF c, HFONT f, UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE, int spacing = 0)
{
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, c); HGDIOBJ of = SelectObject(dc, f);
    SetTextCharacterExtra(dc, spacing);
    DrawTextW(dc, t, -1, &r, fmt | DT_NOPREFIX);
    SetTextCharacterExtra(dc, 0);
    SelectObject(dc, of);
}
// Logo (alfa premultiplicado) escalado a dw x dh con un promedio por area; se guarda por uso y tamano
HBITMAP ScaleLogo(int slot, int dw, int dh)
{
    if (g.logo.empty() || dw <= 0 || dh <= 0) return nullptr;
    if (g.logoBmp[slot] && g.logoBw[slot] == dw && g.logoBh[slot] == dh) return g.logoBmp[slot];
    if (g.logoBmp[slot]) DeleteObject(g.logoBmp[slot]);
    BITMAPINFO bi{}; bi.bmiHeader = { sizeof(BITMAPINFOHEADER), dw, -dh, 1, 32, BI_RGB };
    uint32_t* px = nullptr; HBITMAP hb = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, (void**)&px, nullptr, 0);
    g.logoBmp[slot] = hb; g.logoBw[slot] = dw; g.logoBh[slot] = dh;
    if (!hb || !px) return hb;
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++)
        {
            float x0 = (float)x * g.logoW / dw, x1 = (float)(x + 1) * g.logoW / dw, y0 = (float)y * g.logoH / dh, y1 = (float)(y + 1) * g.logoH / dh;
            float acc[4] = {}, wsum = 0;
            for (int sy = (int)y0; sy < (int)std::ceil(y1) && sy < g.logoH; sy++)
                for (int sx = (int)x0; sx < (int)std::ceil(x1) && sx < g.logoW; sx++)
                {
                    float wgt = (std::min(x1, (float)sx + 1) - std::max(x0, (float)sx)) * (std::min(y1, (float)sy + 1) - std::max(y0, (float)sy));
                    if (wgt <= 0) continue;
                    uint32_t c = g.logo[sy * g.logoW + sx];
                    for (int k = 0; k < 4; k++) acc[k] += wgt * ((c >> (k * 8)) & 255);
                    wsum += wgt;
                }
            uint32_t o = 0;
            for (int k = 0; k < 4; k++) o |= (uint32_t)std::clamp(acc[k] / std::max(wsum, 1e-6f) + 0.5f, 0.0f, 255.0f) << (k * 8);
            px[y * dw + x] = o;
        }
    return hb;
}
void BlendLogo(HDC dc, int slot, int x, int y, int dw, int dh)
{
    HBITMAP lb = ScaleLogo(slot, dw, dh); if (!lb) return;
    HDC mem = CreateCompatibleDC(dc); HGDIOBJ old = SelectObject(mem, lb);
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(dc, x, y, dw, dh, mem, 0, 0, dw, dh, bf);
    SelectObject(mem, old); DeleteDC(mem);
}
void DrawSection(HDC dc, RECT rc, const wchar_t* t)
{
    int w = rc.right - rc.left;
    RECT tr{ 0, 0, w, S(26) };
    Text(dc, t, tr, kAccent2, g.fontHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE, S(2));
    LineC(dc, 0, S(30), w, S(30), kLine);
    for (int k = 0; k < 3; k++) LineC(dc, w - S(6) - k * S(8), S(27), w - S(10) - k * S(8), S(33), kAccentDim);
}
// Portada de Inicio: portada.png cubriendo el ancho (recorte centrado), oscurecida a la izquierda, con el logo encima
void DrawBanner(HDC dc, RECT rc)
{
    int w = rc.right - rc.left, h = rc.bottom - rc.top, band = S(48), ph = h - band;
    BITMAPINFO bi{}; bi.bmiHeader = { sizeof(BITMAPINFOHEADER), w, -ph, 1, 32, BI_RGB };
    uint32_t* pa = nullptr; HBITMAP ba = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&pa, nullptr, 0);
    if (ba && pa)
    {
        HDC da = CreateCompatibleDC(dc); HGDIOBJ oa = SelectObject(da, ba);
        if (g.banner)
        {
            HDC mem = CreateCompatibleDC(dc); HGDIOBJ old = SelectObject(mem, g.banner);
            SetStretchBltMode(da, HALFTONE); SetBrushOrgEx(da, 0, 0, nullptr);
            int sw = g.bannerW, sh = g.bannerW * ph / std::max(1, w);
            if (sh > g.bannerH) { sh = g.bannerH; sw = g.bannerH * w / std::max(1, ph); }
            StretchBlt(da, 0, 0, w, ph, mem, (g.bannerW - sw) / 2, (g.bannerH - sh) / 2, sw, sh, SRCCOPY);
            SelectObject(mem, old); DeleteDC(mem);
        }
        else for (int y = 0; y < ph; y++) { RECT r{ 0, y, w, y + 1 }; float f = (float)y / ph; FillR(da, r, Mix(RGB(30, 30, 36), RGB(90, 14, 20), f * f)); }
        GdiFlush();
        for (int x = 0; x < w * 60 / 100; x++)   // oscurecido hacia la izquierda (donde va el logo)
        {
            float f = 1.0f - 0.70f * std::pow(1.0f - (float)x / (w * 60 / 100), 1.3f);
            for (int y = 0; y < ph; y++)
            {
                uint32_t c = pa[y * w + x];
                pa[y * w + x] = ((uint32_t)(((c >> 16) & 255) * f) << 16) | ((uint32_t)(((c >> 8) & 255) * f) << 8) | (uint32_t)((c & 255) * f);
            }
        }
        BitBlt(dc, 0, 0, w, ph, da, 0, 0, SRCCOPY);
        SelectObject(da, oa); DeleteDC(da);
    }
    if (ba) DeleteObject(ba);
    if (!g.logo.empty())
    {
        int lh = ph * 62 / 100, lw = g.logoW * lh / std::max(1, g.logoH);
        BlendLogo(dc, 1, S(34), (ph - lh) / 2, lw, lh);
    }
    else
    {
        RECT t1{ S(34), 0, w, ph };
        Text(dc, L"NINJA GAIDEN II", t1, kText, g.fontTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE, S(3));
    }
    RECT bd{ 0, ph, w, h }; FillR(dc, bd, RGB(14, 14, 17));
    LineC(dc, 0, ph, w, ph, kAccent);
    RECT nm{ S(20), ph, w - S(20), h };
    Text(dc, L"NINJA GAIDEN II", nm, kText, g.fontHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE, S(3));
    Text(dc, L"544307D5  ·  XBOX 360  ·  RECOMPILADO PARA PC", nm, kAccentDim, g.fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

// ---- selector < valor > ----
struct SelData { std::vector<std::wstring> items; int sel = 0, hot = 0; bool tracking = false; };
LRESULT CALLBACK SelProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    SelData* d = (SelData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    auto step = [&](int dir) {
        if (!d || d->items.empty()) return;
        int n = (int)d->items.size(); d->sel = (d->sel + dir + n) % n;
        InvalidateRect(h, nullptr, FALSE);
        SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), CBN_SELCHANGE), (LPARAM)h);
    };
    switch (m)
    {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)new SelData()); break;
    case WM_NCDESTROY: delete d; SetWindowLongPtrW(h, GWLP_USERDATA, 0); break;
    case CB_ADDSTRING: d->items.push_back((const wchar_t*)l); InvalidateRect(h, nullptr, FALSE); return (LRESULT)d->items.size() - 1;
    case CB_SETCURSEL: d->sel = d->items.empty() ? 0 : std::clamp((int)w, 0, (int)d->items.size() - 1); InvalidateRect(h, nullptr, FALSE); return d->sel;
    case CB_GETCURSEL: return d->items.empty() ? -1 : d->sel;
    case WM_GETDLGCODE: return DLGC_WANTARROWS;
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: { SetFocus(h); RECT rc; GetClientRect(h, &rc); step(GET_X_LPARAM(l) < rc.right / 2 ? -1 : 1); return 0; }
    case WM_MOUSEWHEEL: step(GET_WHEEL_DELTA_WPARAM(w) > 0 ? -1 : 1); return 0;
    case WM_KEYDOWN: if (w == VK_LEFT) step(-1); else if (w == VK_RIGHT || w == VK_SPACE) step(1); return 0;
    case WM_MOUSEMOVE:
    {
        RECT rc; GetClientRect(h, &rc); int hot = GET_X_LPARAM(l) < rc.right / 2 ? -1 : 1;
        if (!d->tracking) { TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, h, 0 }; TrackMouseEvent(&t); d->tracking = true; }
        if (hot != d->hot) { d->hot = hot; InvalidateRect(h, nullptr, FALSE); }
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return 0;
    }
    case WM_MOUSELEAVE: d->hot = 0; d->tracking = false; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bm = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ ob = SelectObject(mem, bm);
        FillR(mem, rc, kPanel);
        bool focus = GetFocus() == h;
        RECT box{ 0, 0, rc.right - 1, rc.bottom - 1 };
        Chamfer(mem, box, S(6), d->hot ? kPanelHi : kInput, focus || d->hot ? kAccent : kLine);
        int aw = S(30);
        RECT left{ 0, 0, aw, rc.bottom }, right{ rc.right - aw, 0, rc.right, rc.bottom }, mid{ aw, 0, rc.right - aw, rc.bottom };
        Text(mem, L"◀", left, d->hot < 0 ? kAccent2 : kAccentDim, g.fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        Text(mem, L"▶", right, d->hot > 0 ? kAccent2 : kAccentDim, g.fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (!d->items.empty()) Text(mem, d->items[d->sel].c_str(), mid, kText, g.font, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bm); DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

void DrawItem(DRAWITEMSTRUCT* d)
{
    HDC dc = d->hDC; RECT rc = d->rcItem;
    if (d->CtlType == ODT_STATIC)
    {
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        HDC mem = CreateCompatibleDC(dc); HBITMAP bm = CreateCompatibleBitmap(dc, w, h); HGDIOBJ ob = SelectObject(mem, bm);
        RECT lr{ 0, 0, w, h }; FillR(mem, lr, kPanel);
        if (d->CtlID == ID_BANNER) DrawBanner(mem, lr);
        else { wchar_t t[128]; GetWindowTextW(d->hwndItem, t, 128); DrawSection(mem, lr, t); }
        BitBlt(dc, rc.left, rc.top, w, h, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bm); DeleteDC(mem);
        return;
    }
    if (d->CtlType != ODT_BUTTON) return;
    HWND h = d->hwndItem;
    wchar_t t[128]; GetWindowTextW(h, t, 128);
    bool pressed = (d->itemState & ODS_SELECTED) != 0, focus = (d->itemState & ODS_FOCUS) != 0, disabled = (d->itemState & ODS_DISABLED) != 0;
    int w = rc.right - rc.left, hh = rc.bottom - rc.top;
    HDC mem = CreateCompatibleDC(dc); HBITMAP bm = CreateCompatibleBitmap(dc, w, hh); HGDIOBJ ob = SelectObject(mem, bm);
    RECT r{ 0, 0, w, hh }, box{ 0, 0, w - 1, hh - 1 };
    bool isTab = d->CtlID >= ID_TAB0 && d->CtlID < ID_TAB0 + kTabs, onBar = d->CtlID == ID_SAVE || d->CtlID == ID_PLAY;
    FillR(mem, r, isTab ? kSide : onBar ? kBar : kPanel);
    auto tg = g.toggles.find(h);
    if (isTab)
    {
        int i = d->CtlID - ID_TAB0; bool sel = i == g.tab;
        if (sel) Chamfer(mem, box, S(10), kPanelHi, kAccent);
        else if (pressed) Chamfer(mem, box, S(10), kPanel, kLine);
        std::wstring up = t; CharUpperBuffW(up.data(), (DWORD)up.size());
        RECT tr{ S(22), 0, w, hh };
        Text(mem, up.c_str(), tr, sel ? kText : kDim, g.fontTab, DT_LEFT | DT_VCENTER | DT_SINGLELINE, S(2));
        if (sel) LineC(mem, S(10), S(13), S(10), hh - S(13), kAccent, S(3));
    }
    else if (tg != g.toggles.end())
    {
        bool on = tg->second;
        Chamfer(mem, box, S(6), on ? kPanelHi : kInput, on || focus ? kAccent : kLine);
        RECT knob = on ? RECT{ box.right - S(36), S(4), box.right - S(4), box.bottom - S(4) } : RECT{ S(4), S(4), S(36), box.bottom - S(4) };
        Chamfer(mem, knob, S(4), on ? kAccent : kLine, CLR_NONE);
        RECT lb = box; if (on) lb.right -= S(38); else lb.left += S(38);
        Text(mem, on ? L"SÍ" : L"NO", lb, on ? kAccent2 : kDim, g.fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE, S(1));
    }
    else if (d->CtlID == ID_PLAY)
    {
        Chamfer(mem, box, S(12), disabled ? kLine : pressed ? RGB(160, 16, 28) : kAccent, CLR_NONE);
        Text(mem, L"JUGAR", r, kPlayText, g.fontPlay, DT_CENTER | DT_VCENTER | DT_SINGLELINE, S(4));
    }
    else
    {
        Chamfer(mem, box, S(8), pressed ? kPanelHi : kInput, focus ? kAccent : kLine);
        std::wstring up = t; if (onBar) CharUpperBuffW(up.data(), (DWORD)up.size());
        Text(mem, up.c_str(), r, disabled ? kDim : pressed ? kAccent2 : kText, onBar ? g.fontTab : g.fontBold, DT_CENTER | DT_VCENTER | DT_SINGLELINE, onBar ? S(2) : 0);
    }
    BitBlt(dc, rc.left, rc.top, w, hh, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob); DeleteObject(bm); DeleteDC(mem);
}
void PaintMain(HDC dc0)
{
    RECT rc; GetClientRect(g.wnd, &rc);
    HDC dc = CreateCompatibleDC(dc0); HBITMAP bm = CreateCompatibleBitmap(dc0, rc.right, rc.bottom); HGDIOBJ ob = SelectObject(dc, bm);
    FillR(dc, rc, kBg);
    for (int y = 0; y < rc.bottom; y += S(3)) LineC(dc, 0, y, rc.right, y, kScan);
    int side = S(kSideW);
    RECT sb{ 0, 0, side, rc.bottom }; FillR(dc, sb, kSide);
    LineC(dc, side, 0, side, rc.bottom, kLine);
    if (!g.logo.empty())
    {   // logo.png con transparencia en la barra lateral
        RECT bd{ S(16), S(18), side - S(16), S(160) };
        int bw = bd.right - bd.left, bh = bd.bottom - bd.top, dw = bw, dh = g.logoH * bw / std::max(1, g.logoW);
        if (dh > bh) { dh = bh; dw = g.logoW * bh / std::max(1, g.logoH); }
        BlendLogo(dc, 0, bd.left + (bw - dw) / 2, bd.top + (bh - dh) / 2, dw, dh);
    }
    else
    {
        RECT t1{ S(22), S(54), side - S(8), S(110) };
        Text(dc, L"NINJA GAIDEN II", t1, kText, g.fontHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE, S(2));
    }
    LineC(dc, S(24), S(176), side - S(24), S(176), kLine);
    RECT t3{ S(24), S(182), side - S(8), S(204) };
    Text(dc, L"PORT PC", t3, kAccent2, g.fontHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE, S(4));
    RECT t4{ S(24), S(204), side - S(8), S(222) };
    Text(dc, L"recompilación estática  ·  DirectX 12", t4, kDim, g.fontSmall);
    RECT note{ S(24), S(kH - 70), side - S(12), S(kH - 16) };
    Text(dc, L"En el juego: F11 o Alt+Intro\npantalla completa  ·  F3 FPS", note, kDim, g.fontSmall, DT_LEFT | DT_TOP | DT_WORDBREAK);
    RECT panel{ S(kX0 - 14), S(16), S(kXR + 14), S(kPanelBottom) };
    Chamfer(dc, panel, S(18), kPanel, kLine);
    for (auto& [p, fr] : g.frames)
        if (p == g.tab || p < 0) Chamfer(dc, RECT{ S(fr.left), S(fr.top), S(fr.right), S(fr.bottom) }, S(6), kInput, kLine);
    RECT bar{ side + 1, S(kBarTop), rc.right, rc.bottom }; FillR(dc, bar, kBar);
    LineC(dc, side, S(kBarTop), rc.right, S(kBarTop), kLine);
    BitBlt(dc0, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc);
}
void UpdateHome();
std::string DataDirL();
// Sesion 26: estado de la actualizacion del titulo (importada desde la carpeta de DLC al arrancar)
void UpdateTuInfo()
{
    std::string tu = FindTitleUpdate(DataDirL());
    bool exe = GetFileAttributesA((ExeDirL() + "\\NG2TU.exe").c_str()) != INVALID_FILE_ATTRIBUTES;
    const wchar_t* t = !tu.empty() ? (exe ? L"Instalada: se usa al jugar (menú MISSIONS). Desactívala para jugar la versión del disco."
                                         : L"Instalada, pero falta NG2TU.exe junto al juego.")
                                   : L"No instalada: copia su paquete (archivo TU_... de tu consola) en la carpeta de DLC y pulsa JUGAR.";
    SetWindowTextW(Item(ID_TUINFO), t);
}
void UpdatePads();
void ShowTab(int t)
{
    g.tab = t;
    for (int p = 0; p < kTabs; p++) for (HWND h : g.pages[p]) ShowWindow(h, p == t ? SW_SHOW : SW_HIDE);
    InvalidateRect(g.wnd, nullptr, TRUE);
    for (int i = 0; i < kTabs; i++) InvalidateRect(Item(ID_TAB0 + i), nullptr, TRUE);
    if (t == kHome) UpdateHome();
    if (t == 2) UpdatePads();
}
void UpdateDlcCount()
{
    std::wstring dir = GetText(Item(ID_DLC)); int n = 0;
    WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (f != INVALID_HANDLE_VALUE)
    {
        do
        {   // los paquetes de 360 no tienen extension; los .txt/.md no cuentan
            const wchar_t* ext = wcsrchr(fd.cFileName, L'.');
            bool doc = ext && (!_wcsicmp(ext, L".txt") || !_wcsicmp(ext, L".md") || !_wcsicmp(ext, L".xcd"));
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !doc) n++;
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    wchar_t t[160];
    if (dir.empty()) wcscpy(t, L"Sin carpeta de DLC");
    else if (f == INVALID_HANDLE_VALUE) wcscpy(t, L"La carpeta no existe");
    else swprintf(t, 160, L"%d paquete%s en la carpeta (se importan al arrancar, una sola vez)", n, n == 1 ? L"" : L"s");
    SetWindowTextW(Item(ID_DLCCOUNT), t);
}
void BrowseIso()
{
    wchar_t file[MAX_PATH] = {}; std::wstring cur = GetText(Item(ID_ISO)); wcsncpy(file, cur.c_str(), MAX_PATH - 1);
    OPENFILENAMEW o{}; o.lStructSize = sizeof(o); o.hwndOwner = g.wnd;
    o.lpstrFilter = L"ISO de Xbox 360 (*.iso)\0*.iso\0Todos los archivos\0*.*\0"; o.lpstrFile = file; o.nMaxFile = MAX_PATH;
    o.lpstrTitle = L"Selecciona el ISO de Ninja Gaiden II (Xbox 360)"; o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&o)) SetWindowTextW(Item(ID_ISO), file);
}
void BrowseFolder(int editId)
{
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return;
    DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    d->SetTitle(L"Carpeta con los paquetes de DLC");
    if (SUCCEEDED(d->Show(g.wnd)))
    {
        IShellItem* it = nullptr;
        if (SUCCEEDED(d->GetResult(&it))) { PWSTR p = nullptr; if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { SetWindowTextW(Item(editId), p); CoTaskMemFree(p); } it->Release(); }
    }
    d->Release();
}
std::string DataDirL() { std::string d = g.ini.Get("datos", "userdata"); return (d.size() > 1 && d[1] == ':') ? d : ExeDirL() + "\\" + d; }
void UpdateSaveInfo()
{
    std::string dir = DataDirL() + "\\content"; int n = 0;
    WIN32_FIND_DATAA fd; HANDLE f = FindFirstFileA((dir + "\\00000001_*").c_str(), &fd);
    if (f != INVALID_HANDLE_VALUE) { do { if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) n++; } while (FindNextFileA(f, &fd)); FindClose(f); }
    wchar_t t[200]; swprintf(t, 200, L"%d archivo%s de partida en %hs", n, n == 1 ? L"" : L"s", dir.c_str());
    SetWindowTextW(Item(ID_SAVEINFO), t);
}
void UpdatePads()
{
    std::wstring s;
    for (DWORD i = 0; i < 4; i++) { XINPUT_STATE st{}; if (XInputGetState(i, &st) == ERROR_SUCCESS) { wchar_t t[48]; swprintf(t, 48, L"Mando XInput %lu conectado   ", i + 1); s += t; } }
    uint16_t b; uint8_t lt, rt; int16_t sticks[4];
    if (PsPadGetState(b, lt, rt, sticks)) s += L"Mando de PlayStation conectado";
    if (s.empty()) s = L"No hay mandos conectados (se puede jugar con el teclado)";
    SetWindowTextW(Item(ID_PADSTATUS), s.c_str());
}

// ---- crear la ventana a partir de ng2.ini ----
void BuildUi()
{
    IniDoc& ini = g.ini;
    auto flag = [&](const char* k, bool def) { std::string v = ini.Get(k, def ? "1" : "0"); return v != "0"; };
    const wchar_t* tabs[kTabs] = { L"Juego", L"Gráficos", L"Controles", L"Audio", L"Ayuda", L"Inicio" };
    for (int i = 0; i < kTabs; i++) Button(-1, tabs[i], 16, 236 + TabPos(i) * 48, kSideW - 32, 40, ID_TAB0 + i);
    Label(-1, L"", kX0, kBarTop + 26, 380, 22, ID_SAVED, true);
    Button(-1, L"Guardar", kW - 390, kBarTop + 16, 140, 42, ID_SAVE);
    Button(-1, L"JUGAR", kW - 234, kBarTop + 12, 200, 50, ID_PLAY);

    std::string pn = ini.Get("panoramico", "16:9");
    const std::vector<std::wstring> panoItems = { L"16:9 (como la consola)", L"Panorámica del monitor", L"21:9", L"32:9" };
    int panoSel = pn == "auto" ? 1 : pn == "21:9" ? 2 : pn == "32:9" ? 3 : 0;

    // ---- Inicio ----
    {
        int y = kY0;
        Make(kHome, L"STATIC", L"", SS_OWNERDRAW, kX0, y, kXR - kX0, 244, ID_BANNER);
        y = Section(kHome, y + 258, L"AJUSTES RÁPIDOS");
        Row(kHome, y, L"Proporción de la imagen", L"21:9 y 32:9 amplían la vista a los lados; la interfaz queda en el centro."); ComboRight(kHome, y, ID_Q_PANO, panoItems, panoSel); y += 44;
        Row(kHome, y, L"Pantalla completa", L"Sin bordes, en el monitor de la ventana. F11 la alterna en el juego."); ToggleRight(kHome, y, ID_Q_FULL, flag("pantalla_completa", false)); y += 44;
        Row(kHome, y, L"Contador de FPS", L"FPS, tiempo por frame, CPU, memoria y audio. F3 lo muestra y oculta."); ToggleRight(kHome, y, ID_Q_FPS, flag("mostrar_fps", false)); y += 48;
        g.frames.push_back({ kHome, RECT{ kX0, y, kXR, y + 62 } });
        Label(kHome, L"", kX0 + 16, y + 9, kXR - kX0 - 32, 20, ID_HOME_ISO);
        Label(kHome, L"Teclado: WASD moverse  ·  IJKL cámara  ·  Espacio saltar  ·  F golpe  ·  R golpe fuerte  ·  E proyectil  ·  Q bloquear  ·  Intro pausa",
              kX0 + 16, y + 34, kXR - kX0 - 32, 18, ID_HOME_KEYS, true, g.fontSmall);
    }
    // ---- Juego ----
    {
        int y = Section(0, kY0, L"ARCHIVOS DEL JUEGO");
        Row(0, y, L"ISO del juego (Xbox 360)", L"Ninja Gaiden II (World) — título 544307D5"); y += 42;
        Edit(0, W(ini.Get("iso")), kX0, y, kXR - kX0 - 140, ID_ISO);
        Button(0, L"Examinar...", kXR - 130, y, 130, 30, ID_ISO_BROWSE); y += 42;
        Row(0, y, L"Carpeta de DLC", nullptr, 520, ID_DLCCOUNT); y += 42;
        Edit(0, W(ini.Get("dlc", "dlc")), kX0, y, kXR - kX0 - 140, ID_DLC);
        Button(0, L"Examinar...", kXR - 130, y, 130, 30, ID_DLC_BROWSE); y += 42;
        Row(0, y, L"Saltar el vídeo de logos al arrancar", L"Entra directo a la pantalla de título."); ToggleRight(0, y, ID_SKIPINTRO, flag("saltar_intro", false)); y += 42;
        Row(0, y, L"Guardar partidas", L"Con perfil el juego guarda la partida y los ajustes; sin perfil no se guarda nada."); ToggleRight(0, y, ID_PROFILE, flag("perfil", true)); y += 42;
        Row(0, y, L"Actualización del título #2 (Mission Mode)", nullptr, 620, ID_TUINFO); ToggleRight(0, y, ID_TU, flag("actualizacion", true)); y += 46;
        y = Section(0, y, L"PARTIDAS");
        Label(0, L"", kX0, y, kXR - kX0, 20, ID_SAVEINFO, true, g.fontSmall); y += 26;
        Button(0, L"Abrir carpeta", kX0, y, 160, 32, ID_SAVEOPEN); y += 48;
        y = Section(0, y, L"LAUNCHER");
        Row(0, y, L"Mostrar este launcher al iniciar", L"Si lo desactivas, mantén Mayús al abrir el juego para verlo."); ToggleRight(0, y, ID_SHOWLAUNCHER, flag("mostrar_launcher", true));
    }
    // ---- Graficos ----
    {
        int ws = 0; std::string v = ini.Get("ventana", "1280x720");
        for (int i = 0; i < kNumWinSizes; i++) if (v == kWinSizesIni[i]) ws = i;
        std::string an = ini.Get("anisotropico", "16");
        int y = Section(1, kY0, L"IMAGEN");
        Row(1, y, L"Proporción de la imagen", L"Más campo de visión a los lados en monitores panorámicos."); ComboRight(1, y, ID_PANO, panoItems, panoSel); y += 42;
        Row(1, y, L"Filtrado de texturas", L"Texturas nítidas en ángulo (suelos, paredes)."); ComboRight(1, y, ID_ANISO, { L"Anisotrópico 16x", L"Anisotrópico 8x", L"Anisotrópico 4x", L"Lineal (consola)" },
              an == "8" ? 1 : an == "4" ? 2 : an == "0" || an == "1" ? 3 : 0); y += 42;
        Row(1, y, L"Desenfoque de movimiento", L"Como en la consola."); ToggleRight(1, y, ID_MBLUR, flag("desenfoque_movimiento", true)); y += 42;
        Row(1, y, L"Grano de película", L"Ruido fino del último pase de la consola (en PC se ve como rayas)."); ToggleRight(1, y, ID_GRAIN, flag("grano_pelicula", false)); y += 44;
        y = Section(1, y, L"PANTALLA");
        Row(1, y, L"Pantalla completa al arrancar", L"Sin bordes. F11 o Alt+Intro la alternan en el juego."); ToggleRight(1, y, ID_FULLSCREEN, flag("pantalla_completa", false)); y += 42;
        Row(1, y, L"Tamaño de la ventana", L"Solo en modo ventana; en pantalla completa se usa el monitor.");
        ComboRight(1, y, ID_WINSIZE, std::vector<std::wstring>(kWinSizes, kWinSizes + kNumWinSizes), ws); y += 42;
        Row(1, y, L"Escalado de la imagen", L"Cómo se adapta la imagen a la ventana."); ComboRight(1, y, ID_SCALE, { L"Nítido (Catmull-Rom)", L"Suave (bilineal)" }, ini.Get("escalado") == "bilineal" ? 1 : 0); y += 42;
        Row(1, y, L"Sincronización vertical (VSync)", L"Evita el desgarro de la imagen."); ToggleRight(1, y, ID_VSYNC, flag("vsync", true)); y += 42;
        Row(1, y, L"Contador de FPS", L"Se muestra y oculta con F3."); ToggleRight(1, y, ID_FPS, flag("mostrar_fps", false));
    }
    // ---- Controles ----
    {
        int y = Section(2, kY0, L"MANDOS");
        Label(2, L"", kX0, y, kXR - kX0, 22, ID_PADSTATUS, false, g.fontBold); y += 30;
        Row(2, y, L"Vibración", L"El mando vibra con los golpes, como en la consola."); ToggleRight(2, y, ID_VIBRA, flag("vibracion", true)); y += 46;
        Label(2, L"Mandos de Xbox (XInput) y de PlayStation (DualSense, DualShock 4) por USB o Bluetooth: como en la consola.", kX0, y, kXR - kX0, 20, 0, true, g.fontSmall); y += 30;
        y = Section(2, y, L"TECLADO");
        const wchar_t* keys[][2] = { { L"W A S D", L"Moverse (stick izquierdo)" }, { L"I J K L", L"Cámara (stick derecho)" }, { L"Espacio", L"Saltar (A)" },
                                     { L"F", L"Golpe rápido (X)" }, { L"R", L"Golpe fuerte (Y)" }, { L"E", L"Proyectil (B)" }, { L"Q", L"Bloquear (RT)" },
                                     { L"Z", L"LT" }, { L"C / V", L"LB / RB (ninpo)" }, { L"Flechas", L"Cruceta / menús" }, { L"Intro / Retroceso", L"START (pausa) / BACK" },
                                     { L"Esc", L"B (cancelar en menús)" } };
        for (int i = 0; i < 12; i++)
        {
            int col = i % 2, row = i / 2, x = kX0 + col * 400;
            Label(2, keys[i][0], x, y + row * 28, 150, 22, 0, false, g.fontBold);
            Label(2, keys[i][1], x + 150, y + row * 28, 240, 22, 0, true);
        }
    }
    // ---- Audio ----
    {
        std::string la = ini.Get("latencia_audio", "normal");
        int y = Section(3, kY0, L"SONIDO");
        Row(3, y, L"Retraso del audio", L"Baja = menos retraso; Segura = sin cortes en equipos lentos.");
        ComboRight(3, y, ID_AUDIOLAT, { L"Baja (25 ms)", L"Normal (40 ms)", L"Segura (80 ms)" }, la == "baja" ? 0 : la == "segura" ? 2 : 1); y += 46;
        Label(3, L"El sonido sale por el dispositivo de audio predeterminado de Windows (estéreo, 48 kHz).", kX0, y, kXR - kX0, 20, 0, true, g.fontSmall);
    }
    // ---- Ayuda ----
    {
        int y = Section(4, kY0, L"AYUDA");
        const wchar_t* help =
            L"• Necesitas tu propio ISO de Ninja Gaiden II para Xbox 360 (versión World). Este paquete no incluye archivos del juego.\n\n"
            L"• DLC: copia tus paquetes de Xbox 360 (archivos LIVE/CON) en la carpeta de DLC. Funcionan los trajes Biometal, Fiend y "
            L"Shadow Walker Hayabusa.\n\n"
            L"• Mission Mode: copia también en la carpeta de DLC el paquete de la actualización del título #2 (archivo TU_... de tu "
            L"consola) junto con el DLC de Mission Mode. Aparece la opción MISSIONS en el menú principal.\n\n"
            L"• En el juego: F11 o Alt+Intro pantalla completa, F3 contador de FPS, F12 captura de pantalla (shot_9xx.bmp).\n\n"
            L"• Las cinemáticas van a 30 fps como en la consola; el juego, a 60.\n\n"
            L"• Si algo falla, el registro de la última partida está en ng2.log junto al ejecutable.\n\n"
            L"• Imágenes del launcher: SteamGridDB (portada.png y logo.png junto al exe; se pueden cambiar por otras).";
        Label(4, help, kX0, y, kXR - kX0, 420, 0, false);
    }
}
// PNG (u otro formato de WIC) -> pixeles de 32 bits; premul = alfa premultiplicado (para el logo)
bool DecodeImage(const std::vector<uint8_t>& d, int& w, int& h, std::vector<uint32_t>& px, bool premul)
{
    bool ok = false; IWICImagingFactory* f = nullptr; IWICStream* s = nullptr; IWICBitmapDecoder* dec = nullptr; IWICBitmapFrameDecode* fr = nullptr; IWICFormatConverter* cv = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) && SUCCEEDED(f->CreateStream(&s))
        && SUCCEEDED(s->InitializeFromMemory((BYTE*)d.data(), (DWORD)d.size())) && SUCCEEDED(f->CreateDecoderFromStream(s, nullptr, WICDecodeMetadataCacheOnLoad, &dec))
        && SUCCEEDED(dec->GetFrame(0, &fr)) && SUCCEEDED(f->CreateFormatConverter(&cv))
        && SUCCEEDED(cv->Initialize(fr, premul ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
    {
        UINT W = 0, H = 0; cv->GetSize(&W, &H);
        px.resize((size_t)W * H);
        if (SUCCEEDED(cv->CopyPixels(nullptr, W * 4, W * H * 4, (BYTE*)px.data()))) { w = (int)W; h = (int)H; ok = true; }
    }
    for (IUnknown* u : std::initializer_list<IUnknown*>{ cv, fr, dec, s, f }) if (u) u->Release();
    return ok;
}
std::vector<uint8_t> ReadAll(const std::string& path)
{
    std::vector<uint8_t> d; FILE* f = fopen(path.c_str(), "rb"); if (!f) return d;
    uint8_t buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && d.size() < (64u << 20)) d.insert(d.end(), buf, buf + n);
    fclose(f); return d;
}
void UpdateHome()
{
    std::wstring iso = GetText(Item(ID_ISO));
    g.isoMissing = iso.empty() || GetFileAttributesW(iso.c_str()) == INVALID_FILE_ATTRIBUTES;
    std::wstring t = g.isoMissing ? L"Falta el ISO del juego: elígelo en la página JUEGO para poder jugar."
                                  : L"ISO del juego: " + iso.substr(iso.find_last_of(L"\\/") + 1);
    SetWindowTextW(Item(ID_HOME_ISO), t.c_str());
}
// Valores de la ventana -> ng2.ini
bool Collect(bool requireIso)
{
    IniDoc& ini = g.ini;
    std::string iso = A(GetText(Item(ID_ISO)));
    if (requireIso && (iso.empty() || GetFileAttributesA(iso.c_str()) == INVALID_FILE_ATTRIBUTES))
    {
        ShowTab(0);
        MessageBoxW(g.wnd, L"Elige el ISO de Ninja Gaiden II (Xbox 360) para poder jugar.", L"Ninja Gaiden II", MB_ICONINFORMATION);
        return false;
    }
    ini.Set("iso", iso);
    ini.Set("dlc", A(GetText(Item(ID_DLC))));
    ini.Set("saltar_intro", Toggled(ID_SKIPINTRO) ? "1" : "0");
    ini.Set("perfil", Toggled(ID_PROFILE) ? "1" : "0");
    ini.Set("actualizacion", Toggled(ID_TU) ? "1" : "0");
    ini.Set("mostrar_launcher", Toggled(ID_SHOWLAUNCHER) ? "1" : "0");
    ini.Set("pantalla_completa", Toggled(ID_FULLSCREEN) ? "1" : "0");
    ini.Set("ventana", kWinSizesIni[std::clamp(Sel(ID_WINSIZE), 0, kNumWinSizes - 1)]);
    ini.Set("escalado", Sel(ID_SCALE) == 1 ? "bilineal" : "catmull-rom");
    ini.Set("vsync", Toggled(ID_VSYNC) ? "1" : "0");
    ini.Set("mostrar_fps", Toggled(ID_FPS) ? "1" : "0");
    ini.Set("desenfoque_movimiento", Toggled(ID_MBLUR) ? "1" : "0");
    ini.Set("grano_pelicula", Toggled(ID_GRAIN) ? "1" : "0");
    ini.Set("vibracion", Toggled(ID_VIBRA) ? "1" : "0");
    { static const char* an[] = { "16", "8", "4", "0" }; ini.Set("anisotropico", an[std::clamp(Sel(ID_ANISO), 0, 3)]); }
    { static const char* pp[] = { "16:9", "auto", "21:9", "32:9" }; ini.Set("panoramico", pp[std::clamp(Sel(ID_PANO), 0, 3)]); }
    { static const char* lat[] = { "baja", "normal", "segura" }; ini.Set("latencia_audio", lat[std::clamp(Sel(ID_AUDIOLAT), 0, 2)]); }
    return true;
}
LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); PaintMain(dc); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_DRAWITEM: DrawItem((DRAWITEMSTRUCT*)l); return TRUE;
    case WM_CTLCOLORSTATIC:
    {
        HDC dc = (HDC)w; HWND c = (HWND)l;
        bool bar = c == Item(ID_SAVED), card = c == Item(ID_HOME_ISO) || c == Item(ID_HOME_KEYS);
        COLORREF col = g.dimLabels.count(c) ? kDim : kText;
        if (bar || (c == Item(ID_HOME_ISO) && g.isoMissing)) col = kAccent2;
        SetTextColor(dc, col); SetBkColor(dc, bar ? kBar : card ? kInput : kPanel);
        return (LRESULT)(bar ? g.brBar : card ? g.brInput : g.brPanel);
    }
    case WM_CTLCOLOREDIT: { HDC dc = (HDC)w; SetTextColor(dc, kText); SetBkColor(dc, kInput); return (LRESULT)g.brInput; }
    case WM_CTLCOLORBTN: return (LRESULT)g.brPanel;
    case WM_TIMER:
        g.ticks++;
        if (g.tab == 2 && g.ticks % 30 == 0) UpdatePads();
        if (g.savedT && GetTickCount() - g.savedT > 2500) { g.savedT = 0; SetWindowTextW(Item(ID_SAVED), L""); }
        return 0;
    case WM_COMMAND:
    {
        int id = LOWORD(w), code = HIWORD(w); HWND c = (HWND)l;
        if (code == BN_CLICKED && g.toggles.count(c))
        {
            g.toggles[c] = !g.toggles[c]; InvalidateRect(c, nullptr, TRUE);
            static const int pairs[][2] = { { ID_Q_FULL, ID_FULLSCREEN }, { ID_Q_FPS, ID_FPS } };   // Inicio repite estas opciones
            for (auto& pr : pairs) for (int k = 0; k < 2; k++) if (id == pr[k]) { HWND o = Item(pr[1 - k]); g.toggles[o] = g.toggles[c]; InvalidateRect(o, nullptr, TRUE); }
            return 0;
        }
        if (id >= ID_TAB0 && id < ID_TAB0 + kTabs && code == BN_CLICKED) { ShowTab(id - ID_TAB0); return 0; }
        switch (id)
        {
        case ID_PLAY: if (Collect(true)) { g.ini.Save(ExeDirL() + "\\ng2.ini"); g.result = true; DestroyWindow(h); } return 0;
        case ID_SAVE: if (Collect(false)) { g.ini.Save(ExeDirL() + "\\ng2.ini"); SetWindowTextW(Item(ID_SAVED), L"Configuración guardada"); g.savedT = GetTickCount(); } return 0;
        case ID_ISO_BROWSE: BrowseIso(); return 0;
        case ID_DLC_BROWSE: BrowseFolder(ID_DLC); UpdateDlcCount(); return 0;
        case ID_DLC: if (code == EN_CHANGE) UpdateDlcCount(); return 0;
        case ID_ISO: if (code == EN_CHANGE) UpdateHome(); return 0;
        case ID_SAVEOPEN: { std::string d = DataDirL(); CreateDirectoryA(d.c_str(), nullptr); ShellExecuteA(h, "open", d.c_str(), nullptr, nullptr, SW_SHOWNORMAL); } return 0;
        case ID_Q_PANO: case ID_PANO:
            if (code == CBN_SELCHANGE) SendMessageW(Item(id == ID_Q_PANO ? ID_PANO : ID_Q_PANO), CB_SETCURSEL, Sel(id), 0);
            return 0;
        }
        break;
    }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: g.done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
} // namespace

// ¿Abrir el launcher? --jugar lo salta, --launcher lo fuerza, Mayus pulsada al abrir lo fuerza; si no, mostrar_launcher (1 por defecto)
bool LauncherWanted(int argc, char** argv)
{
    if (getenv("NG2_NOLAUNCHER")) return false;
    for (int i = 1; i < argc; i++) { if (!_stricmp(argv[i], "--jugar")) return false; if (!_stricmp(argv[i], "--launcher")) return true; }
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) return true;
    IniDoc ini; ini.Load(ExeDirL() + "\\ng2.ini");
    return ini.Get("mostrar_launcher", "1") != "0";
}
// Muestra el launcher. true = JUGAR (ng2.ini ya guardado), false = salir
bool RunLauncherUI()
{
    DPI_AWARENESS_CONTEXT oldDpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES }; InitCommonControlsEx(&icc);
    g.inst = GetModuleHandleW(nullptr);
    g.dpi = (int)GetDpiForSystem(); if (g.dpi < 96) g.dpi = 96;
    g.ini.Load(ExeDirL() + "\\ng2.ini");
    auto font = [](int pt, int weight, const wchar_t* face) { return CreateFontW(-S(pt), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, face); };
    const wchar_t* face = L"Bahnschrift";
    g.font = font(14, FW_NORMAL, face); g.fontBold = font(14, FW_SEMIBOLD, face); g.fontSmall = font(12, FW_NORMAL, face);
    g.fontTitle = font(40, FW_BOLD, face); g.fontHead = font(15, FW_SEMIBOLD, face); g.fontTab = font(14, FW_SEMIBOLD, face); g.fontPlay = font(21, FW_BOLD, face);
    g.brBg = CreateSolidBrush(kBg); g.brInput = CreateSolidBrush(kInput); g.brPanel = CreateSolidBrush(kPanel); g.brBar = CreateSolidBrush(kBar);
    { WNDCLASSW sc{}; sc.lpfnWndProc = SelProc; sc.hInstance = g.inst; sc.lpszClassName = L"NG2Sel"; sc.hCursor = LoadCursor(nullptr, IDC_HAND); sc.style = CS_DBLCLKS; RegisterClassW(&sc); }
    {   // portada.png / logo.png junto al exe (opcionales)
        std::vector<uint32_t> px; int w = 0, h = 0;
        for (const char* ext : { "png", "jpg", "jpeg", "bmp" })
        {
            auto d = ReadAll(ExeDirL() + "\\portada." + ext);
            if (!d.empty() && DecodeImage(d, w, h, px, false))
            {
                BITMAPINFO bi{}; bi.bmiHeader = { sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB };
                void* bits = nullptr; g.banner = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
                if (g.banner && bits) { memcpy(bits, px.data(), px.size() * 4); g.bannerW = w; g.bannerH = h; }
                break;
            }
        }
        auto d = ReadAll(ExeDirL() + "\\logo.png");
        if (!d.empty() && DecodeImage(d, w, h, px, true)) { g.logo = px; g.logoW = w; g.logoH = h; }
    }
    if (const char* pg = getenv("NG2_LAUNCHER_PAGE")) g.tab = std::clamp(atoi(pg), 0, kTabs - 1);   // capturas de cada pagina
    WNDCLASSW wc{}; wc.lpfnWndProc = WndProc; wc.hInstance = g.inst; wc.lpszClassName = L"NG2Launcher"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(g.inst, MAKEINTRESOURCEW(1)); wc.hbrBackground = g.brBg;
    RegisterClassW(&wc);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT r{ 0, 0, S(kW), S(kH) }; AdjustWindowRectExForDpi(&r, style, FALSE, 0, g.dpi);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    g.wnd = CreateWindowExW(0, wc.lpszClassName, L"Ninja Gaiden II - Launcher", style,
                            (GetSystemMetrics(SM_CXSCREEN) - ww) / 2, (GetSystemMetrics(SM_CYSCREEN) - wh) / 2, ww, wh, nullptr, nullptr, g.inst, nullptr);
    BOOL dark = TRUE; DwmSetWindowAttribute(g.wnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    COLORREF caption = kSide, border = kLine;
    DwmSetWindowAttribute(g.wnd, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
    DwmSetWindowAttribute(g.wnd, 34 /* DWMWA_BORDER_COLOR */, &border, sizeof(border));
    BuildUi();
    UpdateDlcCount(); UpdateSaveInfo(); UpdateTuInfo();
    ShowTab(g.tab);
    ShowWindow(g.wnd, SW_SHOW); UpdateWindow(g.wnd);
    SetTimer(g.wnd, 1, 33, nullptr);
    MSG msg;
    while (!g.done && GetMessageW(&msg, nullptr, 0, 0) > 0)
        if (!IsDialogMessageW(g.wnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    for (HBITMAP b : g.logoBmp) if (b) DeleteObject(b);
    if (g.banner) DeleteObject(g.banner);
    for (HFONT f : { g.font, g.fontBold, g.fontSmall, g.fontTitle, g.fontHead, g.fontTab, g.fontPlay }) DeleteObject(f);
    UnregisterClassW(wc.lpszClassName, g.inst); UnregisterClassW(L"NG2Sel", g.inst);
    DeleteObject(g.brBg); DeleteObject(g.brInput); DeleteObject(g.brPanel); DeleteObject(g.brBar);
    CoUninitialize();
    SetThreadDpiAwarenessContext(oldDpi);
    LOG("[launcher] ventana cerrada: %s", g.result ? "JUGAR" : "salir");
    return g.result;
}
