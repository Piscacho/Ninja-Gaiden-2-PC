// Sesion 26: aplica registros delta de un parche XEX2 (xexp) con descompresion LZX con datos de referencia.
// Implementacion propia a partir de la especificacion publica del formato LZX (bloques verbatim / aligned / sin comprimir,
// arbol previo, ventana circular, 3 desplazamientos repetidos, trama de 32 KB).
// Uso: lzxdelta <registros.bin> <ventana> <base.bin> <tamano_salida> <salida.bin>
// Registro: old_addr(4) new_addr(4) uncompressed_len(2) compressed_len(2) [+ datos LZX si compressed_len > 1], big endian.
//   compressed_len 0: new = ceros; 1: new = copia de old; si no: new = LZX(datos) con referencia = base[old .. old+len).
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

static uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
static uint16_t Be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }

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

static int g_errors = 0;
#define FAIL(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_errors++; return false; } while (0)

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
    if (b.Read(1)) { uint32_t hi = b.Read(16), lo = b.Read(16); fprintf(stderr, "aviso: traduccion E8 (tamano %u) no implementada\n", (hi << 16) | lo); }
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

int main(int argc, char** argv)
{
    if (argc < 6) { fprintf(stderr, "uso: lzxdelta registros ventana base tamano salida\n"); return 2; }
    auto rd = [](const char* f) { std::vector<uint8_t> v; FILE* h = fopen(f, "rb"); if (!h) { fprintf(stderr, "no se abre %s\n", f); exit(1); } fseek(h, 0, SEEK_END); v.resize(ftell(h)); fseek(h, 0, SEEK_SET); fread(v.data(), 1, v.size(), h); fclose(h); return v; };
    std::vector<uint8_t> rec = rd(argv[1]), baseImg = rd(argv[3]);
    uint32_t window = (uint32_t)strtoul(argv[2], nullptr, 0); size_t outSize = strtoull(argv[4], nullptr, 0);
    std::vector<uint8_t> dst(outSize, 0); memcpy(dst.data(), baseImg.data(), std::min(outSize, baseImg.size()));
    size_t p = 0; int n = 0, nz = 0, nc = 0, nl = 0;
    while (p + 12 <= rec.size())
    {
        uint32_t oldA = Be32(&rec[p]), newA = Be32(&rec[p + 4]); uint16_t ul = Be16(&rec[p + 8]), cl = Be16(&rec[p + 10]);
        p += 12; n++;
        if (newA + ul > outSize || oldA + ul > outSize) { fprintf(stderr, "registro %d fuera de rango: old %08X new %08X len %u\n", n, oldA, newA, ul); return 1; }
        if (cl == 0) { memset(&dst[newA], 0, ul); nz++; }
        else if (cl == 1) { memmove(&dst[newA], &dst[oldA], ul); nc++; }
        else
        {
            if (p + cl > rec.size()) { fprintf(stderr, "registro %d: datos cortados\n", n); return 1; }
            std::vector<uint8_t> ref(dst.begin() + oldA, dst.begin() + oldA + ul), tmp(ul);
            if (!LzxDecode(&rec[p], cl, tmp.data(), ul, window, ref.data(), ref.size())) { fprintf(stderr, "registro %d (old %08X new %08X ul %u cl %u) fallo\n", n, oldA, newA, ul, cl); return 1; }
            memcpy(&dst[newA], tmp.data(), ul); p += cl; nl++;
        }
    }
    FILE* o = fopen(argv[5], "wb"); fwrite(dst.data(), 1, dst.size(), o); fclose(o);
    printf("%d registros (%d ceros, %d copias, %d LZX), %zu bytes de registros usados de %zu\n", n, nz, nc, nl, p, rec.size());
    return 0;
}
