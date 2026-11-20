#include "cef/BrowserClient.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
#include <QMetaObject>

#include "cef/CefRuntime.h"
#include "rewind/RewindManager.h"
#include "downloads/DownloadManager.h"
#include "include/cef_command_ids.h"
#include "include/cef_image.h"
#include "ui/BrowserView.h"

namespace crumb {

namespace {
QString qs(const CefString& s) { return QString::fromStdWString(s.ToWString()); }

// Получает картинку иконки сайта и отдаёт её вкладке как data:-URL.
class FaviconCallback final : public CefDownloadImageCallback {
 public:
  explicit FaviconCallback(std::shared_ptr<ViewLink> link) : link_(std::move(link)) {}
  void OnDownloadImageFinished(const CefString&, int, CefRefPtr<CefImage> image) override {
    if (!image || image->IsEmpty()) return;
    int w = 0, h = 0;
    CefRefPtr<CefBinaryValue> png = image->GetAsPNG(2.0f, true, w, h);
    if (!png || png->GetSize() == 0) return;
    QByteArray bytes(int(png->GetSize()), Qt::Uninitialized);
    png->GetData(bytes.data(), bytes.size(), 0);
    const QString url = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
    BrowserClient::postToView(link_, [url](BrowserView* v) { v->setFaviconFromCef(url); });
  }

 private:
  std::shared_ptr<ViewLink> link_;
  IMPLEMENT_REFCOUNTING(FaviconCallback);
};

// Настоящее всплывающее окно (window.open с размерами): вход через Google / GitHub,
// оплата, OAuth. Таким окнам нужен window.opener — они возвращают результат
// открывшей странице, поэтому вкладкой их открывать нельзя.
class PopupClient final : public CefClient, public CefLifeSpanHandler, public CefDisplayHandler {
 public:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

  bool OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int, const CefString& target_url, const CefString&,
                     WindowOpenDisposition disposition, bool, const CefPopupFeatures&, CefWindowInfo&,
                     CefRefPtr<CefClient>&, CefBrowserSettings&, CefRefPtr<CefDictionaryValue>&, bool*) override {
    if (disposition == CEF_WOD_NEW_POPUP) return false;   // вложенный попап — тоже окном
    CefRuntime::instance()->notifyStrayUrl(qs(target_url));   // ссылка из попапа — вкладкой Crumb
    return true;
  }
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    CefRuntime::instance()->browserCreated();
    CefRuntime::instance()->trackBrowser(browser, true);   // закроется при выходе из Crumb
  }
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    CefRuntime::instance()->trackBrowser(browser, false);
    CefRuntime::instance()->browserClosed();
  }

 private:
  IMPLEMENT_REFCOUNTING(PopupClient);
};
}  // namespace

template <class F>
void BrowserClient::postToView(const std::shared_ptr<ViewLink>& link, F&& fn) {
  std::lock_guard lock(link->mutex);
  if (!link->view) return;
  // QPointer создаётся под замком: пока он держится, деструктор BrowserView
  // не может пройти. Дальше QPointer сам обнулится, если вид удалят до вызова.
  QPointer<BrowserView> view = link->view;
  QMetaObject::invokeMethod(
      link->view, [view, fn = std::forward<F>(fn)]() mutable {
        if (view) fn(view.data());
      },
      Qt::QueuedConnection);
}

bool BrowserClient::OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int,
                                  const CefString& target_url, const CefString&,
                                  WindowOpenDisposition disposition, bool, const CefPopupFeatures&,
                                  CefWindowInfo& windowInfo, CefRefPtr<CefClient>& client, CefBrowserSettings&,
                                  CefRefPtr<CefDictionaryValue>&, bool*) {
  // window.open(url, name, "width=…") — настоящее окно со связью window.opener,
  // как в Chrome (вход через Google на claude.ai и т.п.). Своё окно Chrome-стиля.
  if (disposition == CEF_WOD_NEW_POPUP) {
    windowInfo.runtime_style = CEF_RUNTIME_STYLE_CHROME;
    client = new PopupClient();
    return false;
  }
  // Обычные ссылки «в новой вкладке» — вкладкой Crumb.
  const QString url = qs(target_url);
  const bool background = disposition == CEF_WOD_NEW_BACKGROUND_TAB;
  toView([url, background](BrowserView* v) { v->handlePopup(url, background); });
  return true;
}

void BrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CefRuntime::instance()->browserCreated();
  CefRuntime::instance()->trackBrowser(browser, true);
  const auto hwnd = reinterpret_cast<quintptr>(browser->GetHost()->GetWindowHandle());
  toView([browser, hwnd](BrowserView* v) { v->attachBrowser(browser, hwnd); });
}

void BrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CefRuntime::instance()->trackBrowser(browser, false);
  toView([](BrowserView* v) { v->detachBrowser(); });
  CefRuntime::instance()->browserClosed();
}

void BrowserClient::OnAddressChange(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  if (!frame->IsMain()) return;
  const QString u = qs(url);
  toView([u](BrowserView* v) { v->setUrlFromCef(u); });
}

void BrowserClient::OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) {
  const QString t = qs(title);
  toView([t](BrowserView* v) { v->setTitleFromCef(t); });
}

void BrowserClient::OnLoadingProgressChange(CefRefPtr<CefBrowser>, double progress) {
  toView([progress](BrowserView* v) { v->setProgressFromCef(progress); });
}

void BrowserClient::OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
                                       const std::vector<CefString>& icon_urls) {
  if (icon_urls.empty()) {
    toView([](BrowserView* v) { v->setFaviconFromCef(QString()); });
    return;
  }
  browser->GetHost()->DownloadImage(icon_urls.front(), true, 32, false, new FaviconCallback(link_));
}

void BrowserClient::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
  // Rewind: снимок страницы после загрузки (только главный фрейм и удачный ответ).
  if (frame->IsMain() && httpStatusCode >= 200 && httpStatusCode < 400)
    RewindManager::instance()->onPageLoaded(browser);
}

void BrowserClient::OnLoadingStateChange(CefRefPtr<CefBrowser>, bool isLoading, bool canGoBack,
                                         bool canGoForward) {
  toView([=](BrowserView* v) { v->setLoadingStateFromCef(isLoading, canGoBack, canGoForward); });
}

namespace {
// Код клавиши Windows → клавиша Qt (для сравнения с сочетаниями из настроек).
int qtKeyFromVk(int vk) {
  if (vk >= 'A' && vk <= 'Z') return Qt::Key_A + (vk - 'A');
  if (vk >= '0' && vk <= '9') return Qt::Key_0 + (vk - '0');
  if (vk >= 0x70 && vk <= 0x87) return Qt::Key_F1 + (vk - 0x70);   // F1…F24
  switch (vk) {
    case 0x09: return Qt::Key_Tab;
    case 0x0D: return Qt::Key_Return;
    case 0x1B: return Qt::Key_Escape;
    case 0x20: return Qt::Key_Space;
    case 0x21: return Qt::Key_PageUp;
    case 0x22: return Qt::Key_PageDown;
    case 0x23: return Qt::Key_End;
    case 0x24: return Qt::Key_Home;
    case 0x25: return Qt::Key_Left;
    case 0x26: return Qt::Key_Up;
    case 0x27: return Qt::Key_Right;
    case 0x28: return Qt::Key_Down;
    case 0x2D: return Qt::Key_Insert;
    case 0x2E: return Qt::Key_Delete;
    case 0xBA: return Qt::Key_Semicolon;
    case 0xBB: return Qt::Key_Equal;
    case 0xBC: return Qt::Key_Comma;
    case 0xBD: return Qt::Key_Minus;
    case 0xBE: return Qt::Key_Period;
    case 0xBF: return Qt::Key_Slash;
    case 0xDB: return Qt::Key_BracketLeft;
    case 0xDD: return Qt::Key_BracketRight;
    default: return 0;
  }
}
}  // namespace

bool BrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser>, const CefKeyEvent& e, CefEventHandle,
                                  bool*) {
  if (e.type != KEYEVENT_RAWKEYDOWN) return false;
  const bool ctrl = e.modifiers & EVENTFLAG_CONTROL_DOWN;
  const bool alt = e.modifiers & EVENTFLAG_ALT_DOWN;
  const bool shift = e.modifiers & EVENTFLAG_SHIFT_DOWN;
  const int vk = e.windows_key_code;

  // Сочетания из настроек (их можно переназначить) — работают и в странице.
  if (const int key = qtKeyFromVk(vk)) {
    Qt::KeyboardModifiers mods;
    if (ctrl) mods |= Qt::ControlModifier;
    if (alt) mods |= Qt::AltModifier;
    if (shift) mods |= Qt::ShiftModifier;
    const QString seq = QKeySequence(QKeyCombination(mods, Qt::Key(key))).toString(QKeySequence::PortableText);
    const QString action = CefRuntime::instance()->actionForShortcut(seq);
    if (!action.isEmpty()) {
      toView([action](BrowserView* v) { emit v->shortcutTriggered(action); });
      return true;
    }
  }
  // Запасные сочетания для адресной строки, как в Chrome.
  constexpr int kF6 = 0x75;
  if ((alt && vk == 'D') || (!ctrl && !alt && vk == kF6)) {
    toView([](BrowserView* v) { emit v->addressFocusRequested(); });
    return true;
  }
  return false;
}

