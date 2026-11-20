#pragma once
// Rewind — машина времени для каждой страницы (QML-синглтон).
//
// 1. Снимок: после загрузки страницы скрипт собирает текст, очищенный DOM
//    (без скриптов, со встроенными стилями) и присылает его служебными
//    сообщениями консоли «CRUMB_RW:…» по частям; мини-скриншот — через
//    DevTools-протокол (Page.captureScreenshot). Всё сжимается и пишется в
//    rewind.db. Одинаковые версии не дублируются.
// 2. Приватность: страницы с полем пароля, чёрный список, не-http(s) — никогда.
// 3. Просмотр прошлого и «что изменилось» (построчный diff) — локальные файлы.
// 4. Подписки: выбранный фрагмент страницы проверяется в фоне скрытым
//    браузером; при изменении — уведомление Windows.
// 5. Поиск по прошлому — FTS5 по тексту всех снимков.

#include <QHash>
#include <QJsonValue>
#include <QObject>
#include <QQuickImageProvider>
#include <QRegularExpression>
#include <QVariantList>
#include <QWindow>
#include <QtQml/qqmlregistration.h>
#include <atomic>
#include <mutex>

#include "include/cef_browser.h"
#include "include/cef_devtools_message_observer.h"
#include "rewind/RewindStore.h"

class QQmlEngine;
class QJSEngine;
class QTimer;

namespace crumb {

class RewindManager final : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Rewind)
  QML_SINGLETON
  Q_PROPERTY(int count READ count NOTIFY statsChanged)
  Q_PROPERTY(double sizeMB READ sizeMB NOTIFY statsChanged)
  Q_PROPERTY(QVariantList watches READ watches NOTIFY watchesChanged)
  Q_PROPERTY(int unseenWatches READ unseenWatches NOTIFY watchesChanged)
  Q_PROPERTY(int revision READ revision NOTIFY snapshotsChanged)   // меняется при каждом новом снимке

 public:
  static RewindManager* instance();
  static RewindManager* create(QQmlEngine*, QJSEngine*);

  int count() const { return count_; }
  double sizeMB() const { return sizeMB_; }
  QVariantList watches() const { return watches_; }
  int unseenWatches() const;
  int revision() const { return revision_; }
  RewindStore& store() { return store_; }

  // --- для QML
  Q_INVOKABLE void start(QWindow* window);                // окно-родитель для фоновой проверки подписок
  Q_INVOKABLE QVariantList snapshotsFor(const QString& url);
  Q_INVOKABLE QString pageUrl(int id);                    // file:///… снимка
  Q_INVOKABLE QString diffUrl(int oldId, int newId = 0);  // file:///… страницы «что изменилось»
  Q_INVOKABLE QVariantList search(const QString& query);
  Q_INVOKABLE QString pickerJs() const;                   // выбор фрагмента для подписки
  Q_INVOKABLE void removeWatch(int id);
  Q_INVOKABLE void markWatchesSeen();
  Q_INVOKABLE void checkWatchesNow();
  Q_INVOKABLE void clearAll();
  Q_INVOKABLE QString formatDate(double ms) const;
  // Синхронизация: добавить подписки, которых здесь ещё нет (по адресу + фрагменту).
  void importWatches(const QVariantList& list);

  // --- из потока CEF
  void onPageLoaded(CefRefPtr<CefBrowser> browser);
  bool handleConsole(CefRefPtr<CefBrowser> browser, const QString& message);   // true — сообщение наше
  void onScreenshot(int messageId, bool ok, const QByteArray& json);
  void onWatchBrowserCreated(CefRefPtr<CefBrowser> b);
  void onWatchBrowserClosed();
  void onWatchPageLoaded();

 signals:
  void statsChanged();
  void watchesChanged();
  void snapshotsChanged(const QString& url);
  void watchAdded(const QString& label);
  void watchTriggered(const QString& label, const QString& oldText, const QString& newText, const QString& url);

 private:
  RewindManager();
  void loadSettings();
  bool eligible(const QString& url) const;
  void ingest(const QString& token, const QByteArray& payload);
  void thumbArrived(const QString& token, const QByteArray& jpeg);
  void addWatchFromPick(const QByteArray& json);
  void watchResult(qint64 id, const QJsonValue& text);
  void nextWatch();
  void notify(const QString& title, const QString& body, const QString& url);
  void refreshStats();

  RewindStore store_;
  int count_ = 0, revision_ = 0;
  double sizeMB_ = 0;
  QVariantList watches_;

  // данные для потока CEF
  mutable std::mutex mutex_;
  bool enabled_ = true;
  QList<QRegularExpression> blacklist_;
  std::atomic<int> tokenCounter_{0};
  struct Pending { CefRefPtr<CefBrowser> browser; QStringList parts; int got = 0; };
  QHash<QString, Pending> pending_;
  QHash<int, QString> shotToken_;                       // id метода DevTools → снимок
  QHash<int, CefRefPtr<CefRegistration>> observers_;    // по id браузера
  CefRefPtr<CefDevToolsMessageObserver> observer_;

  // поток Qt
  QHash<QString, qint64> tokenId_;
  QHash<QString, QByteArray> pendingThumb_;
  int sinceCleanup_ = 0;

  // подписки
  QWindow* window_ = nullptr;
  CefRefPtr<CefBrowser> watchBrowser_;
  bool watchCreating_ = false, watchBusy_ = false, watchForce_ = false;
  qint64 currentWatch_ = 0;
  QString currentSelector_, currentWatchUrl_;
  QTimer* watchTimer_ = nullptr;
  QTimer* watchTimeout_ = nullptr;
  bool trayAdded_ = false;
  QString trayUrl_;
};

class RewindImageProvider final : public QQuickImageProvider {
 public:
  RewindImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
  QImage requestImage(const QString& id, QSize* size, const QSize& requested) override;
};

}  // namespace crumb
