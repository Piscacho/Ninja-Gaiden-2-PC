// Sesion 26: aplica la actualizacion del titulo (default.xexp, parche delta XEX2) al default.xex del ISO, en memoria.
// Igual que tools/lzxdelta.cpp + tools/xexp_records.py (ver NOTAS_SESION26.md):
//  - cabeceras nuevas = registros LZX del descriptor 0x5FF aplicados a las cabeceras del original;
//  - clave de sesion nueva = clave AES de las cabeceras nuevas descifrada con la clave retail; los datos del parche se descifran
//    con la clave AES del parche descifrada con esa clave nueva (AES-CBC, IV cero);
//  - datos = bloques [tamano del siguiente(4) + SHA-1 del siguiente(20)] + registros delta hasta el relleno de ceros;
//  - registro: old(4) new(4) longitud(2) comprimido(2) [+ LZX]; 0 = ceros, 1 = copia, si no LZX con referencia = old.
// El descompresor LZX es el mismo que el de tools/lzxdelta.cpp (implementacion propia segun la especificacion publica).
#include "runtime.h"
#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <algorithm>
#pragma comment(lib, "bcrypt.lib")

namespace
{
uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint16_t Be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }
bool Aes(const uint8_t key[16], uint8_t* data, size_t len, bool cbc)
{
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_KEY_HANDLE k = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
    const wchar_t* mode = cbc ? BCRYPT_CHAIN_MODE_CBC : BCRYPT_CHAIN_MODE_ECB;
    BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (PUCHAR)mode, (ULONG)((wcslen(mode) + 1) * 2), 0);
    bool ok = BCryptGenerateSymmetricKey(alg, &k, nullptr, 0, (PUCHAR)key, 16, 0) == 0;
    if (ok) { uint8_t iv[16] = {}; ULONG o = 0; ok = BCryptDecrypt(k, data, (ULONG)len, nullptr, cbc ? iv : nullptr, cbc ? 16 : 0, data, (ULONG)len, &o, 0) == 0; BCryptDestroyKey(k); }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}
bool Sha1(const uint8_t* d, size_t n, uint8_t out[20])
{
    BCRYPT_ALG_HANDLE alg = nullptr; if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) != 0) return false;
    bool ok = BCryptHash(alg, nullptr, 0, (PUCHAR)d, (ULONG)n, out, 20) == 0;
    BCryptCloseAlgorithmProvider(alg, 0); return ok;
}
struct Bits
{
    const uint8_t *p, *end; uint32_t buf = 0; int n = 0;   // buf: bits alineados a la izquierda (MSB primero), n validos
    void Fill() { while (n <= 16) { uint32_t w = (p + 1 < end + 1 && p + 1 <= end - 0) ? 0 : 0; w = (p + 1 < end) ? (p[0] | (p[1] << 8)) : (p < end ? p[0] : 0); p += 2; buf |= w << (16 - n); n += 16; } }
    uint32_t Peek(int k) { Fill(); return k ? buf >> (32 - k) : 0; }
    void Drop(int k) { buf <<= k; n -= k; }
    uint32_t Read(int k) { if (!k) return 0; if (k > 16) { uint32_t hi = Read(k - 16); return (hi << 16) | Read(16); } uint32_t v = Peek(k); Drop(k); return v; }
    // Alinea a 16 bits: descarta n % 16 bits; las palabras completas que queden en el bufer se devuelven a la entrada
    void Align(bool atLeastOne)
    {
        int r = n % 16;
        if (r == 0 && atLeastOne) { if (n == 0) { Fill(); } Drop(16); r = 0; }
        else Drop(r);
        p -= (n / 16) * 2; buf = 0; n = 0;
    }
};

struct Huff
{
    std::vector<uint16_t> tab; std::vector<uint8_t> len; int nsym = 0;
    bool Build(const uint8_t* lens, int count)
    {
        nsym = count; len.assign(lens, lens + count); tab.assign(1 << 16, 0xFFFF);
        uint32_t code = 0;
        for (int L = 1; L <= 16; L++)
        {
            for (int s = 0; s < count; s++)
                if (lens[s] == L)
                {
                    uint32_t lo = code << (16 - L), hi = (code + 1) << (16 - L);
                    if (hi > (1u << 16)) return false;
                    for (uint32_t i = lo; i < hi; i++) tab[i] = (uint16_t)s;
                    code++;
                }
            code <<= 1;
        }
        return true;
    }
    int Decode(Bits& b) { uint32_t v = b.Peek(16); uint16_t s = tab[v]; if (s == 0xFFFF) return -1; b.Drop(len[s]); return s; }
};

