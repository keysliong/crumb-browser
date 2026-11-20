"""Иконка Crumb в духе TikTok You: узнаваемый символ + печенька Material 3.

У TikTok You — нота TikTok, у которой головка стала печенькой. У Crumb — глобус
браузера, который сам стал печенькой (и заодно каламбур: браузерные cookies).
Три стиля, как у TikTok You: неон, металл, карамель. Все — на плитке-сквиркле
с тёмными фиолетовыми разводами. Свечение и тени собраны из обычных фигур
(без SVG-фильтров), поэтому одинаково рисуются в браузере и в QtSvg.

Запуск: python gen_globe.py  → crumb-e/f/g(.svg, -small.svg)
"""

import math
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_icons import cookie_outline, f, svg  # noqa: E402

OUT = os.path.dirname(os.path.abspath(__file__))
C = 256           # центр
R = 150           # радиус глобуса-печеньки
WAVES, DEPTH = 10, 0.075


# ------------------------------------------------------------------ плитка
def tile_defs(p):
    return (f'<linearGradient id="{p}tile" x1="0" y1="0" x2="1" y2="1">'
            '<stop offset="0" stop-color="#2A1460"/><stop offset="0.55" stop-color="#140A33"/>'
            '<stop offset="1" stop-color="#0A0620"/></linearGradient>\n'
            f'<radialGradient id="{p}sw1" cx="0.5" cy="0.5" r="0.5">'
            '<stop offset="0" stop-color="#9C7BFF" stop-opacity="0.55"/>'
            '<stop offset="0.6" stop-color="#6A47E0" stop-opacity="0.18"/>'
            '<stop offset="1" stop-color="#6A47E0" stop-opacity="0"/></radialGradient>\n'
            f'<radialGradient id="{p}sw2" cx="0.5" cy="0.5" r="0.5">'
            '<stop offset="0" stop-color="#C6B2FF" stop-opacity="0.45"/>'
            '<stop offset="1" stop-color="#7B5CF0" stop-opacity="0"/></radialGradient>\n'
            f'<linearGradient id="{p}gloss" x1="0" y1="0" x2="0" y2="1">'
            '<stop offset="0" stop-color="#FFFFFF" stop-opacity="0.16"/>'
            '<stop offset="0.45" stop-color="#FFFFFF" stop-opacity="0"/></linearGradient>')


def tile(p):
    # Плитка-сквиркл и «жидкие» фиолетовые разводы, как на фоне TikTok You metal
    rr = 'x="20" y="20" width="472" height="472" rx="118"'
    return "\n".join([
        f'<rect {rr} fill="url(#{p}tile)"/>',
        f'<g transform="rotate(-32 175 160)"><ellipse cx="175" cy="160" rx="135" ry="46" fill="url(#{p}sw1)"/></g>',
        f'<g transform="rotate(-32 175 150)"><ellipse cx="180" cy="146" rx="96" ry="14" fill="url(#{p}sw2)"/></g>',
        f'<g transform="rotate(28 330 380)"><ellipse cx="330" cy="380" rx="120" ry="40" fill="url(#{p}sw1)"/></g>',
        f'<rect {rr} fill="url(#{p}gloss)"/>',
        f'<rect x="21.5" y="21.5" width="469" height="469" rx="116.5" fill="none" stroke="#FFFFFF" stroke-opacity="0.10" stroke-width="3"/>',
    ])


# ------------------------------------------------------- линии глобуса
def globe_lines(r=R):
    """Меридианы и параллели внутри печеньки (пути SVG)."""
    k = r / 150
    lines = [
        f'M{f(C)},{f(C - 138 * k)} L{f(C)},{f(C + 138 * k)}',                                   # центральный меридиан
        f'M{f(C - 140 * k)},{f(C)} L{f(C + 140 * k)},{f(C)}',                                   # экватор
        f'M{f(C)},{f(C - 138 * k)} C{f(C - 82 * k)},{f(C - 100 * k)} {f(C - 82 * k)},{f(C + 100 * k)} {f(C)},{f(C + 138 * k)}',
        f'M{f(C)},{f(C - 138 * k)} C{f(C + 82 * k)},{f(C - 100 * k)} {f(C + 82 * k)},{f(C + 100 * k)} {f(C)},{f(C + 138 * k)}',
        f'M{f(C - 116 * k)},{f(C - 72 * k)} Q{f(C)},{f(C - 56 * k)} {f(C + 116 * k)},{f(C - 72 * k)}',   # параллели
        f'M{f(C - 116 * k)},{f(C + 72 * k)} Q{f(C)},{f(C + 88 * k)} {f(C + 116 * k)},{f(C + 72 * k)}',
    ]
    return lines


TILT = -18   # наклон оси глобуса, градусов


def tilted(svg_part, dy=0):
    return f'<g transform="translate(0 {dy}) rotate({TILT} {C} {C})">{svg_part}</g>'


