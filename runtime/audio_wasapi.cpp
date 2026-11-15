// Salida de audio (sesion 21): WASAPI en modo compartido, estereo float a 48 kHz (Windows convierte al formato del dispositivo).
// El juego entrega frames de 256 muestras x 6 canales (5.1) con XAudioSubmitRenderDriverFrame; aqui llegan ya mezclados a estereo.
// NG2_NOAUDIO=1 desactiva la salida.
#include "runtime.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdlib>

namespace
{
constexpr uint32_t kRate = 48000;
constexpr size_t kRingFrames = kRate / 2;          // 0,5 s de margen
constexpr size_t kMaxLatencyFrames = kRate / 4;    // si se acumulan mas de 125 ms se descarta lo antiguo
std::mutex g_mutex;
std::vector<float> g_ring(kRingFrames * 2);
size_t g_read = 0, g_count = 0;                    // en frames estereo
std::atomic<bool> g_started{ false }, g_running{ false };
std::atomic<uint64_t> g_underruns{ 0 }, g_drops{ 0 };
float g_peak = 0; uint64_t g_frames = 0;   // protegidos por g_mutex
uint64_t g_dropouts = 0, g_dropoutsWin = 0, g_dips = 0, g_dipsWin = 0;
constexpr size_t kRecFrames = kRate * 30;          // sesion 28: ultimos 30 s de la salida (F9)
std::mutex g_recMutex; std::vector<float> g_rec; size_t g_recPos = 0; bool g_recFull = false;

void RenderThread()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* en = nullptr; IMMDevice* dev = nullptr; IAudioClient* client = nullptr; IAudioRenderClient* render = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en)) ||
        FAILED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) ||
        FAILED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client)))
    { LOG("[wasapi] no hay dispositivo de salida"); return; }
    {   // nombre del dispositivo de salida predeterminado (sesion 24: para diagnosticar "no se oye")
        IPropertyStore* ps = nullptr; PROPVARIANT pv; PropVariantInit(&pv);
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &ps)) && SUCCEEDED(ps->GetValue(PKEY_Device_FriendlyName, &pv)) && pv.vt == VT_LPWSTR)
        { char name[256]; WideCharToMultiByte(CP_UTF8, 0, pv.pwszVal, -1, name, sizeof(name), nullptr, nullptr); LOG("[wasapi] dispositivo: %s", name); }
        PropVariantClear(&pv); if (ps) ps->Release();
    }
    WAVEFORMATEXTENSIBLE wf{};
    wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE; wf.Format.nChannels = 2; wf.Format.nSamplesPerSec = kRate;
    wf.Format.wBitsPerSample = 32; wf.Format.nBlockAlign = 8; wf.Format.nAvgBytesPerSec = kRate * 8; wf.Format.cbSize = 22;
    wf.Samples.wValidBitsPerSample = 32; wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT; wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        600000 /* 60 ms */, 0, &wf.Format, nullptr);
    if (FAILED(hr)) { LOG("[wasapi] Initialize fallo (0x%08lX)", (unsigned long)hr); return; }
    client->SetEventHandle(ev);
    client->GetService(__uuidof(IAudioRenderClient), (void**)&render);
    UINT32 bufFrames = 0; client->GetBufferSize(&bufFrames);
    // Sesion 24 (retraso): solo se mantienen ~NG2_WASAPIMS ms (por defecto 20, minimo 2 periodos del dispositivo) en el buffer
    // de WASAPI; antes se llenaba entero (60 ms) y sumaba ese retraso a cada sonido.
    REFERENCE_TIME defPeriod = 100000, minPeriod = 0; client->GetDevicePeriod(&defPeriod, &minPeriod);
    int wasapiMs = getenv("NG2_WASAPIMS") ? atoi(getenv("NG2_WASAPIMS")) : 20;
    UINT32 periodFrames = (UINT32)(defPeriod * kRate / 10000000);
    UINT32 targetPad = std::min<UINT32>(bufFrames, std::max<UINT32>((UINT32)(kRate * std::max(wasapiMs, 1) / 1000), 2 * periodFrames));
    client->Start();
    g_running = true;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);   // sesion 23
    LOG("[wasapi] salida iniciada: estereo float %u Hz, buffer %u frames, objetivo %u frames (periodo %u)", kRate, bufFrames, targetPad, periodFrames);
    { std::lock_guard l(g_recMutex); g_rec.assign(kRecFrames * 2, 0.0f); }
    UINT32 holeFrames = 0;
    for (;;)
    {
        WaitForSingleObject(ev, 100);
        UINT32 pad = 0; if (FAILED(client->GetCurrentPadding(&pad))) continue;
        UINT32 want = targetPad > pad ? targetPad - pad : 0;
        if (!want) continue;
        BYTE* data = nullptr; if (FAILED(render->GetBuffer(want, &data))) continue;
        float* out = (float*)data;
        {
            std::lock_guard l(g_mutex);
            UINT32 n = (UINT32)std::min<size_t>(want, g_count);
            for (UINT32 i = 0; i < n; i++)
            {
                size_t r = (g_read + i) % kRingFrames;
                out[i * 2] = g_ring[r * 2]; out[i * 2 + 1] = g_ring[r * 2 + 1];
            }
            g_read = (g_read + n) % kRingFrames; g_count -= n;
            if (n < want) { memset(out + n * 2, 0, (want - n) * 8); if (g_started) { g_underruns++; holeFrames = want - n; } }
        }
        {   // Sesion 28: lo que sale de verdad (huecos incluidos) a la grabacion de los ultimos 30 s (F9 -> corte_NN.wav)
            std::lock_guard l(g_recMutex);
            for (UINT32 i = 0; i < want; i++) { g_rec[g_recPos * 2] = out[i * 2]; g_rec[g_recPos * 2 + 1] = out[i * 2 + 1]; if (++g_recPos == kRecFrames) { g_recPos = 0; g_recFull = true; } }
        }
        render->ReleaseBuffer(want, 0);
        if (holeFrames) { static int nu = 0; if (nu++ < 200) LOG("[wasapi] hueco t=%.2fs: faltaron %u de %u frames", GetTickCount64() / 1000.0, holeFrames, want); holeFrames = 0; }
    }
}
} // namespace

