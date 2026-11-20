"""Генератор SVG-иконок Crumb (этап 3).

Пишет три варианта иконки и их упрощённые версии для 16-32 px, плюс preview.html.
Контуры (волнистый край печеньки, укус) считаются здесь математически, поэтому
SVG не использует маски/фильтры и одинаково рисуется в браузере и в QtSvg.

Запуск:  python gen_icons.py   (пишет файлы рядом со скриптом)
"""

import math
import os

OUT = os.path.dirname(os.path.abspath(__file__))
SIZE = 512


def f(v):
    return f"{v:.1f}".rstrip("0").rstrip(".")


def cookie_outline(cx, cy, r, lobes, depth, bite=None, steps=1440):
    """Путь волнистого круга; bite=(bx, by, br) вырезает укус дугой окружности."""
    pts = []
    for i in range(steps):
        a = 2 * math.pi * i / steps
        rr = r * (1 - depth / 2 + depth / 2 * math.cos(lobes * a))
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    if not bite:
        return "M" + " L".join(f"{f(x)},{f(y)}" for x, y in pts) + " Z"

    bx, by, br = bite
    inside = [math.hypot(x - bx, y - by) < br for x, y in pts]
    # Находим участок внутри укуса (одна непрерывная дуга по кругу).
    n = len(pts)
    start = next(i for i in range(n) if inside[i] and not inside[i - 1])   # вход в укус
    end = next(i for i in range(n) if not inside[i] and inside[i - 1])     # выход из укуса
    seq = []
    i = end
    while i != start:
        seq.append(pts[i])
        i = (i + 1) % n
    d = "M" + " L".join(f"{f(x)},{f(y)}" for x, y in seq)
    ex, ey = pts[end]
    # Дуга укуса — вогнутая внутрь печеньки (sweep=0).
    d += f" A{f(br)},{f(br)} 0 0 0 {f(ex)},{f(ey)} Z"
    return d


def hand(cx, cy, angle_deg, length, width, color, extra=""):
    a = math.radians(angle_deg - 90)
    x2, y2 = cx + length * math.cos(a), cy + length * math.sin(a)
    return (f'<line x1="{f(cx)}" y1="{f(cy)}" x2="{f(x2)}" y2="{f(y2)}" stroke="{color}" '
            f'stroke-width="{f(width)}" stroke-linecap="round"{extra}/>')


def blob(cx, cy, r, seed, lobes=5):
    """Неровная «шоколадная капля»."""
    pts = []
    for i in range(48):
        a = 2 * math.pi * i / 48
        wob = 1 + 0.16 * math.sin(lobes * a + seed) + 0.08 * math.cos(3 * a + seed * 1.7)
        pts.append((cx + r * wob * math.cos(a), cy + r * 0.86 * wob * math.sin(a)))
    return "M" + " L".join(f"{f(x)},{f(y)}" for x, y in pts) + " Z"


def crumb(cx, cy, r, rot):
    pts = []
    for i in range(5):
        a = math.radians(rot + i * 72 + (13 if i % 2 else -9))
        rr = r * (1.0 if i % 2 else 0.72)
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    return "M" + " L".join(f"{f(x)},{f(y)}" for x, y in pts) + " Z"


def rewind_arc(cx, cy, r, a0, a1, width, color):
    """Дуга против часовой стрелки со стрелкой на конце (отсылка к Rewind)."""
    p0 = (cx + r * math.cos(math.radians(a0)), cy + r * math.sin(math.radians(a0)))
    p1 = (cx + r * math.cos(math.radians(a1)), cy + r * math.sin(math.radians(a1)))
    large = 1 if abs(a0 - a1) > 180 else 0
    s = (f'<path d="M{f(p0[0])},{f(p0[1])} A{f(r)},{f(r)} 0 {large} 0 {f(p1[0])},{f(p1[1])}" '
         f'fill="none" stroke="{color}" stroke-width="{f(width)}" stroke-linecap="round"/>')
    # наконечник: касательная к окружности в точке p1 (движение против часовой)
    t = math.radians(a1)
    tx, ty = math.sin(t), -math.cos(t)          # направление движения
    nx, ny = math.cos(t), math.sin(t)           # нормаль наружу
    L = width * 2.1
    tip = (p1[0] + tx * L, p1[1] + ty * L)
    b1 = (p1[0] + nx * L * 0.85, p1[1] + ny * L * 0.85)
    b2 = (p1[0] - nx * L * 0.85, p1[1] - ny * L * 0.85)
    s += (f'<path d="M{f(b1[0])},{f(b1[1])} L{f(tip[0])},{f(tip[1])} L{f(b2[0])},{f(b2[1])} Z" '
          f'fill="{color}" stroke="{color}" stroke-width="{f(width * 0.35)}" stroke-linejoin="round"/>')
    return s


