#!/usr/bin/env bash
# Собрать Crumb-x86_64.AppImage из готовой сборки (build/out/Release).
#   QT_DIR=~/Qt/6.10.3/gcc_64 scripts/make-appimage.sh
# Нужны: appimagetool (скачивается сам), patchelf не нужен — пути задаёт AppRun.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build/out/Release}"
QT_DIR="${QT_DIR:?укажите QT_DIR, например ~/Qt/6.10.3/gcc_64}"
APPDIR="$ROOT/build/AppDir"
VERSION="${VERSION:-$(grep -oP 'project\(Crumb VERSION \K[0-9.]+' "$ROOT/CMakeLists.txt")}"
DEST="$ROOT/build/Crumb-$VERSION-x86_64.AppImage"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/lib/crumb" "$APPDIR/usr/lib" "$APPDIR/usr/plugins" "$APPDIR/usr/qml"

# 1. Crumb и CEF (libcef.so, *.pak, icudtl.dat, locales, snapshot…) — одной папкой:
#    CEF ищет свои файлы рядом с программой.
cp -a "$OUT/CrumbBrowser" "$OUT/crumb_helper" "$APPDIR/usr/lib/crumb/"
for f in "$OUT"/*.so "$OUT"/*.so.* "$OUT"/*.pak "$OUT"/*.dat "$OUT"/*.bin "$OUT"/*.json; do
  [ -e "$f" ] && cp -a "$f" "$APPDIR/usr/lib/crumb/"
done
cp -a "$OUT/locales" "$APPDIR/usr/lib/crumb/"
rm -f "$APPDIR/usr/lib/crumb/chrome-sandbox"   # песочница выключена (no_sandbox)

# 2. Плагины и QML-модули Qt, которые нужны Crumb.
for p in platforms xcbglintegrations imageformats iconengines platforminputcontexts tls sqldrivers \
         platformthemes wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client; do
  [ -d "$QT_DIR/plugins/$p" ] && cp -a "$QT_DIR/plugins/$p" "$APPDIR/usr/plugins/"
done
for m in QtQuick QtQml QtCore QtQuick.2 Qt; do
  [ -d "$QT_DIR/qml/$m" ] && cp -a "$QT_DIR/qml/$m" "$APPDIR/usr/qml/"
done

# 3. Библиотеки Qt: всё, от чего зависят программа, плагины и QML-модули.
collect() {
  find "$APPDIR/usr/lib/crumb/CrumbBrowser" "$APPDIR/usr/plugins" "$APPDIR/usr/qml" -type f \( -name '*.so*' -o -name CrumbBrowser \) \
    -exec env LD_LIBRARY_PATH="$QT_DIR/lib" ldd {} \; 2>/dev/null | grep -oP '=> \K/\S+' | sort -u \
    | while read -r p; do
        # Только библиотеки Qt; копируем под именем из ldd (libQt6Core.so.6), а не под полным.
        case "$(readlink -f "$p")" in "$(readlink -f "$QT_DIR")"/lib/*) echo "$p";; esac
      done
}
for pass in 1 2 3; do
  for lib in $(collect); do cp -nL "$lib" "$APPDIR/usr/lib/$(basename "$lib")"; done
done
# Qt ищет ICU рядом с собой
cp -n --preserve=links "$QT_DIR"/lib/libicu*.so* "$APPDIR/usr/lib/" 2>/dev/null || true

# Небольшие системные библиотеки X11/xcb, без которых не запускается Qt (на многих
# системах их нет по умолчанию: xcb-util-cursor, xcb-util-wm, xkbcommon-x11…).
for so in "$APPDIR"/usr/plugins/platforms/libqxcb.so "$APPDIR"/usr/lib/libQt6XcbQpa.so*; do
  [ -e "$so" ] || continue
  env LD_LIBRARY_PATH="$APPDIR/usr/lib" ldd "$so" 2>/dev/null \
    | grep -oP '=> \K/\S+(libxcb-(cursor|icccm|image|keysyms|render-util|util|xinput|xkb)|libxkbcommon-x11)\S*' \
    | sort -u | while read -r lib; do cp -anL "$lib" "$APPDIR/usr/lib/"; done
done

# 4. Запуск, ярлык, значок.
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib/crumb:$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export QML_IMPORT_PATH="$HERE/usr/qml"
export QML2_IMPORT_PATH="$HERE/usr/qml"
# Окна Chromium встраиваются через X11; на Wayland работает через XWayland.
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
exec "$HERE/usr/lib/crumb/CrumbBrowser" "$@"
EOF
chmod +x "$APPDIR/AppRun"
cat > "$APPDIR/crumb.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Crumb
GenericName=Web Browser
GenericName[ru]=Веб-браузер
Comment=Browser in Material You style
Comment[ru]=Браузер в стиле Material You
Exec=AppRun %U
Icon=crumb
Terminal=false
StartupWMClass=crumb
Categories=Network;WebBrowser;
MimeType=text/html;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
EOF
cp "$ROOT/resources/icons/build/crumb-256.png" "$APPDIR/crumb.png"
cp "$APPDIR/crumb.png" "$APPDIR/.DirIcon"

# 5. Упаковка.
TOOL="$ROOT/build/appimagetool"
if [ ! -x "$TOOL" ]; then
  curl -fsSL -o "$TOOL" https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
  chmod +x "$TOOL"
fi
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$TOOL" --no-appstream "$APPDIR" "$DEST"
echo "Готово: $DEST"
