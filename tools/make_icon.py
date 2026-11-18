# Genera project/ng2.ico (icono propio del ejecutable y del launcher, sesion 28): shuriken rojo y acero sobre fondo oscuro.
# Uso: python tools/make_icon.py   (requiere Pillow)
import math, os
from PIL import Image, ImageDraw, ImageFilter

S = 1024  # se dibuja grande y se reduce
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
d.rounded_rectangle([24, 24, S - 24, S - 24], radius=180, fill=(12, 12, 15, 255), outline=(120, 30, 38, 255), width=20)

def star(cx, cy, r_out, r_in, rot, n=4):
    pts = []
    for i in range(n * 2):
        r = r_out if i % 2 == 0 else r_in
        a = rot + i * math.pi / n
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts

c = S / 2
# brillo rojo detras
glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ImageDraw.Draw(glow).polygon(star(c, c, 430, 120, -math.pi / 4), fill=(206, 24, 38, 170))
img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(40)))
# shuriken: cuchillas rojas, filo de acero en una mitad de cada punta
d.polygon(star(c, c, 400, 105, -math.pi / 4), fill=(206, 24, 38, 255))
for k in range(4):
    a = -math.pi / 4 + k * math.pi / 2
    tip = (c + 400 * math.cos(a), c + 400 * math.sin(a))
    side = (c + 105 * math.cos(a + math.pi / 4), c + 105 * math.sin(a + math.pi / 4))
    d.polygon([(c, c), tip, side], fill=(236, 236, 240, 255))
d.ellipse([c - 70, c - 70, c + 70, c + 70], fill=(12, 12, 15, 255), outline=(170, 176, 188, 255), width=14)

sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
img.resize((256, 256), Image.LANCZOS).save(os.path.join(root, "project", "ng2.ico"), sizes=sizes)
img.resize((256, 256), Image.LANCZOS).save(os.path.join(root, "project", "ng2_icon_preview.png"))
print("project/ng2.ico")