def svg(body, defs):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {SIZE} {SIZE}" width="{SIZE}" height="{SIZE}">\n'
            f'<defs>\n{defs}\n</defs>\n{body}\n</svg>\n')


CX, CY, R = 256, 252, 196
BITE = (CX + R * 0.80, CY - R * 0.80, 92)


def shadow_defs(prefix):
    return (f'<radialGradient id="{prefix}sh" cx="50%" cy="50%" r="50%">'
            f'<stop offset="0" stop-color="#000" stop-opacity="0.32"/>'
            f'<stop offset="0.6" stop-color="#000" stop-opacity="0.14"/>'
            f'<stop offset="1" stop-color="#000" stop-opacity="0"/></radialGradient>\n'
            f'<radialGradient id="{prefix}hl" cx="35%" cy="28%" r="55%">'
            f'<stop offset="0" stop-color="#fff" stop-opacity="0.55"/>'
            f'<stop offset="0.45" stop-color="#fff" stop-opacity="0.12"/>'
            f'<stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>')


def shadow(prefix):
    return f'<ellipse cx="{CX}" cy="{CY + R * 0.93}" rx="{R * 0.92}" ry="{R * 0.17}" fill="url(#{prefix}sh)"/>'


# ----------------------------------------------------------- вариант A
def variant_a():
    p = "a"
    defs = shadow_defs(p) + (
        f'\n<radialGradient id="{p}body" cx="38%" cy="32%" r="75%">'
        '<stop offset="0" stop-color="#FCE3A8"/><stop offset="0.55" stop-color="#EDB462"/>'
        '<stop offset="1" stop-color="#B8702C"/></radialGradient>\n'
        f'<linearGradient id="{p}choc" x1="0" y1="0" x2="1" y2="1">'
        '<stop offset="0" stop-color="#7A4A2C"/><stop offset="1" stop-color="#3E2112"/></linearGradient>')
    outline = cookie_outline(CX, CY, R, 12, 0.075, BITE)
    body = [shadow(p),
            f'<path d="{outline}" fill="url(#{p}body)"/>',
            # тёмный «пропечённый» край
            f'<path d="{outline}" fill="none" stroke="#9A5A22" stroke-opacity="0.45" stroke-width="10"/>']
    chips = [(252, 404, 22, 1.0), (330, 330, 22, 2.3), (196, 352, 20, 4.1), (300, 196, 15, 0.4),
             (140, 290, 15, 3.3), (360, 262, 13, 5.2)]
    for x, y, r, s in chips:
        body.append(f'<path d="{blob(x, y, r, s)}" fill="url(#{p}choc)"/>')
        body.append(f'<ellipse cx="{f(x - r * 0.3)}" cy="{f(y - r * 0.35)}" rx="{f(r * 0.32)}" ry="{f(r * 0.2)}" fill="#fff" fill-opacity="0.35"/>')
    # стрелки часов — «шоколадная глазурь»
    body.append(rewind_arc(CX, CY + 4, 120, 330, 205, 14, "#5A331C"))
    body.append(hand(CX, CY + 4, -30, 92, 22, "#4A2814"))
    body.append(hand(CX, CY + 4, 80, 64, 22, "#4A2814"))
    body.append(f'<circle cx="{CX}" cy="{CY + 4}" r="17" fill="#4A2814"/>')
    body.append(f'<circle cx="{CX - 5}" cy="{CY - 1}" r="5" fill="#fff" fill-opacity="0.45"/>')
    # крошки у укуса
    for x, y, r, rot in [(420, 120, 15, 10), (452, 168, 10, 50), (392, 80, 9, 80), (466, 100, 7, 20)]:
        body.append(f'<path d="{crumb(x, y, r, rot)}" fill="#E3A352"/>')
        body.append(f'<path d="{crumb(x - 2, y - 2, r * 0.5, rot)}" fill="#FCE3A8" fill-opacity="0.7"/>')
    body.append(f'<path d="{outline}" fill="url(#{p}hl)"/>')
    return svg("\n".join(body), defs)