#define FAIL(...) do { char _m[160]; snprintf(_m, sizeof(_m), __VA_ARGS__); LOG("[tu] LZX: %s", _m); return false; } while (0)

// Descomprime un flujo LZX completo a out[0..outLen) con la ventana precargada con ref (al final de la ventana)
static bool LzxDecode(const uint8_t* in, size_t inLen, uint8_t* out, size_t outLen, uint32_t window, const uint8_t* ref, size_t refLen)
{
    int wbits = 0; while ((1u << wbits) < window) wbits++;
    static const int kSlots[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 30, 32, 34, 36, 38, 42, 50 };
    int nslots = wbits <= 21 ? kSlots[wbits] : 50;
    if (!nslots) FAIL("ventana no soportada %u", window);
    int extra[64]; uint32_t base[64];
    for (int i = 0, j = 0; i < 64; i += 2) { extra[i] = extra[i + 1] = j; if (i != 0 && j < 17) j++; }
    base[0] = 0; for (int i = 1; i < 64; i++) base[i] = base[i - 1] + (1u << extra[i - 1]);
    std::vector<uint8_t> win(window, 0);
    if (refLen > window) { ref += refLen - window; refLen = window; }
    memcpy(win.data() + window - refLen, ref, refLen);
    const int mainN = 256 + 8 * nslots;
    std::vector<uint8_t> mainLen(mainN, 0), lenLen(249, 0), alignLen(8, 0);
    Huff mainT, lenT, alignT, pre;
    Bits b{ in, in + inLen };
    uint32_t R0 = 1, R1 = 1, R2 = 1;
    size_t pos = 0;   // posicion de salida (la ventana es circular: pos & (window-1))
    int btype = 0; uint32_t bremain = 0, blength = 0;
    if (b.Read(1)) { uint32_t hi = b.Read(16), lo = b.Read(16); LOG("[tu] aviso: traduccion E8 (%u) sin soporte", (hi << 16) | lo); }
    auto readLens = [&](uint8_t* lens, int first, int last) -> bool {
        uint8_t pl[20]; for (int i = 0; i < 20; i++) pl[i] = (uint8_t)b.Read(4);
        if (!pre.Build(pl, 20)) return false;
        for (int x = first; x < last;)
        {
            int z = pre.Decode(b); if (z < 0) return false;
            if (z == 17) { int y = b.Read(4) + 4; while (y-- && x < last) lens[x++] = 0; }
            else if (z == 18) { int y = b.Read(5) + 20; while (y-- && x < last) lens[x++] = 0; }
            else if (z == 19) { int y = b.Read(1) + 4; int z2 = pre.Decode(b); if (z2 < 0) return false; int v = (lens[x] - z2 + 17) % 17; while (y-- && x < last) lens[x++] = (uint8_t)v; }
            else { lens[x] = (uint8_t)((lens[x] - z + 17) % 17); x++; }
        }
        return true;
    };
    size_t frameEnd = 32768;
    while (pos < outLen)
    {
        if (bremain == 0)
        {
            if (btype == 3 && (blength & 1)) b.p++;   // relleno de un bloque sin comprimir de tamano impar
            btype = (int)b.Read(3);
            uint32_t hi = b.Read(16), lo = b.Read(8); bremain = blength = (hi << 8) | lo;
            if (btype == 2) { for (int i = 0; i < 8; i++) alignLen[i] = (uint8_t)b.Read(3); if (!alignT.Build(alignLen.data(), 8)) FAIL("arbol alineado"); }
            if (btype == 1 || btype == 2)
            {
                if (!readLens(mainLen.data(), 0, 256) || !readLens(mainLen.data(), 256, mainN)) FAIL("longitudes del arbol principal");
                if (!mainT.Build(mainLen.data(), mainN)) FAIL("arbol principal");
                if (!readLens(lenLen.data(), 0, 249)) FAIL("longitudes del arbol de longitud");
                if (!lenT.Build(lenLen.data(), 249)) FAIL("arbol de longitud");
            }
            else if (btype == 3)
            {
                b.Align(true);
                if (b.p + 12 > b.end) FAIL("fin de datos en bloque sin comprimir");
                R0 = b.p[0] | (b.p[1] << 8) | (b.p[2] << 16) | (uint32_t(b.p[3]) << 24); R1 = b.p[4] | (b.p[5] << 8) | (b.p[6] << 16) | (uint32_t(b.p[7]) << 24);
                R2 = b.p[8] | (b.p[9] << 8) | (b.p[10] << 16) | (uint32_t(b.p[11]) << 24); b.p += 12;
            }
            else FAIL("tipo de bloque %d no valido (salida %zu/%zu)", btype, pos, outLen);
        }
        // decodificar hasta fin de bloque, fin de trama o fin de salida
        size_t stop = std::min<size_t>(std::min<size_t>(outLen, frameEnd), pos + bremain);
        if (btype == 3)
        {
            size_t k = stop - pos; if (b.p + k > b.end) FAIL("fin de datos sin comprimir");
            for (size_t i = 0; i < k; i++) { uint8_t c = b.p[i]; win[(pos + i) & (window - 1)] = c; out[pos + i] = c; }
            b.p += k; bremain -= (uint32_t)k; pos += k;
        }
        else
        {
            while (pos < stop)
            {
                int m = mainT.Decode(b); if (m < 0) FAIL("simbolo principal invalido en %zu", pos);
                if (m < 256) { win[pos & (window - 1)] = (uint8_t)m; out[pos] = (uint8_t)m; pos++; bremain--; continue; }
                m -= 256;
                int lh = m & 7; if (lh == 7) { int e = lenT.Decode(b); if (e < 0) FAIL("longitud invalida"); lh += e; }
                uint32_t mlen = lh + 2; int slot = m >> 3; uint32_t off;
                if (slot == 0) off = R0;
                else if (slot == 1) { off = R1; R1 = R0; R0 = off; }
                else if (slot == 2) { off = R2; R2 = R0; R0 = off; }
                else
                {
                    int ex = extra[slot]; off = base[slot] - 2;
                    if (btype == 2 && ex >= 3) { if (ex > 3) off += b.Read(ex - 3) << 3; int a = alignT.Decode(b); if (a < 0) FAIL("alineado invalido"); off += a; }
                    else if (ex > 0) off += b.Read(ex);
                    R2 = R1; R1 = R0; R0 = off;
                }
                if (off > window) FAIL("desplazamiento %u fuera de la ventana en %zu", off, pos);
                if (mlen > bremain) FAIL("coincidencia de %u cruza el bloque (%u)", mlen, bremain);
                for (uint32_t i = 0; i < mlen; i++)
                {
                    uint8_t c = win[(pos - off + window) & (window - 1)];
                    win[pos & (window - 1)] = c; if (pos < outLen) out[pos] = c; pos++;
                }
                bremain -= mlen;
            }
        }
        if (pos >= frameEnd) { frameEnd += 32768; if (btype != 3) b.Align(false); }
    }
    return true;
}


