#include "cef/CrumbCefApp.h"

#include <QStringList>

#include "cef/CefRuntime.h"
#include "cef/CefTask.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace crumb {

namespace {

// Клиент для браузеров, которые Chromium создал без нашего участия: окна
// Chrome (chrome.tabs.create из расширений, «открыть в новом окне» и т.п.).
// Своё окно Chromium мы не показываем: как только станет известен адрес,
// он открывается обычной вкладкой Crumb, а этот браузер закрывается.
class StrayClient final : public CefClient, public CefLifeSpanHandler, public CefDisplayHandler {
 public:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    // Учитываем в счётчике, чтобы выход дождался их закрытия.
    CefRuntime::instance()->browserCreated();
#if defined(_WIN32)
    ::ShowWindow(browser->GetHost()->GetWindowHandle(), SW_HIDE);
#endif
    // Если адреса так и не появится (пустое окно) — закрываем через пару секунд.
    CefRefPtr<CefBrowser> b = browser;
    CefPostDelayedTask(TID_UI, new FunctionTask([b] { b->GetHost()->CloseBrowser(true); }), 3000);
  }
  void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                       const CefString& url) override {
    const QString u = QString::fromStdWString(url.ToWString());
    if (!frame->IsMain() || u.isEmpty() || u == QLatin1String("about:blank")) return;
    CefRuntime::instance()->notifyStrayUrl(u);
    browser->GetHost()->CloseBrowser(true);
  }
  void OnBeforeClose(CefRefPtr<CefBrowser>) override { CefRuntime::instance()->browserClosed(); }

 private:
  IMPLEMENT_REFCOUNTING(StrayClient);
};

}  // namespace

CefRefPtr<CefClient> CrumbCefApp::GetDefaultClient() {
  if (!strayClient_) strayClient_ = new StrayClient();
  return strayClient_;
}

void CrumbCefApp::OnBeforeCommandLineProcessing(const CefString& process_type,
                                                CefRefPtr<CefCommandLine> command_line) {
  if (!process_type.empty()) return;  // только основной процесс
  // Не показывать предложение «сделать браузером по умолчанию» от самого Chromium.
  command_line->AppendSwitch("no-default-browser-check");
  command_line->AppendSwitch("no-first-run");
  // Восстановлением сессии займётся сам Crumb (вкладки — этап 2), а не Chromium.
  command_line->AppendSwitch("disable-session-crashed-bubble");
  // Звук и видео могут стартовать без жеста пользователя: плееры (Яндекс Музыка и
  // т.п.) сами переключают треки и запускают воспроизведение из скриптов.
  command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
}

void CrumbCefApp::OnContextInitialized() {
  CefRuntime::instance()->notifyContextReady();
}

bool CrumbCefApp::OnAlreadyRunningAppRelaunch(CefRefPtr<CefCommandLine> command_line,
                                              const CefString& /*current_directory*/) {
  QStringList urls;
  CefCommandLine::ArgumentList args;
  command_line->GetArguments(args);
  for (const auto& a : args) urls << QString::fromStdWString(a.ToWString());
  CefRuntime::instance()->notifyRelaunch(urls);
  return true;
}

}  // namespace crumb
