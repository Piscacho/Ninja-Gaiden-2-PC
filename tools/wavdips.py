"""Busca caidas cortas de la mezcla en un volcado NG2_WAVDUMP (float estereo 48 kHz): bloques de 256 muestras 20 dB por debajo
de sus vecinos (sesion 27, "el audio se traba"). Uso: python tools/wavdips.py run/x.wav [desde_s]"""
import sys
import numpy as np
d = np.fromfile(sys.argv[1], dtype=np.float32, offset=44).reshape(-1, 2)
t0 = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
m = d.mean(1); B = 256; nb = len(m) // B
db = 20 * np.log10(np.sqrt((m[:nb * B].reshape(nb, B) ** 2).mean(1)) + 1e-9)
dips = [i for i in range(3, nb - 3) if i * B / 48000 >= t0 and np.r_[db[i - 3:i], db[i + 1:i + 4]].mean() > -30
        and db[i] < np.r_[db[i - 3:i], db[i + 1:i + 4]].mean() - 20]
dur = len(m) / 48000 - t0
print("duracion %.0f s, caidas %d (%.2f por minuto)" % (dur, len(dips), len(dips) / dur * 60), [round(i * B / 48000, 2) for i in dips[:40]])
