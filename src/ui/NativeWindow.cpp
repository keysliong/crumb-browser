#include "ui/NativeWindow.h"

#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
// Xlib — последним: его макросы (None, Bool, Status…) конфликтуют с Qt.
#include <X11/Xlib.h>
#endif

namespace crumb::native {

#if defined(_WIN32)

void moveResize(quintptr h, const QRect& r) {
  if (!h) return;
  // Окно браузера принадлежит UI-потоку CEF: асинхронный SetWindowPos не ждёт его.
  ::SetWindowPos(reinterpret_cast<HWND>(h), nullptr, r.x(), r.y(), r.width(), r.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
}

void setVisible(quintptr h, bool visible) {
  if (h) ::ShowWindowAsync(reinterpret_cast<HWND>(h), visible ? SW_SHOWNA : SW_HIDE);
}

void installXErrorHandlers() {}

#elif defined(__linux__)

namespace {
// Своё соединение с X-сервером: идентификаторы окон глобальные, а соединения
// Qt (xcb) и Chromium трогать не нужно.
Display* display() {
  static Display* d = XOpenDisplay(nullptr);
  return d;
}
int onXError(Display*, XErrorEvent*) { return 0; }
int onXIoError(Display*) { return 0; }
}  // namespace

void moveResize(quintptr h, const QRect& r) {
  Display* d = display();
  if (!d || !h) return;
  XMoveResizeWindow(d, static_cast<Window>(h), r.x(), r.y(), unsigned(std::max(1, r.width())),
                    unsigned(std::max(1, r.height())));
  XFlush(d);
}

void setVisible(quintptr h, bool visible) {
  Display* d = display();
  if (!d || !h) return;
  if (visible) XMapWindow(d, static_cast<Window>(h));
  else XUnmapWindow(d, static_cast<Window>(h));
  XFlush(d);
}

void installXErrorHandlers() {
  XSetErrorHandler(onXError);
  XSetIOErrorHandler(onXIoError);
}

#else

void moveResize(quintptr, const QRect&) {}
void setVisible(quintptr, bool) {}
void installXErrorHandlers() {}

#endif

}  // namespace crumb::native
