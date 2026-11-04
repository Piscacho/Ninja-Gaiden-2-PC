"""Montaje de imagenes (PNG/BMP) con Pillow: python montage_png.py salida.png columnas [--crop x0,y0,x1,y1] img1 img2 ..."""
import sys
from PIL import Image
out, cols = sys.argv[1], int(sys.argv[2]); args = sys.argv[3:]; crop = None
if args and args[0] == "--crop": crop = tuple(int(v) for v in args[1].split(",")); args = args[2:]
ims = []
for p in args:
    im = Image.open(p).convert("RGB")
    if crop: im = im.crop(crop)
    ims.append(im.resize((640, 360)))
rows = (len(ims) + cols - 1) // cols
m = Image.new("RGB", (640 * cols, 360 * rows))
for i, im in enumerate(ims): m.paste(im, ((i % cols) * 640, (i // cols) * 360))
m.save(out)
