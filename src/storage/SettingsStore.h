#pragma once
// Settings — все настройки Crumb в одном месте (QML-синглтон).
//
// Хранятся в SQLite (%LOCALAPPDATA%\Crumb\crumb.db, таблица settings).
// В QML каждая настройка — обычное свойство: Settings.searchEngine,
// Settings.homeShowClock и т.д.; запись в него сразу сохраняется в базу.
// Параметры темы (Theme) и внешнего вида (Prefs) тоже сохраняются здесь.

#include <QQmlPropertyMap>
#include <QSqlDatabase>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace crumb {

class SettingsStore : public QQmlPropertyMap {
  Q_OBJECT
  QML_NAMED_ELEMENT(Settings)
  QML_SINGLETON

 public:
  static SettingsStore* instance();
  static SettingsStore* create(QQmlEngine*, QJSEngine*);

  // Открыть базу и применить сохранённые значения. Вызывается в main().
  void load();

  QVariant get(const QString& key) const { return value(key); }
  void set(const QString& key, const QVariant& v);

  // Адрес поиска для запроса (с учётом выбранного поисковика).
  QString searchUrl(const QString& query) const;

  Q_INVOKABLE QVariant defaultValue(const QString& key) const { return defaults_.value(key); }
  Q_INVOKABLE void reset(const QString& key);
  Q_INVOKABLE void resetAll();
  Q_INVOKABLE bool exportTo(const QUrl& file) const;
  Q_INVOKABLE bool importFrom(const QUrl& file);
  // Сочетание клавиш из события QML (key, modifiers) в текст вида «Ctrl+Shift+T».
  Q_INVOKABLE QString keyToString(int key, int modifiers) const;
  Q_INVOKABLE QString searchTemplate() const;
  Q_INVOKABLE QString localFile(const QUrl& url) const { return url.toLocalFile(); }

  // Все текущие значения (для синхронизации) и применение пачки значений.
  QVariantMap snapshot() const;
  void applyAll(const QVariantMap& values);

 signals:
  void settingChanged(const QString& key);

 private slots:
  void onBoundChanged();

 protected:
  QVariant updateValue(const QString& key, const QVariant& input) override;

 private:
  SettingsStore();
  void buildDefaults();
  void bindObject(QObject* obj, const QString& prefix, const QStringList& props);
  void save(const QString& key, const QVariant& v);

  QVariantMap defaults_;
  struct Bound { QObject* obj; QByteArray prop; };
  QHash<QString, Bound> bound_;   // ключ → свойство Theme/Prefs
  QSqlDatabase db_;
  bool applying_ = false;
};

}  // namespace crumb