namespace {
// Пакет расширения (.crx) из Chrome Web Store — его ставит сам Chromium.
bool isExtensionPackage(CefRefPtr<CefDownloadItem> item) {
  const QString mime = QString::fromStdWString(item->GetMimeType().ToWString());
  const QString url = QString::fromStdWString(item->GetURL().ToWString());
  return mime == QLatin1String("application/x-chrome-extension") || url.contains(QLatin1String(".crx")) ||
         url.contains(QLatin1String("/crx/"));
}
}  // namespace

void BrowserClient::OnDownloadUpdated(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem> item,
                                      CefRefPtr<CefDownloadItemCallback> callback) {
  if (!item || !item->IsValid() || isExtensionPackage(item)) return;
  DownloadInfo d;
  d.cefId = item->GetId();
  d.path = qs(item->GetFullPath());
  d.name = d.path.isEmpty() ? qs(item->GetSuggestedFileName()) : QFileInfo(d.path).fileName();
  d.url = qs(item->GetOriginalUrl());
  d.received = item->GetReceivedBytes();
  d.total = item->GetTotalBytes();
  d.speed = item->GetCurrentSpeed();
  d.percent = item->GetPercentComplete();
  d.state = item->IsComplete() ? "done"
          : item->IsCanceled() ? "canceled"
          : item->IsInterrupted() ? "failed"
          : item->IsPaused() ? "paused" : "progress";
  d.started = QDateTime::currentDateTime();
  DownloadManager::instance()->update(d, callback);
}

bool BrowserClient::OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString& message,
                                     const CefString& source, int line) {
  const QString m = qs(message);
  if (m.startsWith(QLatin1String("CRUMB_RW")) && RewindManager::instance()->handleConsole(browser, m)) return true;
  if (!m.startsWith(QLatin1String("CRUMB_"))) {
    // Предупреждения и ошибки страниц — в журнал (если включён в настройках).
    if (level >= LOGSEVERITY_WARNING) CefRuntime::instance()->logConsole(int(level), m, qs(source), line);
    return false;
  }
  toView([m](BrowserView* v) { emit v->consoleMessage(m); });
  return true;
}

bool BrowserClient::OnBeforeDownload(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem> item,
                                     const CefString& suggested_name,
                                     CefRefPtr<CefBeforeDownloadCallback> callback) {
  if (item && isExtensionPackage(item)) return false;   // установку расширения ведёт Chromium
  QString dir;
  bool ask = false;
  CefRuntime::instance()->downloads(&dir, &ask);
  const QString name = qs(suggested_name);
  if (ask || dir.isEmpty()) {
    callback->Continue(CefString(name.toStdWString()), true);   // диалог «Сохранить как»
  } else {
    // Не перезаписываем: «файл.zip» → «файл (1).zip».
    const QFileInfo fi(name);
    QString path = QDir(dir).filePath(name);
    for (int i = 1; QFileInfo::exists(path) && i < 1000; ++i)
      path = QDir(dir).filePath(QStringLiteral("%1 (%2)%3").arg(fi.completeBaseName()).arg(i)
                                    .arg(fi.suffix().isEmpty() ? QString() : "." + fi.suffix()));
    QDir().mkpath(dir);
    callback->Continue(CefString(QDir::toNativeSeparators(path).toStdWString()), false);
  }
  return true;
}

bool BrowserClient::OnChromeCommand(CefRefPtr<CefBrowser>, int id, cef_window_open_disposition_t) {
  if (id == IDC_FOCUS_LOCATION) {
    toView([](BrowserView* v) { emit v->addressFocusRequested(); });
    return true;
  }
  // Команды вкладок и окон выполняет интерфейс Crumb, а не Chromium.
  const bool ours = id == IDC_NEW_TAB || id == IDC_NEW_WINDOW || id == IDC_NEW_INCOGNITO_WINDOW ||
                    id == IDC_CLOSE_TAB || id == IDC_CLOSE_WINDOW || id == IDC_EXIT ||
                    id == IDC_SELECT_NEXT_TAB || id == IDC_SELECT_PREVIOUS_TAB ||
                    (id >= IDC_SELECT_TAB_0 && id <= IDC_SELECT_LAST_TAB) ||
                    id == IDC_DUPLICATE_TAB || id == IDC_RESTORE_TAB || id == IDC_NEW_TAB_TO_RIGHT;
  if (!ours) return false;
  toView([id](BrowserView* v) { emit v->chromeCommand(id); });
  return true;
}

}  // namespace crumb
