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

// Родитель для окна Chromium внутри окна Qt. Windows: само окно Qt.
// Linux: отдельное окно X11 с обычной палитрой экрана внутри окна Qt — окно Qt
// создано под OpenGL (своя палитра), и Chromium не может стать его прямым
// потомком (X11 MatchError). Дальше двигаем/прячем это промежуточное окно.
quintptr childHost(quintptr qtWindow, const QRect& physicalRect, bool visible);
void destroyHost(quintptr host);
// true — у этой ОС окно Chromium живёт внутри промежуточного окна (childHost).
bool usesHost();

// Linux: обработчики ошибок X11, иначе любая ошибка Xlib (окно уже закрыто и т.п.)
// завершает процесс. Вызвать один раз в main(). На Windows ничего не делает.
void installXErrorHandlers();

}  // namespace crumb::native
