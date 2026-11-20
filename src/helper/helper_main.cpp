// crumb_helper.exe — вспомогательные процессы Chromium (рендерер, GPU, сеть, утилиты).
// Без Qt, чтобы каждый процесс стартовал быстро и ел меньше памяти.
// На этапе 7 сюда добавится CefRenderProcessHandler для снимков Rewind.

#include "include/cef_app.h"

#if defined(_WIN32)
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  CefMainArgs args(instance);
  return CefExecuteProcess(args, nullptr, nullptr);
}
#else
int main(int argc, char* argv[]) {
  CefMainArgs args(argc, argv);
  return CefExecuteProcess(args, nullptr, nullptr);
}
#endif
