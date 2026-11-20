// Crumb-Setup.exe / Crumb-Uninstall.exe — маленький стартовый exe без Qt.
// Распаковывает во временную папку только интерфейс установщика (Qt + crumb_setup.exe),
// запускает его, ждёт завершения и убирает за собой. Сам браузер установщик
// распаковывает из этого же файла прямо в папку установки.
// Режим удаления: имя файла содержит «uninstall» или аргумент --uninstall.

#include "../common/Payload.h"  // первым: тянет windows.h

#include <shellapi.h>

#include <algorithm>
#include <cwctype>

namespace fs = std::filesystem;
using namespace crumb::payload;

namespace {

void fail(const std::wstring& text) {
  MessageBoxW(nullptr, text.c_str(), L"Установка Crumb", MB_OK | MB_ICONERROR);
}

std::wstring quote(const std::wstring& s) { return L"\"" + s + L"\""; }

}  // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
  wchar_t self[MAX_PATH * 4] = {};
  GetModuleFileNameW(nullptr, self, DWORD(std::size(self)));
  const fs::path selfPath(self);

  std::wstring stem = selfPath.stem().wstring();
  std::transform(stem.begin(), stem.end(), stem.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
  bool uninstall = stem.find(L"uninstall") != std::wstring::npos || stem.find(L"удал") != std::wstring::npos;
  int argc = 0;
  if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
    for (int i = 1; i < argc; ++i)
      if (std::wstring(argv[i]) == L"--uninstall") uninstall = true;
    LocalFree(argv);
  }

  Archive archive;
  if (!archive.open(selfPath.wstring())) {
    fail(L"Установочный файл повреждён или неполный. Скачайте Crumb-Setup.exe заново.");
    return 1;
  }

  wchar_t tmp[MAX_PATH] = {};
  GetTempPathW(MAX_PATH, tmp);
  const fs::path temp = fs::path(tmp) / (L"CrumbSetup-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code ec;
  fs::remove_all(temp, ec);
  fs::create_directories(temp, ec);

  SetCursor(LoadCursorW(nullptr, IDC_APPSTARTING));
  for (const auto& e : archive.index().entries) {
    if (!(e.flags & kFlagUi)) continue;
    std::wstring err;
    if (!archive.extract(e, temp / widen(e.path), [](uint64_t) {}, &err)) {
      fail(L"Не удалось подготовить установку.\n\n" + err);
      fs::remove_all(temp, ec);
      return 1;
    }
  }
  archive.close();

  const fs::path ui = temp / L"crumb_setup.exe";
  std::wstring cmd = quote(ui.wstring()) + L" --payload " + quote(selfPath.wstring());
  if (uninstall) cmd += L" --uninstall";

  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  DWORD code = 1;
  if (CreateProcessW(ui.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                     temp.c_str(), &si, &pi)) {
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  } else {
    fail(L"Не удалось запустить установщик (код " + std::to_wstring(GetLastError()) + L").");
  }

  // Убираем временную папку; DLL могут освобождаться не мгновенно — несколько попыток.
  for (int i = 0; i < 20; ++i) {
    fs::remove_all(temp, ec);
    if (!fs::exists(temp)) break;
    Sleep(250);
  }
  return int(code);
}
