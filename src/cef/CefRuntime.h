#pragma once
// Жизненный цикл CEF внутри Qt-приложения.
//
// На Windows CEF работает в режиме multi_threaded_message_loop: у Chromium свой
// UI-поток, а главный поток остаётся за Qt. Всё, что приходит из CEF, мы
// пересылаем в Qt через QMetaObject::invokeMethod(..., Qt::QueuedConnection).
// На Linux такого режима нет — там используется external_message_pump и
// CefDoMessageLoopWork() по таймеру Qt.

#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include <QHash>
#include <QVariant>
#include <atomic>
#include <mutex>
#include <vector>

#include "include/cef_app.h"
#include "include/cef_browser.h"

class QQmlEngine;
class QJSEngine;
class QTimer;
#include <QWindow>

namespace crumb {

class CefRuntime final : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(CefRuntime)
  QML_SINGLETON
  Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
  Q_PROPERTY(bool canQuit READ canQuit NOTIFY browserCountChanged)

 public:
  static CefRuntime* instance();
  static CefRuntime* create(QQmlEngine*, QJSEngine*);

  // Вызывается в main() после создания QGuiApplication. false — уже запущен
  // другой экземпляр (CEF передал ему нашу командную строку), надо выйти.
  bool initialize(const CefMainArgs& args, CefRefPtr<CefApp> app);
  void shutdown();

  bool ready() const { return ready_; }
  bool canQuit() const { return browsers_.load() == 0; }

  // Закрыть все браузеры и выйти, когда закроется последний.
  Q_INVOKABLE void requestQuit();
  // Адреса, с которыми запустили Crumb (например, Windows открывает ссылку).
  Q_INVOKABLE QStringList startupUrls() const;
  // Переменная окружения (для диагностических режимов).
  Q_INVOKABLE QString env(const QString& name) const { return qEnvironmentVariable(name.toUtf8().constData()); }

  // Журнал сообщений консоли страниц (warn/error) в «User Data/console.log».
  void setConsoleLog(bool on);
  void logConsole(int level, const QString& message, const QString& source, int line);
  Q_INVOKABLE QString consoleLogPath() const;
  // Окно Crumb (или его дочернее окно страницы) сейчас на переднем плане Windows.
  Q_INVOKABLE bool isForeground(QWindow* window) const;
  // Попросить главное окно открыть адрес вкладкой (из любой страницы QML).
  Q_INVOKABLE void openUrlRequest(const QString& url) { emit strayUrlRequested(url); }

  // Светлая/тёмная схема и основной цвет для страниц (prefers-color-scheme)
  // и встроенных элементов Chromium — чтобы сайты следовали теме Crumb.
  void setPageColorScheme(bool dark, quint32 argb);

  // Настройка Chromium (prefs), например "webkit.webprefs.default_font_size".
  void setPreference(const QString& name, const QVariant& value);
  // Удалить cookie и кэш всех сайтов.
  Q_INVOKABLE void clearBrowsingData();

  // Горячие клавиши, которые ловятся и в странице: «Ctrl+Shift+T» → действие.
  void setShortcuts(const QHash<QString, QString>& seqToAction);
  QString actionForShortcut(const QString& seq) const;

  // Загрузки (читаются из потока CEF).
  void setDownloads(const QString& dir, bool ask);
  void downloads(QString* dir, bool* ask) const;

  // Живые браузеры — для команд, которым нужен любой из них (очистка кэша).
  void trackBrowser(CefRefPtr<CefBrowser> b, bool alive);

  // Потокобезопасные уведомления из CEF.
  void notifyContextReady();
  void notifyRelaunch(const QStringList& args);
  void notifyStrayUrl(const QString& url);
  void browserCreated();
  void browserClosed();

 signals:
  void readyChanged();
  void browserCountChanged();
  void relaunchRequested(const QStringList& args);
  // Chromium сам открыл окно (например, расширение) — открыть адрес вкладкой Crumb.
  void strayUrlRequested(const QString& url);
  void closeAllBrowsers();  // BrowserView слушают и закрывают свои браузеры

 private:
  CefRuntime() = default;

  bool ready_ = false;
  bool quitting_ = false;
  std::atomic<int> browsers_{0};
  QTimer* pump_ = nullptr;

  mutable std::mutex mutex_;
  QHash<QString, QString> shortcuts_;
  QString downloadDir_;
  bool consoleLog_ = false;
  bool downloadAsk_ = false;
  std::vector<CefRefPtr<CefBrowser>> live_;
};

}  // namespace crumb
