#!/usr/bin/env python3
"""Renderiza una vista previa fiel del nuevo diseno del programa (win.c)
   usando las mismas coordenadas y colores. Genera desktop/preview.png
"""
from PIL import Image, ImageDraw, ImageFont

W, H = 760, 936

BG      = (11, 16, 32)
BG2     = (15, 22, 48)
CARD    = (19, 27, 56)
CARD2   = (24, 34, 67)
EDGE    = (36, 48, 89)
TXT     = (232, 236, 248)
MUT     = (139, 150, 192)
ACC     = (34, 211, 238)
ACC2    = (59, 130, 246)
GOLD    = (251, 191, 36)
GREEN   = (52, 211, 153)
FIELD   = (13, 19, 48)
INK     = (4, 18, 26)
HDRTOP  = (20, 30, 66)

img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)


def vgrad(rect, a, b):
    x0, y0, x1, y1 = rect
    w = x1 - x0
    h = y1 - y0
    for i in range(h):
        t = i / max(1, h - 1)
        c = tuple(int(a[k] + (b[k] - a[k]) * t) for k in range(3))
        d.line([(x0, y0 + i), (x0 + w - 1, y0 + i)], fill=c)


def hgrad(rect, a, b):
    x0, y0, x1, y1 = rect
    w = x1 - x0
    h = y1 - y0
    for i in range(w):
        t = i / max(1, w - 1)
        c = tuple(int(a[k] + (b[k] - a[k]) * t) for k in range(3))
        d.line([(x0 + i, y0), (x0 + i, y1 - 1)], fill=c)


def rr(rect, rad, fill=None, outline=None, width=1, dash=False):
    x0, y0, x1, y1 = rect
    if fill:
        if isinstance(fill, tuple) and len(fill) == 2 and isinstance(fill[0], tuple):
            pass
        else:
            d.rounded_rectangle([x0, y0, x1, y1], radius=rad, fill=fill)
    if outline:
        import copy
        d.rounded_rectangle([x0, y0, x1, y1], radius=rad, outline=outline, width=width)


def rrg(rect, rad, a, b):
    x0, y0, x1, y1 = rect
    mask = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle([x0, y0, x1, y1], radius=rad, fill=255)
    tmp = Image.new("RGB", (W, H), (0, 0, 0))
    td = ImageDraw.Draw(tmp)
    w = x1 - x0
    for i in range(w):
        t = i / max(1, w - 1)
        c = tuple(int(a[k] + (b[k] - a[k]) * t) for k in range(3))
        td.line([(x0 + i, y0), (x0 + i, y1 - 1)], fill=c)
    img.paste(tmp, (0, 0), mask)


