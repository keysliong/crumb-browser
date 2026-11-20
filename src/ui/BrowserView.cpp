#include "ui/BrowserView.h"

#include <QMetaMethod>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QUrl>

#include "cef/BrowserClient.h"
#include "cef/CefRuntime.h"
#include "ui/UrlUtil.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace crumb {

namespace {
CefString toCef(const QString& s) { return CefString(s.toStdWString()); }
}  // namespace

BrowserView::BrowserView(QQuickItem* parent)
    : QQuickItem(parent), link_(std::make_shared<ViewLink>()) {
  link_->view = this;
  auto* rt = CefRuntime::instance();
  connect(rt, &CefRuntime::readyChanged, this, &BrowserView::tryCreate);
  connect(rt, &CefRuntime::closeAllBrowsers, this, &BrowserView::closeBrowser);
}

BrowserView::~BrowserView() {
  {
    std::lock_guard lock(link_->mutex);
    link_->view = nullptr;
  }
  closeBrowser();
}

void BrowserView::setInitialUrl(const QString& url) {
  if (url == initialUrl_) return;
  initialUrl_ = url;
  emit initialUrlChanged();
}

void BrowserView::componentComplete() {
  QQuickItem::componentComplete();
  // Ждём один оборот цикла событий: к этому времени окно и раскладка готовы.
  QTimer::singleShot(0, this, &BrowserView::tryCreate);
}

void BrowserView::tryCreate() {
  if (browser_ || creating_ || !isComponentComplete()) return;
  if (!CefRuntime::instance()->ready() || !window()) return;
  creating_ = true;

  const QRect r = physicalRect();
  CefWindowInfo info;
#if defined(_WIN32)
  info.SetAsChild(reinterpret_cast<CefWindowHandle>(window()->winId()),
                  CefRect(r.x(), r.y(), std::max(1, r.width()), std::max(1, r.height())));
#else
  info.SetAsChild(static_cast<CefWindowHandle>(window()->winId()),
                  CefRect(r.x(), r.y(), std::max(1, r.width()), std::max(1, r.height())));
#endif
  // Chrome style: полноценный Chromium — расширения, Web Store, менеджер паролей.
  info.runtime_style = CEF_RUNTIME_STYLE_CHROME;

  CefBrowserSettings settings;
  CefRefPtr<BrowserClient> client = new BrowserClient(link_);
  CefBrowserHost::CreateBrowser(info, client, toCef(UrlUtil::fixup(initialUrl_)), settings,
                                nullptr, nullptr);
}

void BrowserView::attachBrowser(CefRefPtr<CefBrowser> browser, quintptr hwnd) {
  browser_ = browser;
  hwnd_ = hwnd;
  creating_ = false;
  lastRect_ = QRect();
  syncGeometry();
  syncVisibility();
}

void BrowserView::detachBrowser() {
  browser_ = nullptr;
  hwnd_ = 0;
  if (!closingByUs_) emit pageClosed();
}

void BrowserView::closeBrowser() {
  closingByUs_ = true;
  if (browser_) browser_->GetHost()->CloseBrowser(true);
}

void BrowserView::executeJavaScript(const QString& code) {
  if (browser_) browser_->GetMainFrame()->ExecuteJavaScript(toCef(code), "crumb://internal", 0);
}

void BrowserView::setFaviconFromCef(const QString& dataUrl) {
  if (dataUrl == favicon_) return;
  favicon_ = dataUrl;
  emit faviconChanged();
}

// ---------------------------------------------------------------- навигация

void BrowserView::navigate(const QString& input) {
  const QString target = UrlUtil::fixup(input);
  if (target.isEmpty()) return;
  if (browser_) {
    browser_->GetMainFrame()->LoadURL(toCef(target));
    focusPage();
  } else {
    setInitialUrl(target);
  }
}

void BrowserView::goBack() {
  if (browser_) browser_->GoBack();
}
void BrowserView::goForward() {
  if (browser_) browser_->GoForward();
}
void BrowserView::reload() {
  if (browser_) browser_->Reload();
}
void BrowserView::stop() {
  if (browser_) browser_->StopLoad();
}

void BrowserView::focusPage() {
  if (browser_) browser_->GetHost()->SetFocus(true);
}

void BrowserView::releasePageFocus() {
  if (browser_) browser_->GetHost()->SetFocus(false);
#if defined(_WIN32)
  if (window()) ::SetFocus(reinterpret_cast<HWND>(window()->winId()));
#endif
}

void BrowserView::handlePopup(const QString& url, bool background) {
  if (isSignalConnected(QMetaMethod::fromSignal(&BrowserView::popupRequested)))
    emit popupRequested(url, background);
  else
    navigate(url);
}

// --------------------------------------------------------- состояние из CEF

QString BrowserView::displayUrl() const {
  return QUrl::fromEncoded(url_.toUtf8()).toDisplayString();
}

void BrowserView::setUrlFromCef(const QString& url) {
  if (url == url_) return;
  url_ = url;
  emit urlChanged();
}

void BrowserView::setTitleFromCef(const QString& title) {
  if (title == title_) return;
  title_ = title;
  emit titleChanged();
}

void BrowserView::setProgressFromCef(double progress) {
  progress_ = progress;
  emit progressChanged();
}

void BrowserView::setLoadingStateFromCef(bool loading, bool canGoBack, bool canGoForward) {
  loading_ = loading;
  canGoBack_ = canGoBack;
  canGoForward_ = canGoForward;
  emit loadingStateChanged();
}

// ------------------------------------------------- положение дочернего окна

QRect BrowserView::physicalRect() const {
  if (!window()) return {};
  const qreal dpr = window()->devicePixelRatio();
  const QPointF tl = mapToScene(QPointF(0, 0));
  return QRect(qRound(tl.x() * dpr), qRound(tl.y() * dpr), qRound(width() * dpr),
               qRound(height() * dpr));
}

void BrowserView::syncGeometry() {
  if (!hwnd_) return;
  const QRect r = physicalRect();
  if (r == lastRect_) return;
  lastRect_ = r;
#if defined(_WIN32)
  // Окно браузера принадлежит UI-потоку CEF: асинхронный SetWindowPos не ждёт его.
  ::SetWindowPos(reinterpret_cast<HWND>(hwnd_), nullptr, r.x(), r.y(), r.width(), r.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
#else
  // Linux (X11): перемещение окна добавим при портировании.
  if (browser_) browser_->GetHost()->WasResized();
#endif
}

void BrowserView::syncVisibility() {
  if (!hwnd_) return;
#if defined(_WIN32)
  ::ShowWindowAsync(reinterpret_cast<HWND>(hwnd_), isVisible() ? SW_SHOWNA : SW_HIDE);
#endif
  if (browser_) browser_->GetHost()->WasHidden(!isVisible());
}

void BrowserView::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  syncGeometry();
}

void BrowserView::itemChange(ItemChange change, const ItemChangeData& value) {
  QQuickItem::itemChange(change, value);
  if (change == ItemVisibleHasChanged) {
    syncVisibility();
  } else if (change == ItemSceneChange) {
    disconnect(frameConn_);
    disconnect(screenConn_);
    if (QQuickWindow* w = value.window) {
      // Родители могут сдвинуть элемент без geometryChange — сверяемся каждый кадр.
      frameConn_ = connect(w, &QQuickWindow::afterAnimating, this, &BrowserView::syncGeometry);
      screenConn_ = connect(w, &QWindow::screenChanged, this, [this] {
        lastRect_ = QRect();
        syncGeometry();
      });
      QTimer::singleShot(0, this, &BrowserView::tryCreate);
    }
  }
}

}  // namespace crumb