// Aplica una lista de registros sobre dst (que ya tiene los datos originales)
bool ApplyRecords(const uint8_t* rec, size_t len, uint32_t window, std::vector<uint8_t>& dst, int& count)
{
    size_t p = 0;
    while (p + 12 <= len)
    {
        uint32_t oldA = Be32(rec + p), newA = Be32(rec + p + 4); uint16_t ul = Be16(rec + p + 8), cl = Be16(rec + p + 10);
        if (ul == 0 && cl == 0) break;   // relleno
        p += 12;
        if (size_t(newA) + ul > dst.size() || size_t(oldA) + ul > dst.size()) { LOG("[tu] registro fuera de rango (old %08X new %08X)", oldA, newA); return false; }
        if (cl == 0) memset(&dst[newA], 0, ul);
        else if (cl == 1) memmove(&dst[newA], &dst[oldA], ul);
        else
        {
            if (p + cl > len) return false;
            std::vector<uint8_t> ref(dst.begin() + oldA, dst.begin() + oldA + ul), tmp(ul);
            if (!LzxDecode(rec + p, cl, tmp.data(), ul, window, ref.data(), ref.size())) return false;
            memcpy(&dst[newA], tmp.data(), ul); p += cl;
        }
        count++;
    }
    return true;
}
const uint8_t kRetail[16] = { 0x20, 0xB1, 0x85, 0xA5, 0x9D, 0x28, 0xFD, 0xC3, 0x40, 0x58, 0x3F, 0xBB, 0x08, 0x96, 0xBF, 0x91 };
} // namespace