// Mezcla un frame del XAudio de la 360 (256 muestras x 6 canales planares, float big-endian; orden FL FR FC LFE BL BR) a estereo
void AudioSubmitXenonFrame(const uint8_t* src)
{
    static bool disabled = getenv("NG2_NOAUDIO") != nullptr;
    if (disabled) return;
    static std::once_flag once;
    std::call_once(once, [] { std::thread(RenderThread).detach(); });
    auto ch = [src](int c, int i) { uint32_t v = __builtin_bswap32(((const uint32_t*)src)[c * 256 + i]); float f; memcpy(&f, &v, 4); return f; };
    std::lock_guard l(g_mutex);
    g_started = true;
    // NG2_WAVDUMP=<archivo>: guarda la mezcla estereo (float 48 kHz) que se envia a WASAPI, hasta 10 minutos (diagnostico, sesion 24)
    static FILE* wav = [] {
        const char* pth = getenv("NG2_WAVDUMP"); if (!pth) return (FILE*)nullptr;
        FILE* f = fopen(pth, "wb"); if (!f) return f;
        uint8_t hdr[44] = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ', 16,0,0,0, 3,0, 2,0 };
        *(uint32_t*)(hdr + 24) = kRate; *(uint32_t*)(hdr + 28) = kRate * 8; *(uint16_t*)(hdr + 32) = 8; *(uint16_t*)(hdr + 34) = 32;
        memcpy(hdr + 36, "data", 4); fwrite(hdr, 1, 44, f); return f; }();
    static uint64_t wavFrames = 0;
    for (int i = 0; i < 256; i++)
    {
        float c = ch(2, i) * 0.7071f;
        float L = ch(0, i) + c + ch(4, i) * 0.7071f;
        float R = ch(1, i) + c + ch(5, i) * 0.7071f;
        if (g_count >= kMaxLatencyFrames) { g_read = (g_read + 1) % kRingFrames; g_count--; g_drops++; }
        size_t w = (g_read + g_count) % kRingFrames;
        g_peak = std::max(g_peak, std::max(std::fabs(L), std::fabs(R)));   // pico de la mezcla (antes del limitador)
        // Sesion 27: limitador suave. La mezcla 5.1 -> estereo (FL + 0,71 C + 0,71 BL) pasa de 1 en combate (picos de 1,5 en la
        // partida del usuario) y el recorte duro sonaba a chasquidos/distorsion. Ataque inmediato, recuperacion en ~50 ms.
        // NG2_NOLIMITER=1 = recorte duro (antes).
        static const bool noLim = getenv("NG2_NOLIMITER") != nullptr;
        static float limGain = 1.0f;
        if (!noLim)
        {
            const float pk = std::max(std::fabs(L), std::fabs(R)), ceilv = 0.95f;
            const float target = pk > ceilv ? ceilv / pk : 1.0f;
            if (target < limGain) limGain = target; else limGain += (target - limGain) * 0.0004f;
            L *= limGain; R *= limGain;
        }
        g_ring[w * 2] = std::clamp(L, -1.0f, 1.0f); g_ring[w * 2 + 1] = std::clamp(R, -1.0f, 1.0f);
        g_count++;
        if (wav && wavFrames < uint64_t(kRate) * 600) { float lr[2] = { L, R }; fwrite(lr, 4, 2, wav); wavFrames++;
            if ((wavFrames & 0x3FFF) == 0) { long pos = ftell(wav); uint32_t d = (uint32_t)(wavFrames * 8), r = d + 36;
                fseek(wav, 4, SEEK_SET); fwrite(&r, 4, 1, wav); fseek(wav, 40, SEEK_SET); fwrite(&d, 4, 1, wav); fseek(wav, pos, SEEK_SET); fflush(wav); } }
    }
    {   // Sesion 27: frame 20 dB por debajo de los 3 anteriores = "el audio se traba". Sesion 28: siempre activo (el contador de
        // silencios totales no ve los cortes en los que algo sigue sonando); se cuenta en la linea [wasapi] y se registran las
        // primeras 300 con la hora (t= de GetTickCount, como las demas lineas). NG2_NODIPLOG=1 = solo contar.
        static const bool dipLog = getenv("NG2_NODIPLOG") == nullptr;
        static float hist[3] = {}; static uint64_t fno = 0; static int nd = 0;
        double e = 0; for (int i = 0; i < 256; i++) { float v = 0.5f * (ch(0, i) + ch(1, i)); e += v * v; }
        float r = (float)sqrt(e / 256), avg = (hist[0] + hist[1] + hist[2]) / 3;
        if (avg > 0.03f && r < avg * 0.1f)
        {
            g_dips++; g_dipsWin++;
            if (dipLog && nd++ < 300)
            { extern std::string XmaRecentStarves(double ms); LOG("[audio] caida t=%.2fs (frame de audio %llu): rms %.4f, antes %.4f, en cola %zu; XMA sin entrada <30 ms:%s", GetTickCount64() / 1000.0, (unsigned long long)fno, r, avg, g_count, XmaRecentStarves(30).c_str()); }
        }
        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = r; fno++;
    }
    // Caidas bruscas a silencio en la mezcla del juego (sesion 23): frame de 256 muestras casi mudo tras uno con sonido
    {
        float fp = 0;
        for (int i = 0; i < 256; i++) fp = std::max(fp, std::max(std::fabs(ch(0, i)), std::fabs(ch(1, i))));
        static float lastFp = 0;
        if (fp < 0.0005f && lastFp > 0.05f) { g_dropouts++; g_dropoutsWin++; }
        lastFp = fp;
    }
    // Nivel de pico cada ~5 s (para comprobar en el log que llega sonido de verdad)
    if (++g_frames % 940 == 0)
    {
        LOG("[wasapi] t=%lus pico %.3f, en cola %zu frames, huecos %llu, descartes %llu, cortes en la mezcla %llu (total %llu), caidas -20 dB %llu (total %llu)", GetTickCount() / 1000, g_peak, g_count,
            (unsigned long long)g_underruns.load(), (unsigned long long)g_drops.load(), (unsigned long long)g_dropoutsWin, (unsigned long long)g_dropouts,
            (unsigned long long)g_dipsWin, (unsigned long long)g_dips);
        g_peak = 0; g_dropoutsWin = 0; g_dipsWin = 0;
    }
}
// Sesion 28: F9 guarda los ultimos 30 s de lo que salio por los altavoces en corte_<n>.wav (float estereo 48 kHz), en otro hilo
void AudioSaveRecent(int n)
{
    std::vector<float> buf;
    {
        std::lock_guard l(g_recMutex);
        if (g_rec.empty()) return;
        size_t frames = g_recFull ? kRecFrames : g_recPos, start = g_recFull ? g_recPos : 0;
        buf.resize(frames * 2);
        for (size_t i = 0; i < frames; i++) { size_t r = (start + i) % kRecFrames; buf[i * 2] = g_rec[r * 2]; buf[i * 2 + 1] = g_rec[r * 2 + 1]; }
    }
    std::thread([n, buf = std::move(buf)] {
        char name[32]; snprintf(name, sizeof name, "corte_%02d.wav", n);
        FILE* f = fopen(name, "wb"); if (!f) { LOG("[audio] no se pudo crear %s", name); return; }
        uint8_t hdr[44] = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ', 16,0,0,0, 3,0, 2,0 };
        uint32_t d = (uint32_t)(buf.size() * 4), r = d + 36;
        memcpy(hdr + 4, &r, 4); *(uint32_t*)(hdr + 24) = kRate; *(uint32_t*)(hdr + 28) = kRate * 8; *(uint16_t*)(hdr + 32) = 8; *(uint16_t*)(hdr + 34) = 32;
        memcpy(hdr + 36, "data", 4); memcpy(hdr + 40, &d, 4);
        fwrite(hdr, 1, 44, f); fwrite(buf.data(), 4, buf.size(), f); fclose(f);
        LOG("[audio] %s guardado (%.1f s de la salida antes de la marca)", name, buf.size() / 2 / double(kRate));
    }).detach();
}
size_t AudioQueuedFrames() { std::lock_guard l(g_mutex); return g_count; }
bool AudioOutputActive() { return g_running.load(); }
uint64_t AudioUnderruns() { return g_underruns.load(); }
uint64_t AudioDrops() { return g_drops.load(); }
