#pragma once
// CefClient одной вкладки. Все колбэки приходят на UI-поток CEF; наружу
// (в BrowserView, который живёт в потоке Qt) пересылаем через очередь событий.

#include <QPointer>
#include <QString>
#include <memory>
#include <mutex>

#include "include/cef_client.h"

namespace crumb {

class BrowserView;

// Связь клиента с QML-элементом. BrowserView обнуляет её в деструкторе,
// поэтому клиент (который может жить дольше) не обратится к удалённому объекту.
struct ViewLink {
  std::mutex mutex;
  BrowserView* view = nullptr;
};

class BrowserClient final : public CefClient,
                            public CefLifeSpanHandler,
                            public CefDisplayHandler,
                            public CefLoadHandler,
                            public CefKeyboardHandler,
                            public CefCommandHandler,
                            public CefDownloadHandler,
                            public CefFindHandler,
                            public CefRequestHandler,
                            public CefContextMenuHandler {
 public:
  explicit BrowserClient(std::shared_ptr<ViewLink> link) : link_(std::move(link)) {}

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
  CefRefPtr<CefCommandHandler> GetCommandHandler() override { return this; }
  CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
  CefRefPtr<CefFindHandler> GetFindHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }

  // CefRequestHandler — системные страницы Chromium (chrome://, about:) в Crumb не открываются:
  // вместо них — свои страницы Crumb или ничего.
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request,
                      bool user_gesture, bool is_redirect) override;
  // CefContextMenuHandler — убираем из меню Chrome пункты, которых в Crumb нет
  // (окна инкогнито, Google Lens, перевод, отправка на устройства и т.п.).
  void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model) override;

  // CefFindHandler — результат поиска по странице («3 из 17»).
  void OnFindResult(CefRefPtr<CefBrowser> browser, int identifier, int count, const CefRect& selectionRect,
                    int activeMatchOrdinal, bool finalUpdate) override;

  // CefDownloadHandler — сохранять в папку из настроек (или спрашивать).
  bool OnBeforeDownload(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> item,
                        const CefString& suggested_name,
                        CefRefPtr<CefBeforeDownloadCallback> callback) override;
  void OnDownloadUpdated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> item,
                         CefRefPtr<CefDownloadItemCallback> callback) override;

  // CefDisplayHandler — служебные сообщения страниц Crumb (CRUMB_…) в BrowserView.
  bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
                        const CefString& message, const CefString& source, int line) override;

  // CefLifeSpanHandler
  bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int popup_id,
                     const CefString& target_url, const CefString& target_frame_name,
                     CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture,
                     const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client, CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue>& extra_info,
                     bool* no_javascript_access) override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefDisplayHandler
  void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                       const CefString& url) override;
  void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
  void OnLoadingProgressChange(CefRefPtr<CefBrowser> browser, double progress) override;
  void OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
                          const std::vector<CefString>& icon_urls) override;

  // CefLoadHandler
  void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack,
                            bool canGoForward) override;

  // CefKeyboardHandler — перехватываем горячие клавиши, пока фокус в странице.
  bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
                     CefEventHandle os_event, bool* is_keyboard_shortcut) override;

  // CefCommandHandler — команды Chrome style (Ctrl+T, Ctrl+N…). Не даём Chromium
  // открывать собственные окна: эти действия выполняет интерфейс Crumb.
  bool OnChromeCommand(CefRefPtr<CefBrowser> browser, int command_id,
                       cef_window_open_disposition_t disposition) override;

  // Переслать лямбду в BrowserView (в потоке Qt), если он ещё жив.
  template <class F>
  static void postToView(const std::shared_ptr<ViewLink>& link, F&& fn);

 private:
  template <class F>
  void toView(F&& fn) { postToView(link_, std::forward<F>(fn)); }

  std::shared_ptr<ViewLink> link_;
  IMPLEMENT_REFCOUNTING(BrowserClient);
};

}  // namespace crumb