def stroke(d, color, width, opacity=1.0, cap="round"):
    return (f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{f(width)}" '
            f'stroke-opacity="{opacity}" stroke-linecap="{cap}" stroke-linejoin="round"/>')


def glow(d, color, core, widths=(46, 30, 18), core_w=10):
    """Неоновое свечение: несколько полупрозрачных обводок + яркая сердцевина."""
    out = [stroke(d, color, w, op) for w, op in zip(widths, (0.07, 0.14, 0.32))]
    out.append(stroke(d, color, core_w + 6, 0.9))
    out.append(stroke(d, core, core_w * 0.55, 1.0))
    return "\n".join(out)


# ------------------------------------------------------------- E: неон
def variant_e():
    p = "e"
    outline = cookie_outline(C, C, R, WAVES, DEPTH)
    body = [tile(p)]
    # мягкое пятно света за глобусом
    body.append(f'<circle cx="{C}" cy="{C}" r="190" fill="url(#{p}halo)"/>')
    body.append(glow(outline, "#A66BFF", "#F1E6FF"))
    for d in globe_lines(R - 26):
        body.append(tilted(glow(d, "#FFD43B", "#FFF8C4", widths=(30, 20, 12), core_w=7)))
    defs = tile_defs(p) + (f'\n<radialGradient id="{p}halo" cx="0.5" cy="0.5" r="0.5">'
                           '<stop offset="0" stop-color="#8E5CFF" stop-opacity="0.28"/>'
                           '<stop offset="1" stop-color="#8E5CFF" stop-opacity="0"/></radialGradient>')
    return svg("\n".join(body), defs)


def variant_e_small():
    p = "es"
    outline = cookie_outline(C, C, 176, 9, 0.10)
    body = [f'<rect x="8" y="8" width="496" height="496" rx="120" fill="#1B0D42"/>',
            stroke(outline, "#B07CFF", 40),
            tilted(stroke(f'M{C},{C - 150} L{C},{C + 150}', "#FFD43B", 30)),
            tilted(stroke(f'M{C - 152},{C} L{C + 152},{C}', "#FFD43B", 30))]
    return svg("\n".join(body), "")


# ------------------------------------------------------------ F: металл
def variant_f():
    p = "f"
    outline = cookie_outline(C, C, R, WAVES, DEPTH)
    inner = cookie_outline(C, C, R - 14, WAVES, DEPTH)
    defs = tile_defs(p) + (
        f'\n<linearGradient id="{p}chrome" x1="0.1" y1="0.05" x2="0.9" y2="0.95">'
        '<stop offset="0" stop-color="#FFFFFF"/><stop offset="0.18" stop-color="#E4DAFF"/>'
        '<stop offset="0.36" stop-color="#B9A4FF"/><stop offset="0.5" stop-color="#FF9BD8"/>'
        '<stop offset="0.62" stop-color="#8FF0FF"/><stop offset="0.78" stop-color="#9C86FF"/>'
        '<stop offset="1" stop-color="#F3EEFF"/></linearGradient>\n'
        f'<linearGradient id="{p}edge" x1="0" y1="0" x2="1" y2="1">'
        '<stop offset="0" stop-color="#FFFFFF"/><stop offset="1" stop-color="#4A35A8"/></linearGradient>\n'
        f'<radialGradient id="{p}spec" cx="0.32" cy="0.26" r="0.4">'
        '<stop offset="0" stop-color="#FFFFFF" stop-opacity="0.95"/>'
        '<stop offset="1" stop-color="#FFFFFF" stop-opacity="0"/></radialGradient>\n'
        f'<radialGradient id="{p}drop" cx="0.5" cy="0.5" r="0.5">'
        '<stop offset="0" stop-color="#000000" stop-opacity="0.55"/>'
        '<stop offset="1" stop-color="#000000" stop-opacity="0"/></radialGradient>')
    body = [tile(p),
            f'<ellipse cx="{C}" cy="{C + 150}" rx="140" ry="26" fill="url(#{p}drop)"/>',
            f'<path d="{outline}" fill="url(#{p}edge)"/>',              # кант-«фаска»
            f'<path d="{inner}" fill="url(#{p}chrome)"/>']
    # гравировка: тёмная канавка + светлый край снизу
    for d in globe_lines(R - 18):
        body.append(tilted(stroke(d, "#4B37B0", 10, 0.55)))
        body.append(tilted(stroke(d, "#FFFFFF", 4, 0.55), dy=2.5))
    body.append(f'<path d="{inner}" fill="url(#{p}spec)"/>')
    return svg("\n".join(body), defs)


