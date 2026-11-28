#pragma once
// Окна Chromium внутри окна Crumb — без привязки к ОС в остальном коде.
// Windows: HWND (SetWindowPos/ShowWindowAsync). Linux: окно X11 (Xlib; на Wayland —
// через XWayland, как и сам Crumb: Qt запускается с QT_QPA_PLATFORM=xcb).

#include <QRect>
#include <QtGlobal>

namespace crumb::native {

// h — CefWindowHandle браузера (HWND или X11 Window), приведённый к quintptr.
void moveResize(quintptr h, const QRect& physicalRect);
void setVisible(quintptr h, bool visible);
inline void hide(quintptr h) { setVisible(h, false); }

// Linux: обработчики ошибок X11, иначе любая ошибка Xlib (окно уже закрыто и т.п.)
// завершает процесс. Вызвать один раз в main(). На Windows ничего не делает.
void installXErrorHandlers();

}  // namespace crumb::native