def variant_a_small():
    p = "as"
    defs = (f'<radialGradient id="{p}body" cx="38%" cy="32%" r="75%">'
            '<stop offset="0" stop-color="#FBD893"/><stop offset="1" stop-color="#C47D33"/></radialGradient>')
    outline = cookie_outline(CX, 262, 236, 8, 0.10, (CX + 236 * 0.78, 262 - 236 * 0.78, 120))
    body = [f'<path d="{outline}" fill="url(#{p}body)"/>',
            f'<path d="{outline}" fill="none" stroke="#8A4E1C" stroke-width="18"/>',
            hand(CX, 270, -30, 120, 46, "#3E2112"),
            hand(CX, 270, 80, 84, 46, "#3E2112")]
    return svg("\n".join(body), defs)


# ----------------------------------------------------------- вариант B
def variant_b():
    p = "b"
    defs = shadow_defs(p) + (
        f'\n<radialGradient id="{p}body" cx="36%" cy="30%" r="78%">'
        '<stop offset="0" stop-color="#EBDDFF"/><stop offset="0.5" stop-color="#B69CF5"/>'
        '<stop offset="1" stop-color="#6B4FC9"/></radialGradient>\n'
        f'<linearGradient id="{p}chip" x1="0" y1="0" x2="1" y2="1">'
        '<stop offset="0" stop-color="#5B43B0"/><stop offset="1" stop-color="#3A2780"/></linearGradient>\n'
        f'<linearGradient id="{p}hand" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#FFFFFF"/><stop offset="1" stop-color="#EDE4FF"/></linearGradient>')
    outline = cookie_outline(CX, CY, R, 11, 0.09, BITE)
    body = [shadow(p),
            f'<path d="{outline}" fill="url(#{p}body)"/>',
            f'<path d="{outline}" fill="none" stroke="#4E35A8" stroke-opacity="0.35" stroke-width="8"/>']
    for x, y, r, s in [(252, 404, 19, 1.0), (334, 336, 19, 2.3), (190, 350, 16, 4.1), (148, 286, 12, 3.3),
                       (356, 258, 11, 5.2)]:
        body.append(f'<path d="{blob(x, y, r, s)}" fill="url(#{p}chip)" fill-opacity="0.85"/>')
    # циферблат: мягкий светлый круг
    body.append(f'<circle cx="{CX}" cy="{CY + 4}" r="112" fill="#fff" fill-opacity="0.16"/>')
    body.append(rewind_arc(CX, CY + 4, 114, 330, 205, 13, "#FFFFFF"))
    body.append(hand(CX, CY + 4, -30, 88, 22, "url(#%shand)" % p))
    body.append(hand(CX, CY + 4, 80, 60, 22, "url(#%shand)" % p))
    body.append(f'<circle cx="{CX}" cy="{CY + 4}" r="16" fill="#FFFFFF"/>')
    for x, y, r, rot in [(420, 120, 15, 10), (454, 168, 10, 50), (392, 80, 9, 80), (468, 102, 7, 20)]:
        body.append(f'<path d="{crumb(x, y, r, rot)}" fill="#A88BF0"/>')
    body.append(f'<path d="{outline}" fill="url(#{p}hl)"/>')
    return svg("\n".join(body), defs)


def variant_b_small():
    p = "bs"
    defs = (f'<radialGradient id="{p}body" cx="38%" cy="32%" r="75%">'
            '<stop offset="0" stop-color="#D9C6FF"/><stop offset="1" stop-color="#6E52CC"/></radialGradient>')
    outline = cookie_outline(CX, 262, 236, 8, 0.11, (CX + 236 * 0.78, 262 - 236 * 0.78, 120))
    body = [f'<path d="{outline}" fill="url(#{p}body)"/>',
            f'<path d="{outline}" fill="none" stroke="#3A2780" stroke-width="18"/>',
            hand(CX, 270, -30, 120, 46, "#FFFFFF"),
            hand(CX, 270, 80, 84, 46, "#FFFFFF")]
    return svg("\n".join(body), defs)