// baseXex = default.xex del ISO; baseImage = su imagen ya descifrada (XexToImage). Devuelve las cabeceras nuevas (para
// InitXexHeader) y la imagen nueva. Comprueba la version, la clave (image_key_source) y el SHA-1 de cada bloque.
bool XexpApply(const std::vector<uint8_t>& baseXex, const std::vector<uint8_t>& baseImage, const std::vector<uint8_t>& xexp,
               std::vector<uint8_t>& newHdr, std::vector<uint8_t>& newImage, std::string& err)
{
    auto fail = [&](const char* m) { err = m; LOG("[tu] %s", m); return false; };
    if (xexp.size() < 0x40 || memcmp(xexp.data(), "XEX2", 4) != 0) return fail("default.xexp no es un XEX2");
    const uint8_t* x = xexp.data();
    uint32_t flags = Be32(x + 4), peOff = Be32(x + 8), sec = Be32(x + 0x10), n = Be32(x + 0x14);
    if (!(flags & 0x40)) return fail("default.xexp no es un parche delta");
    uint32_t ffi = 0, desc = 0;
    for (uint32_t i = 0; i < n; i++) { uint32_t k = Be32(x + 0x18 + i * 8), v = Be32(x + 0x1C + i * 8); if (k == 0x3FF) ffi = v; else if (k == 0x5FF) desc = v; }
    if (!ffi || !desc) return fail("faltan cabeceras del parche");
    uint32_t dsize = Be32(x + desc), srcVer = Be32(x + desc + 8);
    uint32_t tgtHdr = Be32(x + desc + 0x30), hsOff = Be32(x + desc + 0x34), hsSize = Be32(x + desc + 0x38), htOff = Be32(x + desc + 0x3C);
    const uint8_t* b = baseXex.data(); uint32_t bn = Be32(b + 0x14), bver = 0;
    for (uint32_t i = 0; i < bn; i++) if (Be32(b + 0x18 + i * 8) == 0x40006) bver = Be32(b + Be32(b + 0x1C + i * 8) + 4);
    if (bver != srcVer) { char m[112]; snprintf(m, sizeof(m), "la actualizacion es para la version %08X y el disco es la %08X", srcVer, bver); return fail(m); }
    newHdr.assign(tgtHdr, 0);
    if (size_t(hsOff) + hsSize > baseXex.size() || size_t(htOff) + hsSize > tgtHdr) return fail("cabeceras fuera de rango");
    memcpy(&newHdr[htOff], b + hsOff, hsSize);
    uint32_t window = Be32(x + ffi + 8); int nh = 0;
    if (!ApplyRecords(x + desc + 0x4C, dsize - 0x4C, window, newHdr, nh) || memcmp(newHdr.data(), "XEX2", 4) != 0) return fail("no se pudieron aplicar las cabeceras");
    uint32_t nsec = Be32(&newHdr[0x10]);
    uint8_t newKey[16]; memcpy(newKey, &newHdr[nsec + 0x150], 16); if (!Aes(kRetail, newKey, 16, false)) return fail("AES");
    {   // image_key_source descifrada con la clave nueva = clave de sesion del original
        uint8_t baseKey[16], src[16]; uint32_t bsec = Be32(b + 0x10);
        memcpy(baseKey, b + bsec + 0x150, 16); Aes(kRetail, baseKey, 16, false);
        memcpy(src, x + desc + 0x20, 16); Aes(newKey, src, 16, false);
        if (memcmp(src, baseKey, 16) != 0) return fail("la actualizacion no corresponde a este disco (clave)");
    }
    uint8_t patchKey[16]; memcpy(patchKey, x + sec + 0x150, 16); Aes(newKey, patchKey, 16, false);
    std::vector<uint8_t> data(xexp.begin() + peOff, xexp.end());
    if (Be16(x + ffi + 4) == 1 && !Aes(patchKey, data.data(), data.size() & ~size_t(15), true)) return fail("AES de los datos");
    uint32_t imgSize = Be32(&newHdr[nsec + 4]);
    newImage.assign(imgSize, 0); memcpy(newImage.data(), baseImage.data(), std::min<size_t>(imgSize, baseImage.size()));
    uint32_t blk = Be32(x + ffi + 12); uint8_t want[20]; memcpy(want, x + ffi + 16, 20);
    size_t p = 0; int nb = 0, nr = 0;
    while (blk)
    {
        if (p + blk > data.size()) return fail("bloque fuera del archivo");
        uint8_t h[20]; Sha1(&data[p], blk, h);
        if (memcmp(h, want, 20) != 0) return fail("SHA-1 de un bloque del parche incorrecto");
        uint32_t next = Be32(&data[p]); memcpy(want, &data[p + 4], 20);
        if (!ApplyRecords(&data[p + 24], blk - 24, window, newImage, nr)) return fail("no se pudo aplicar un bloque de la imagen");
        p += blk; blk = next; nb++;
    }
    LOG("[tu] actualizacion aplicada: version %08X -> %08X, %d bloques, %d registros, imagen 0x%X", srcVer, Be32(x + desc + 4), nb, nr, imgSize);
    return true;
}
