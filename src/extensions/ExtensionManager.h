#pragma once
// Extensions — расширения Chrome в Crumb (QML-синглтон).
//
// В CEF 156 нет C++-API для расширений Chrome style (CefExtension удалён вместе
// с Alloy), поэтому управление идёт через страницу chrome://extensions:
// скрытый браузер открывает её и вызывает chrome.developerPrivate /
// chrome.management (список, включение, удаление, распакованные расширения).
// Ответы приходят служебными сообщениями консоли «CRUMB_EXT:{json}».
// Значки и всплывающие окна (action.default_popup) читаются из manifest.json.

#include <QObject>
#include <QVariantList>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

#include "include/cef_browser.h"

class QQmlEngine;
class QJSEngine;


namespace crumb {

class ExtensionManager final : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Extensions)
  QML_SINGLETON
  Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

 public:
  static ExtensionManager* instance();
  static ExtensionManager* create(QQmlEngine*, QJSEngine*);

  QVariantList items() const { return items_; }
  bool ready() const { return ready_; }
  QString lastError() const { return lastError_; }

  // Запустить скрытый мост (нужно окно-родитель для браузера CEF).
  Q_INVOKABLE void start(QWindow* window);
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void setEnabled(const QString& id, bool enabled);
  Q_INVOKABLE void remove(const QString& id);
  // Выбор папки — системный диалог Chromium; включает режим разработчика.
  Q_INVOKABLE void loadUnpacked();
  Q_INVOKABLE QVariantMap find(const QString& id) const;

  // Из потока CEF.
  void onBridgeCreated(CefRefPtr<CefBrowser> b);
  void onBridgeClosed();
  void onBridgeLoaded();
  void onBridgeMessage(const QString& json);

 signals:
  void itemsChanged();
  void readyChanged();
  void errorOccurred(const QString& message);

 private:
  ExtensionManager();
  void run(const QString& js);
  QVariantMap enrich(const QVariantMap& info) const;

  CefRefPtr<CefBrowser> bridge_;
  bool starting_ = false, ready_ = false;
  QVariantList items_;
  QString lastError_;
};

}  // namespace crumb