# ----------------------------------------------------------- вариант C
def variant_c():
    p = "c"
    defs = shadow_defs(p) + (
        f'\n<linearGradient id="{p}ring" x1="0.1" y1="0" x2="0.9" y2="1">'
        '<stop offset="0" stop-color="#3DDC97"/><stop offset="0.5" stop-color="#2D9CDB"/>'
        '<stop offset="1" stop-color="#7B61FF"/></linearGradient>\n'
        f'<radialGradient id="{p}face" cx="40%" cy="35%" r="70%">'
        '<stop offset="0" stop-color="#FFFDF7"/><stop offset="1" stop-color="#F1E6D2"/></radialGradient>')
    outline = cookie_outline(CX, CY, R, 12, 0.085)
    body = [shadow(p),
            f'<path d="{outline}" fill="url(#{p}ring)"/>',
            # «тесто» внутри — светлый циферблат
            f'<circle cx="{CX}" cy="{CY + 2}" r="128" fill="url(#{p}face)"/>',
            f'<circle cx="{CX}" cy="{CY + 2}" r="128" fill="none" stroke="#000" stroke-opacity="0.08" stroke-width="6"/>']
    # деления циферблата
    for i in range(12):
        a = math.radians(i * 30 - 90)
        r0, r1 = (100, 116) if i % 3 == 0 else (106, 114)
        body.append(f'<line x1="{f(CX + r0 * math.cos(a))}" y1="{f(CY + 2 + r0 * math.sin(a))}" '
                    f'x2="{f(CX + r1 * math.cos(a))}" y2="{f(CY + 2 + r1 * math.sin(a))}" stroke="#2D6BA8" '
                    f'stroke-opacity="0.55" stroke-width="{8 if i % 3 == 0 else 5}" stroke-linecap="round"/>')
    body.append(rewind_arc(CX, CY + 2, 80, 330, 205, 12, "#7B61FF"))
    body.append(hand(CX, CY + 2, -30, 84, 20, "#1F4E80"))
    body.append(hand(CX, CY + 2, 80, 58, 20, "#1F4E80"))
    body.append(f'<circle cx="{CX}" cy="{CY + 2}" r="15" fill="#1F4E80"/>')
    body.append(f'<path d="{outline}" fill="url(#{p}hl)" fill-opacity="0.8"/>')
    return svg("\n".join(body), defs)


def variant_c_small():
    p = "cs"
    defs = (f'<linearGradient id="{p}ring" x1="0.1" y1="0" x2="0.9" y2="1">'
            '<stop offset="0" stop-color="#2FCB8A"/><stop offset="1" stop-color="#5B4BE0"/></linearGradient>')
    outline = cookie_outline(CX, CX, 240, 9, 0.11)
    body = [f'<path d="{outline}" fill="url(#{p}ring)"/>',
            f'<circle cx="{CX}" cy="{CX}" r="150" fill="#FFFDF7"/>',
            hand(CX, CX, -30, 110, 42, "#1F4E80"),
            hand(CX, CX, 80, 78, 42, "#1F4E80")]
    return svg("\n".join(body), defs)



# ----------------------------------------------------------- вариант D
def c_arrow(cx, cy, r, width, color, gap_deg=62, start_deg=0):
    """Буква «C» из стрелки перемотки: дуга почти по кругу против часовой,
    наконечник у верхнего конца."""
    a0 = start_deg + gap_deg / 2          # конец дуги (наконечник)
    a1 = start_deg + 360 - gap_deg / 2    # начало дуги
    p0 = (cx + r * math.cos(math.radians(a1)), cy + r * math.sin(math.radians(a1)))
    p1 = (cx + r * math.cos(math.radians(a0)), cy + r * math.sin(math.radians(a0)))
    s = (f'<path d="M{f(p0[0])},{f(p0[1])} A{f(r)},{f(r)} 0 1 0 {f(p1[0])},{f(p1[1])}" fill="none" '
         f'stroke="{color}" stroke-width="{f(width)}" stroke-linecap="round"/>')
    t = math.radians(a0)
    tx, ty = math.sin(t), -math.cos(t)      # движение против часовой
    nx, ny = math.cos(t), math.sin(t)
    L = width * 1.1
    tip = (p1[0] + tx * L * 1.05, p1[1] + ty * L * 1.05)
    b1 = (p1[0] + nx * L - tx * L * 0.25, p1[1] + ny * L - ty * L * 0.25)
    b2 = (p1[0] - nx * L - tx * L * 0.25, p1[1] - ny * L - ty * L * 0.25)
    s += (f'<path d="M{f(b1[0])},{f(b1[1])} L{f(tip[0])},{f(tip[1])} L{f(b2[0])},{f(b2[1])} Z" fill="{color}" '
          f'stroke="{color}" stroke-width="{f(width * 0.32)}" stroke-linejoin="round"/>')
    return s


