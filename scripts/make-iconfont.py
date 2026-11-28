#!/usr/bin/env python3
"""Шрифт значков «Crumb Icons» для Linux (и других систем без Segoe Fluent Icons).

Интерфейс Crumb рисует значки символами из области PUA шрифта Segoe Fluent Icons
(например, "\\uE72B" — назад). Этого шрифта нет вне Windows, а распространять его нельзя.
Скрипт берёт свободный Material Symbols Rounded (Apache 2.0), делает из переменного
шрифта обычный (вес 400, без заливки), оставляет только нужные значки и кладёт их
на те же коды, что у Segoe. Код QML менять не нужно.

    pip install fonttools brotli
    python3 scripts/make-iconfont.py ui/fonts/CrumbIcons.ttf
"""
import sys
import urllib.request
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset

BASE = "https://raw.githubusercontent.com/google/material-design-icons/master/variablefont/"
NAME = "MaterialSymbolsRounded[FILL,GRAD,opsz,wght]"

# Код Segoe Fluent Icons → имя значка Material Symbols.
MAP = {
    0xE706: "light_mode", 0xE708: "dark_mode", 0xE70B: "sticky_note_2", 0xE70D: "expand_more",
    0xE70E: "expand_less", 0xE70F: "edit", 0xE710: "add", 0xE711: "close", 0xE713: "settings",
    0xE714: "videocam", 0xE718: "keep", 0xE71C: "filter_list", 0xE721: "search", 0xE72A: "arrow_forward",
    0xE72B: "arrow_back", 0xE72C: "refresh", 0xE72E: "lock", 0xE734: "star", 0xE73E: "check",
    0xE74D: "delete", 0xE765: "keyboard", 0xE767: "volume_up", 0xE768: "play_arrow", 0xE769: "pause",
    0xE76E: "mood", 0xE76F: "drag_handle", 0xE774: "language", 0xE777: "open_in_full", 0xE77B: "person",
    0xE785: "lock_open", 0xE790: "palette", 0xE7A7: "undo", 0xE7AC: "apps", 0xE7B3: "visibility",
    0xE7C3: "description", 0xE7F4: "desktop_windows", 0xE80A: "grid_view", 0xE80F: "home",
    0xE81C: "history", 0xE823: "schedule", 0xE838: "folder_open", 0xE894: "clear_all", 0xE895: "sync",
    0xE896: "download", 0xE897: "help", 0xE8A0: "skip_previous", 0xE8A1: "skip_next",
    0xE8A5: "description", 0xE8A7: "open_in_new", 0xE8A9: "apps", 0xE8B7: "folder", 0xE8BB: "close",
    0xE8C8: "content_copy", 0xE8D2: "text_fields", 0xE8D6: "music_note", 0xE8D7: "key",
    0xE8E5: "file_open", 0xE8F1: "collections_bookmark", 0xE8FB: "done_all", 0xE8FC: "bookmarks",
    0xE916: "timer", 0xE91F: "circle", 0xE921: "remove", 0xE922: "crop_square", 0xE923: "filter_none",
    0xE946: "info", 0xE9CA: "cloud", 0xEA86: "extension", 0xEB9F: "image", 0xED1A: "visibility_off",
    0xEDE1: "ios_share", 0xEF3C: "animation", 0xF012: "cookie", 0xF3B1: "mouse",
}


def fetch(url: str, dest: Path) -> Path:
    if not dest.exists():
        urllib.request.urlretrieve(url, dest)
    return dest


def main() -> None:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "CrumbIcons.ttf")
    work = Path(sys.argv[2] if len(sys.argv) > 2 else ".")
    work.mkdir(parents=True, exist_ok=True)
    ttf = fetch(BASE + urllib.request.quote(NAME + ".ttf"), work / "MaterialSymbolsRounded.ttf")
    cps = fetch(BASE + urllib.request.quote(NAME + ".codepoints"), work / "MaterialSymbolsRounded.codepoints")

    names = {}
    for line in cps.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) == 2:
            names[parts[0]] = int(parts[1], 16)

    font = TTFont(ttf)
    # Обычный (не переменный) шрифт: контуры как у значков интерфейса Crumb.
    font = instancer.instantiateVariableFont(font, {"FILL": 0, "GRAD": 0, "opsz": 24, "wght": 400})
    cmap = font.getBestCmap()

    missing = [n for n in MAP.values() if n not in names or names[n] not in cmap]
    fallback = cmap[names["help"]]
    keep = set()
    new_cmap = {}
    for code, name in MAP.items():
        glyph = cmap[names[name]] if name in names and names[name] in cmap else fallback
        new_cmap[code] = glyph
        keep.add(glyph)

    # Оставить только нужные глифы, без лигатур Material.
    opts = subset.Options()
    opts.layout_features = []
    opts.notdef_outline = True
    opts.name_IDs = ["*"]
    sub = subset.Subsetter(opts)
    sub.populate(glyphs=sorted(keep))
    sub.subset(font)

    for table in font["cmap"].tables:
        if table.isUnicode():
            table.cmap = dict(new_cmap)

    for rec in font["name"].names:
        if rec.nameID in (1, 16):
            rec.string = "Crumb Icons"
        elif rec.nameID in (4,):
            rec.string = "Crumb Icons Regular"
        elif rec.nameID == 6:
            rec.string = "CrumbIcons-Regular"
    out.parent.mkdir(parents=True, exist_ok=True)
    font.save(out)
    print(f"{out}: {len(MAP)} значков" + (f", заменены на «help»: {missing}" if missing else ""))


if __name__ == "__main__":
    main()