def txt(rect, s, color, font_size, bold=False, anchor=None):
    f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else
                           "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", font_size)
    x0, y0, x1, y1 = rect
    if anchor == "center":
        d.text(((x0 + x1) // 2, (y0 + y1) // 2), s, font=f, fill=color, anchor="mm")
    elif anchor == "lm":
        d.text((x0, (y0 + y1) // 2), s, font=f, fill=color, anchor="lm")
    else:  # top-left
        d.text((x0, y0), s, font=f, fill=color)


font = lambda sz, bold=False: ImageFont.truetype(
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", sz)

# ---- fondo ----
vgrad((0, 0, W, H), BG2, BG)

# ---- encabezado ----
vgrad((0, 0, W, 104), HDRTOP, BG)
# logo
rrg((32, 26, 96, 90), 14, ACC, ACC2)
d.text((64, 58), "BCFX", font=font(17, True), fill=INK, anchor="mm")
d.text((116, 34), "BCFX  -  Activador de Licencias", font=font(19, True), fill=TXT)
d.text((116, 64), "MQL4 / MQL5 / MT4 / MT5   -   Windows 10/11   -   v1.2.0", font=font(12), fill=MUT)
# badge
rr((520, 40, 712, 78), 18, fill=(9, 36, 28), outline=(38, 92, 72))
d.text((616, 59), "100% local - sin internet", font=font(12), fill=GREEN, anchor="mm")

# ---- tarjeta 1: EA ----
rr((48, 120, 712, 258), 14, fill=CARD, outline=EDGE)
rrg((72, 146, 108, 182), 18, ACC, ACC2)
d.text((90, 164), "1", font=font(15, True), fill=INK, anchor="mm")
d.text((122, 148), "Archivo del Asesor Experto (.mq4 / .mq5)", font=font(14, True), fill=TXT)

# zona de soltar
rr((48, 176, 712, 236), 12, fill=FIELD, outline=(58, 76, 130))
# flecha abajo
cx = 380
d.line([(cx, 198), (cx, 212)], fill=ACC, width=2)
d.line([(cx - 6, 206), (cx, 212), (cx + 6, 206)], fill=ACC, width=2)
d.text((380, 226), "Arrastra aqui el .mq4 / .mq5", font=font(14, True), fill=(205, 214, 238), anchor="mm")
d.text((380, 252), "Ningun EA cargado", font=font(12), fill=MUT, anchor="mm")

# ---- tarjeta 2: datos ----
rr((48, 274, 712, 726), 14, fill=CARD, outline=EDGE)
rrg((72, 300, 108, 336), 18, ACC, ACC2)
d.text((90, 318), "2", font=font(15, True), fill=INK, anchor="mm")
d.text((122, 302), "Datos de la activacion", font=font(14, True), fill=TXT)

# etiquetas
d.text((72, 318), "Nombre de la persona", font=font(11), fill=MUT)
d.text((424, 318), "Tipo de licencia", font=font(11), fill=MUT)
d.text((72, 398), "Vigencia:  dias  /  fecha limite (AAAA-MM-DD)", font=font(11), fill=MUT)
d.text((72, 478), "Restriccion de broker", font=font(11), fill=MUT)
d.text((72, 610), "Restriccion de cuenta", font=font(11), fill=MUT)

# campos
def field(rect):
    rr(rect, 9, fill=FIELD, outline=EDGE)

field((72, 338, 392, 376))
field((72, 418, 182, 456))
field((196, 418, 712, 456))
field((72, 544, 542, 582))
field((72, 676, 242, 714))

# valores de ejemplo dentro de los campos
d.text((84, 344), "Juan Perez", font=font(14), fill=TXT)
d.text((84, 424), "30", font=font(14), fill=TXT)
d.text((208, 424), "2026-10-22", font=font(14), fill=TXT)
d.text((84, 550), "ICMarkets", font=font(14), fill=TXT)
d.text((84, 682), "12345678", font=font(14), fill=MUT)

# flecha entre dias y fecha
d.line([(190, 437), (196, 437), (196, 431), (196, 437), (196, 443)], fill=MUT, width=2)

# segmentos
def seg(rect, o1, o2, active):
    rr(rect, 9, fill=FIELD, outline=EDGE)
    w = (rect[2] - rect[0]) // 2
    a = (rect[0] + 3, rect[1] + 3, rect[0] + w, rect[3] - 3)
    b = (rect[0] + w, rect[1] + 3, rect[2] - 3, rect[3] - 3)
    if active == 0:
        rrg(a, 6, ACC, ACC2)
    else:
        rrg(b, 6, ACC, ACC2)
    d.text(((a[0]+a[2])//2, (a[1]+a[3])//2), o1, font=font(12, True), fill=INK if active==0 else MUT, anchor="mm")
    d.text(((b[0]+b[2])//2, (b[1]+b[3])//2), o2, font=font(12, True), fill=INK if active==1 else MUT, anchor="mm")

seg((424, 338, 712, 376), "Por tiempo", "Ilimitada", 1)
seg((72, 498, 472, 536), "Cualquier broker", "Un broker especifico", 1)
seg((72, 630, 472, 668), "Todas las cuentas", "Una cuenta especifica", 0)

d.text((72, 588), "El broker debe coincidir con AccountCompany() en MT4 / ACCOUNT_COMPANY en MT5 (se ignoran mayusculas y espacios).", font=font(11), fill=MUT)

# ---- boton generar ----
rrg((72, 736, 392, 782), 10, ACC, ACC2)
d.text((232, 759), "ACTIVAR LICENCIA Y PROTEGER EA", font=font(13, True), fill=INK, anchor="mm")
d.text((408, 746), "Genera la licencia y protege el codigo del EA.", font=font(11), fill=MUT)

# ---- tarjeta 3: resultado ----
rr((48, 792, 712, 892), 14, fill=CARD, outline=EDGE)
rrg((72, 812, 108, 848), 18, ACC, ACC2)
d.text((90, 830), "3", font=font(15, True), fill=INK, anchor="mm")
d.text((122, 812), "Licencia generada", font=font(14, True), fill=TXT)
d.text((122, 832), "Descarga el EA protegido y el archivo .lic para tu cliente.", font=font(11), fill=MUT)

# licencia (muestra)
rr((72, 798, 592, 834), 9, fill=FIELD, outline=EDGE)
lic = "B-FFFFFFFF-E7F1378E2EB866DD67E1Jose Garcia nanez*********F6941CBA"
d.text((84, 812), lic, font=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 11), fill=GOLD)

# botones
rr((600, 798, 712, 834), 10, fill=CARD2, outline=EDGE)
d.text((656, 816), "Copiar", font=font(12, True), fill=TXT, anchor="mm")
rrg((72, 846, 332, 886), 10, ACC, ACC2)
d.text((202, 866), "Guardar EA protegido", font=font(13, True), fill=INK, anchor="mm")
rr((348, 846, 568, 886), 10, fill=CARD2, outline=EDGE)
d.text((458, 866), "Guardar .lic", font=font(13, True), fill=TXT, anchor="mm")

img.save("/home/user/mql4andmql5licenseactivator/desktop/preview.png")
print("preview.png generado")
