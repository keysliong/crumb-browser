// Подтверждение личности перед показом паролей: Windows Hello (PIN, лицо, отпечаток),
// а если он не настроен — пароль учётной записи Windows. Отдельный файл без Qt:
// заголовки C++/WinRT и макросы Qt (emit, signals) не дружат.

#include "passwords/WinVerify.h"

#if defined(_WIN32)
#include <windows.h>
#include <wincred.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Credentials.UI.h>
#include <UserConsentVerifierInterop.h>

#include <string>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "credui.lib")
#pragma comment(lib, "advapi32.lib")

namespace crumb::winverify {

namespace {
using namespace winrt::Windows::Security::Credentials::UI;
using winrt::Windows::Foundation::IAsyncOperation;

// 1 — подтверждено, 0 — отказ/отмена, -1 — Hello недоступен.
int tryHello(void* hwnd, const std::wstring& message) {
  try {
    if (UserConsentVerifier::CheckAvailabilityAsync().get() != UserConsentVerifierAvailability::Available) return -1;
    auto interop = winrt::get_activation_factory<UserConsentVerifier, IUserConsentVerifierInterop>();
    IAsyncOperation<UserConsentVerificationResult> op{nullptr};
    winrt::hstring msg(message);
    winrt::check_hresult(interop->RequestVerificationForWindowAsync(
        static_cast<HWND>(hwnd), reinterpret_cast<HSTRING>(winrt::get_abi(msg)),
        winrt::guid_of<IAsyncOperation<UserConsentVerificationResult>>(), winrt::put_abi(op)));
    return op.get() == UserConsentVerificationResult::Verified ? 1 : 0;
  } catch (...) {
    return -1;
  }
}

// Запасной путь: системное окно «Введите пароль Windows» и проверка через LogonUser.
bool tryWindowsPassword(void* hwnd, const std::wstring& message) {
  CREDUI_INFOW info{sizeof(info)};
  info.hwndParent = static_cast<HWND>(hwnd);
  info.pszCaptionText = L"Crumb";
  info.pszMessageText = message.c_str();
  ULONG pkg = 0;
  void* out = nullptr;
  ULONG outSize = 0;
  BOOL save = FALSE;
  for (int attempt = 0; attempt < 3; ++attempt) {
    const DWORD rc = CredUIPromptForWindowsCredentialsW(&info, attempt ? ERROR_LOGON_FAILURE : 0, &pkg, nullptr, 0,
                                                        &out, &outSize, &save, CREDUIWIN_ENUMERATE_CURRENT_USER);
    if (rc != ERROR_SUCCESS) return false;   // отмена
    wchar_t user[CREDUI_MAX_USERNAME_LENGTH + 1]{}, domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1]{},
        pass[CREDUI_MAX_PASSWORD_LENGTH + 1]{};
    DWORD ul = CREDUI_MAX_USERNAME_LENGTH, dl = CREDUI_MAX_DOMAIN_TARGET_LENGTH, pl = CREDUI_MAX_PASSWORD_LENGTH;
    const BOOL ok = CredUnPackAuthenticationBufferW(0, out, outSize, user, &ul, domain, &dl, pass, &pl);
    SecureZeroMemory(out, outSize);
    CoTaskMemFree(out);
    out = nullptr;
    if (!ok) continue;
    // «DOMAIN\user» или «MicrosoftAccount\mail@…»
    std::wstring u = user, d = domain;
    if (d.empty()) {
      const auto slash = u.find(L'\\');
      if (slash != std::wstring::npos) { d = u.substr(0, slash); u = u.substr(slash + 1); }
    }
    HANDLE token = nullptr;
    const BOOL logged = LogonUserW(u.c_str(), d.empty() ? nullptr : d.c_str(), pass, LOGON32_LOGON_INTERACTIVE,
                                   LOGON32_PROVIDER_DEFAULT, &token);
    SecureZeroMemory(pass, sizeof(pass));
    if (logged) {
      CloseHandle(token);
      return true;
    }
  }
  return false;
}
}  // namespace

bool verify(void* hwnd, const std::wstring& message) {
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  const int hello = tryHello(hwnd, message);
  bool ok = hello == 1;
  if (hello < 0) ok = tryWindowsPassword(hwnd, message);
  winrt::uninit_apartment();
  return ok;
}

}  // namespace crumb::winverify

#else
namespace crumb::winverify {
bool verify(void*, const std::wstring&) { return true; }
}  // namespace crumb::winverify
#endif
