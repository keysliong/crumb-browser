#pragma once
// BrowserView — QML-элемент, внутри которого живёт окно браузера CEF.
//
// Chrome style в CEF работает только с настоящим (оконным) рендерингом, поэтому
// браузер — это дочернее HWND поверх окна Qt. Элемент следит за своим положением
// в сцене и двигает это окно. Ограничение «airspace»: QML не может рисовать
// поверх страницы, поэтому всплывающие панели Crumb располагаются рядом с ней
// или в отдельных окнах.

#include <QQuickItem>
#include <QRect>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <memory>

#include "include/cef_browser.h"

namespace crumb {

struct ViewLink;

class BrowserView : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString initialUrl READ initialUrl WRITE setInitialUrl NOTIFY initialUrlChanged)
  Q_PROPERTY(QString url READ url NOTIFY urlChanged)
  // Адрес для показа человеку: кириллица и пробелы без %-кодирования.
  Q_PROPERTY(QString displayUrl READ displayUrl NOTIFY urlChanged)
  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(bool secure READ secure NOTIFY urlChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingStateChanged)
  Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY loadingStateChanged)
  Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY loadingStateChanged)
  Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
  Q_PROPERTY(QString favicon READ favicon NOTIFY faviconChanged)

 public:
  // Команды Chrome (include/cef_command_ids.h), которые выполняет интерфейс Crumb.
  enum Command {
    CmdNewWindow = 34000, CmdNewIncognitoWindow = 34001, CmdCloseWindow = 34012,
    CmdNewTab = 34014, CmdCloseTab = 34015, CmdSelectNextTab = 34016,
    CmdSelectPreviousTab = 34017, CmdSelectTab0 = 34018, CmdSelectTab7 = 34025,
    CmdSelectLastTab = 34026, CmdDuplicateTab = 34027, CmdRestoreTab = 34028,
    CmdExit = 34031, CmdNewTabToRight = 35024
  };
  Q_ENUM(Command)

  explicit BrowserView(QQuickItem* parent = nullptr);
  ~BrowserView() override;

  QString initialUrl() const { return initialUrl_; }
  void setInitialUrl(const QString& url);
  QString url() const { return url_; }
  QString displayUrl() const;
  QString title() const { return title_; }
  bool secure() const { return url_.startsWith(QLatin1String("https://")); }
  bool loading() const { return loading_; }
  bool canGoBack() const { return canGoBack_; }
  bool canGoForward() const { return canGoForward_; }
  double progress() const { return progress_; }
  QString favicon() const { return favicon_; }

  // Текст из адресной строки: адрес или поисковый запрос.
  Q_INVOKABLE void navigate(const QString& input);
  Q_INVOKABLE void goBack();
  Q_INVOKABLE void goForward();
  Q_INVOKABLE void reload();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void focusPage();
  // Забрать клавиатуру у страницы (перед вводом в адресную строку).
  Q_INVOKABLE void releasePageFocus();
  Q_INVOKABLE void closeBrowser();
  Q_INVOKABLE void executeJavaScript(const QString& code);

  // Вызываются из BrowserClient (уже в потоке Qt).
  void attachBrowser(CefRefPtr<CefBrowser> browser, quintptr hwnd);
  void detachBrowser();
  void setUrlFromCef(const QString& url);
  void setTitleFromCef(const QString& title);
  void setProgressFromCef(double progress);
  void setLoadingStateFromCef(bool loading, bool canGoBack, bool canGoForward);
  void setFaviconFromCef(const QString& dataUrl);
  void handlePopup(const QString& url, bool background);

 signals:
  void initialUrlChanged();
  void urlChanged();
  void titleChanged();
  void loadingStateChanged();
  void progressChanged();
  void addressFocusRequested();
  void chromeCommand(int commandId);
  void popupRequested(const QString& url, bool background);
  void faviconChanged();
  // Сработало сочетание клавиш из настроек, пока фокус был в странице.
  void shortcutTriggered(const QString& action);
  // console.log("CRUMB_…") со страницы — служебный канал (размер всплывающего окна расширения и т.п.).
  void consoleMessage(const QString& message);
  // Страница закрылась сама (window.close()), а не по нашей команде.
  void pageClosed();

 protected:
  void componentComplete() override;
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
  void itemChange(ItemChange change, const ItemChangeData& value) override;

 private:
  void tryCreate();
  void syncGeometry();
  void syncVisibility();
  QRect physicalRect() const;

  std::shared_ptr<ViewLink> link_;
  CefRefPtr<CefBrowser> browser_;
  quintptr hwnd_ = 0;
  bool creating_ = false;
  bool closingByUs_ = false;
  QString favicon_;
  QRect lastRect_;
  QMetaObject::Connection frameConn_, screenConn_;

  QString initialUrl_ = QStringLiteral("https://www.google.com/");
  QString url_, title_;
  bool loading_ = false, canGoBack_ = false, canGoForward_ = false;
  double progress_ = 0.0;
};

}  // namespace crumb