def variant_f_small():
    p = "fs"
    outline = cookie_outline(C, C, 180, 9, 0.10)
    defs = (f'<linearGradient id="{p}c" x1="0.1" y1="0" x2="0.9" y2="1">'
            '<stop offset="0" stop-color="#FFFFFF"/><stop offset="0.45" stop-color="#C2B0FF"/>'
            '<stop offset="0.7" stop-color="#FF9BD8"/><stop offset="1" stop-color="#8FE9FF"/></linearGradient>')
    body = [f'<rect x="8" y="8" width="496" height="496" rx="120" fill="#1B0D42"/>',
            f'<path d="{outline}" fill="url(#{p}c)"/>',
            tilted(stroke(f'M{C},{C - 150} L{C},{C + 150}', "#3A2890", 26)),
            tilted(stroke(f'M{C - 152},{C} L{C + 152},{C}', "#3A2890", 26))]
    return svg("\n".join(body), defs)


# ----------------------------------------------------------- G: карамель
def drip(x, y, length, width, color):
    """Подтёк карамели: линия с каплей на конце."""
    return (stroke(f'M{f(x)},{f(y)} L{f(x)},{f(y + length)}', color, width) +
            f'<circle cx="{f(x)}" cy="{f(y + length)}" r="{f(width * 0.85)}" fill="{color}"/>')


def variant_g():
    p = "g"
    outline = cookie_outline(C, C - 8, R, WAVES, DEPTH)
    defs = tile_defs(p) + (
        f'\n<linearGradient id="{p}car" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#F6CF78"/><stop offset="0.55" stop-color="#E0A845"/>'
        '<stop offset="1" stop-color="#B97A26"/></linearGradient>')
    body = [tile(p)]
    for x, ln, w in [(178, 46, 10), (214, 84, 8), (258, 34, 7), (296, 66, 9), (334, 30, 7)]:
        edge = C - 8 + math.sqrt(R * R - (x - C) ** 2) - 14   # чуть выше нижнего края
        body.append(drip(x, edge, ln, w, "#D99E3E"))
    body.append(f'<path d="{outline}" fill="url(#{p}car)"/>')
    # меридианы «прорезаны» — цветом плитки
    for d in globe_lines(R - 24):
        body.append(tilted(stroke(d, "#1E0F45", 11), dy=-8))
    body.append(f'<path d="{outline}" fill="none" stroke="#FFE7AE" stroke-opacity="0.5" stroke-width="4"/>')
    return svg("\n".join(body), defs)


def variant_g_small():
    outline = cookie_outline(C, C, 180, 9, 0.10)
    body = [f'<rect x="8" y="8" width="496" height="496" rx="120" fill="#1B0D42"/>',
            f'<path d="{outline}" fill="#E8B04E"/>',
            tilted(stroke(f'M{C},{C - 150} L{C},{C + 150}', "#1B0D42", 26)),
            tilted(stroke(f'M{C - 152},{C} L{C + 152},{C}', "#1B0D42", 26))]
    return svg("\n".join(body), "")


if __name__ == "__main__":
    for key, big, small in (("e", variant_e, variant_e_small), ("f", variant_f, variant_f_small),
                            ("g", variant_g, variant_g_small)):
        for suffix, fn in (("", big), ("-small", small)):
            with open(os.path.join(OUT, f"crumb-{key}{suffix}.svg"), "w", encoding="utf-8") as fh:
                fh.write(fn())
    # Общая страница сравнения всех вариантов
    names = {"a": "Карамель-печенье", "b": "Лаванда", "c": "Кольцо", "d": "Фиолетовая «C»",
             "e": "Глобус · неон", "f": "Глобус · металл", "g": "Глобус · карамель"}
    cards = []
    for key, name in names.items():
        if not os.path.exists(os.path.join(OUT, f"crumb-{key}.svg")):
            continue
        sizes = "".join(f'<img src="crumb-{key}{"-small" if px <= 32 else ""}.svg" width="{px}" height="{px}">'
                        for px in (256, 128, 64, 48, 32, 24, 16))
        cards.append(f'<section><h2>{key.upper()} · {name}</h2><div class="row light">{sizes}</div>'
                     f'<div class="row dark">{sizes}</div></section>')
    html = ("<!doctype html><html lang=ru><meta charset=utf-8><title>Иконка Crumb — варианты</title>"
            "<style>body{font-family:'Segoe UI',sans-serif;margin:32px;background:#f4f1f8;color:#1d1b20}"
            "section{margin-bottom:32px}h2{margin:0 0 10px}.row{display:flex;align-items:flex-end;gap:24px;"
            "padding:20px;border-radius:20px;margin-bottom:8px}.light{background:#fff}.dark{background:#1c1b1f}</style>"
            "<h1>Crumb — варианты иконки</h1><p>256 · 128 · 64 · 48 · 32 · 24 · 16 px</p>" + "".join(cards) + "</html>")
    with open(os.path.join(OUT, "preview.html"), "w", encoding="utf-8") as fh:
        fh.write(html)
    print("ok: e (неон), f (металл), g (карамель) + preview.html")