def variant_d():
    p = "d"
    cy = 248
    defs = shadow_defs(p) + (
        f'\n<linearGradient id="{p}body" x1="0.12" y1="0.05" x2="0.88" y2="0.98">'
        '<stop offset="0" stop-color="#C7B4FF"/><stop offset="0.45" stop-color="#8C6CF0"/>'
        '<stop offset="1" stop-color="#4B2FB3"/></linearGradient>\n'
        f'<linearGradient id="{p}inner" x1="0.2" y1="0" x2="0.8" y2="1">'
        '<stop offset="0" stop-color="#FFFFFF" stop-opacity="0.30"/>'
        '<stop offset="1" stop-color="#FFFFFF" stop-opacity="0.04"/></linearGradient>\n'
        f'<linearGradient id="{p}mark" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#FFFFFF"/><stop offset="1" stop-color="#EEE6FF"/></linearGradient>')
    outline = cookie_outline(CX, cy, 204, 12, 0.085)
    inner = cookie_outline(CX, cy, 148, 12, 0.10)
    body = [f'<ellipse cx="{CX}" cy="{cy + 196}" rx="176" ry="30" fill="url(#{p}sh)"/>',
            f'<path d="{outline}" fill="url(#{p}body)"/>',
            # тональный внутренний слой, повёрнут на полволны
            f'<g transform="rotate(15 {CX} {cy})"><path d="{inner}" fill="url(#{p}inner)"/></g>',
            # тонкий светлый кант по краю
            f'<path d="{outline}" fill="none" stroke="#FFFFFF" stroke-opacity="0.18" stroke-width="5"/>',
            c_arrow(CX, cy, 104, 34, f"url(#{p}mark)"),
            hand(CX, cy, -50, 56, 26, f"url(#{p}mark)"),
            f'<circle cx="{CX}" cy="{cy}" r="21" fill="#FFFFFF"/>',
            f'<circle cx="{CX}" cy="{cy}" r="8" fill="#6A4FD8"/>',
            f'<path d="{outline}" fill="url(#{p}hl)" fill-opacity="0.75"/>']
    return svg("\n".join(body), defs)


def variant_d_small():
    p = "ds"
    defs = (f'<linearGradient id="{p}body" x1="0.1" y1="0" x2="0.9" y2="1">'
            '<stop offset="0" stop-color="#B49CFF"/><stop offset="1" stop-color="#5136C2"/></linearGradient>')
    outline = cookie_outline(CX, CX, 244, 9, 0.11)
    body = [f'<path d="{outline}" fill="url(#{p}body)"/>',
            c_arrow(CX, CX, 132, 54, "#FFFFFF", gap_deg=74),
            f'<circle cx="{CX}" cy="{CX}" r="30" fill="#FFFFFF"/>']
    return svg("\n".join(body), defs)


VARIANTS = {
    "a": ("Карамель", "Тёплая печенька с шоколадной глазурью-стрелками", variant_a, variant_a_small),
    "b": ("Лаванда", "Тональная палитра Material You, белые стрелки", variant_b, variant_b_small),
    "d": ("Фиолетовый Material You", "Тональный фиолетовый градиент, стрелка перемотки в форме «C»", variant_d, variant_d_small),
    "c": ("Кольцо", "Форма печеньки (M3 cookie) с циферблатом и стрелкой перемотки внутри", variant_c, variant_c_small),
}


def main():
    cards = []
    for key, (name, desc, big, small) in VARIANTS.items():
        for suffix, fn in (("", big), ("-small", small)):
            with open(os.path.join(OUT, f"crumb-{key}{suffix}.svg"), "w", encoding="utf-8") as fh:
                fh.write(fn())
        sizes = "".join(
            f'<img src="crumb-{key}{"-small" if s <= 32 else ""}.svg" width="{s}" height="{s}" title="{s} px">'
            for s in (256, 128, 64, 48, 32, 24, 16))
        cards.append(f'<section><h2>{key.upper()} · {name}</h2><p>{desc}</p>'
                     f'<div class="row light">{sizes}</div><div class="row dark">{sizes}</div></section>')
    html = ("<!doctype html><html lang=ru><meta charset=utf-8><title>Иконка Crumb — варианты</title>"
            "<style>body{font-family:'Segoe UI',sans-serif;margin:32px;background:#f4f1f8;color:#1d1b20}"
            "section{margin-bottom:36px}h2{margin:0 0 4px}p{margin:0 0 12px;color:#555}"
            ".row{display:flex;align-items:flex-end;gap:24px;padding:20px;border-radius:20px;margin-bottom:8px}"
            ".light{background:#fff}.dark{background:#1c1b1f}"
            "img{image-rendering:auto}</style>"
            "<h1>Crumb — выберите иконку</h1>"
            "<p>Справа налево: 256 · 128 · 64 · 48 · 32 · 24 · 16 px. Для 32 px и меньше — упрощённая версия.</p>"
            + "".join(cards) + "</html>")
    with open(os.path.join(OUT, "preview.html"), "w", encoding="utf-8") as fh:
        fh.write(html)
    print("ok:", ", ".join(VARIANTS))


if __name__ == "__main__":
    main()
