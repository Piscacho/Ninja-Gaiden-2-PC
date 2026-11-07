"""Sesion 26: descifra un parche XEX2 (default.xexp de una actualizacion del titulo) y extrae sus registros delta.
Uso: python -I xexp_records.py default.xexp salida_dir
Genera: hdr_records.bin (registros delta de las cabeceras, del descriptor 0x5FF), img_records.bin (registros delta de la
imagen: el contenido de los bloques de datos, descifrado), xexp_info.txt. Los registros se aplican con lzxdelta.exe.
Formato de un registro: old_addr(4) new_addr(4) uncompressed_len(2) compressed_len(2) + datos LZX (compressed_len > 1)."""
import struct, sys, os, subprocess

RETAIL_KEY = bytes.fromhex("20B185A59D28FDC340583FBB0896BF91")

def aes(data, key, ecb=False):
    args = ["openssl", "enc", "-d", "-nopad", "-K", key.hex()]
    args += ["-aes-128-ecb"] if ecb else ["-aes-128-cbc", "-iv", "00" * 16]
    return subprocess.run(args, input=data, stdout=subprocess.PIPE, check=True).stdout

be32 = lambda b, o: struct.unpack_from(">I", b, o)[0]

def scan(buf, start, end):
    """Recorre registros; devuelve (n, final, lista)"""
    p, out = start, []
    while p + 12 <= end:
        old, new, ul, cl = struct.unpack_from(">IIHH", buf, p)
        out.append((p, old, new, ul, cl))
        p += 12 + (cl if cl > 1 else 0)
    return p, out

def main():
    x = open(sys.argv[1], "rb").read(); out = sys.argv[2]; os.makedirs(out, exist_ok=True)
    flags, pe_off, _, sec, n = struct.unpack_from(">IIIII", x, 4)
    hdrs = {}
    for i in range(n):
        k, v = struct.unpack_from(">II", x, 0x18 + i * 8); hdrs[k] = v
    info = ["flags=0x%X pe_off=0x%X sec=0x%X headers=%s" % (flags, pe_off, sec, " ".join("%08X" % k for k in hdrs))]
    # descriptor delta (0x5FF)
    d = hdrs[0x5FF]; dsize = be32(x, d)
    f = struct.unpack_from(">IIIIIIIIII", x, d + 0x30 - 0x30 + 0x30) if False else None
    tgt_ver, src_ver = be32(x, d + 4), be32(x, d + 8)
    size_tgt_hdr, hs_off, hs_size, ht_off, is_off, is_size, it_off = struct.unpack_from(">IIIIIII", x, d + 0x30)
    info.append("delta: target 0x%08X source 0x%08X tgt_headers 0x%X hdr_src 0x%X+0x%X->0x%X img_src 0x%X+0x%X->0x%X" %
                (tgt_ver, src_ver, size_tgt_hdr, hs_off, hs_size, ht_off, is_off, is_size, it_off))
    end, recs = scan(x, d + 0x4C, d + dsize)
    info.append("registros de cabecera: %d (fin 0x%X, esperado 0x%X)" % (len(recs), end, d + dsize))
    open(os.path.join(out, "hdr_records.bin"), "wb").write(x[d + 0x4C:d + dsize])
    # informacion de archivo (0x3FF): cifrado y compresion (3 = delta)
    ffi = hdrs[0x3FF]; enc, comp = struct.unpack_from(">HH", x, ffi + 4)
    window, blk = be32(x, ffi + 8), be32(x, ffi + 12)
    info.append("cifrado %d compresion %d ventana 0x%X primer bloque 0x%X" % (enc, comp, window, blk))
    image_size = be32(x, sec + 4)
    data = x[pe_off:]
    if enc == 1:
        # Clave de los datos del parche: la clave AES del parche descifrada con la clave de sesion del ejecutable NUEVO
        # (la de las cabeceras ya parcheadas, hdr_new.bin, descifrada con la clave retail). Comprobado con el SHA-1 del bloque.
        nh = open(os.path.join(out, "new_hdr.bin"), "rb").read()
        nsec = be32(nh, 0x10)
        new_session = aes(nh[nsec + 0x150:nsec + 0x160], RETAIL_KEY, ecb=True)
        session = aes(x[sec + 0x150:sec + 0x160], new_session, ecb=True)
        data = aes(data[:len(data) & ~15], session) + data[len(data) & ~15:]
    # bloques encadenados: [tamano del siguiente(4) + sha1 del siguiente(20)] + registros delta (resto del bloque)
    import hashlib
    recbuf = bytearray(); p = 0; nb = 0; want = x[ffi + 16:ffi + 36]
    while blk:
        b = data[p:p + blk]
        if hashlib.sha1(b).digest() != want: info.append("  aviso: SHA-1 del bloque %d no coincide" % nb)
        nxt = be32(b, 0); want = b[4:24]
        # los registros no cruzan bloques; el bloque acaba con relleno de ceros (registro con longitudes 0)
        q = 24
        while q + 12 <= len(b):
            o_, n_, ul_, cl_ = struct.unpack_from(">IIHH", b, q)
            if ul_ == 0 and cl_ == 0: break
            sz = 12 + (cl_ if cl_ > 1 else 0)
            recbuf += b[q:q + sz]; q += sz
        p += blk; blk = nxt; nb += 1
    open(os.path.join(out, "img_records.bin"), "wb").write(recbuf)
    end, recs = scan(recbuf, 0, len(recbuf))
    info.append("imagen: %d bloques, %d bytes de registros, %d registros (fin %d), tamano de imagen nuevo 0x%X" % (nb, len(recbuf), len(recs), end, image_size))
    kinds = {0: 0, 1: 0, 2: 0}
    for (_, old, new, ul, cl) in recs: kinds[min(cl, 2)] += 1
    info.append("  ceros %d, copias %d, LZX %d" % (kinds[0], kinds[1], kinds[2]))
    for r in recs[:12] + recs[-4:]: info.append("  @%06X old %08X new %08X ul %04X cl %04X" % r)
    open(os.path.join(out, "xexp_info.txt"), "w").write("\n".join(info) + "\n")
    print("\n".join(info))

if __name__ == "__main__":
    main()
