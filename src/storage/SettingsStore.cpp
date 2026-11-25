#include "storage/SettingsStore.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJSValue>
#include <QJsonObject>
#include <QKeySequence>
#include <QMetaProperty>
#include <QQmlEngine>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>

#include "Prefs.h"
#include "Theme.h"

namespace crumb {

namespace {

QString encode(const QVariant& v) {
  QVariant val = v;
  if (val.metaType().id() == QMetaType::QColor) val = val.value<QColor>().name(QColor::HexArgb);
  QJsonArray arr;
  arr.append(QJsonValue::fromVariant(val));
  return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVariant decode(const QString& s) {
  const QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8());
  if (!doc.isArray() || doc.array().isEmpty()) return {};
  return doc.array().at(0).toVariant();
}

QVariantMap site(const QString& title, const QString& url) {
  return {{"title", title}, {"url", url}};
}

}  // namespace

SettingsStore* SettingsStore::instance() {
  static SettingsStore* self = new SettingsStore();
  return self;
}

SettingsStore* SettingsStore::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

SettingsStore::SettingsStore() : QQmlPropertyMap(this, nullptr) {
  buildDefaults();
  for (auto it = defaults_.cbegin(); it != defaults_.cend(); ++it) insert(it.key(), it.value());
}

void SettingsStore::buildDefaults() {
  const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  defaults_ = {
      // Поисковик
      {"searchEngine", "google"},
      {"searchCustom", "https://www.startpage.com/do/search?q=%s"},
      // Домашняя страница
      {"homeShowClock", true},
      {"homeShowGreeting", true},
      {"homeShowSearch", true},
      {"homeShowSites", true},
      {"homeShowWeather", true},
      {"homeShowNotes", true},
      {"homeBackground", "gradient"},   // gradient | wallpaper | image | solid
      {"homeGradient", "theme"},        // id из Gradients.qml
      {"homeImage", ""},
      {"homeBlur", 0},
      {"homeDim", 0.25},
      {"homeColumns", 6},
      {"homeTileSize", 64},
      {"homeCity", ""},
      {"homeCityResolved", ""},   // для какого текста найдены координаты ниже
      {"homeLat", 1000.0},        // 1000 — ещё не найдено
      {"homeLon", 1000.0},
      {"homeNotes", ""},
      {"homeSites", QVariantList{site("YouTube", "https://www.youtube.com"),
                                 site("Википедия", "https://ru.wikipedia.org"),
                                 site("GitHub", "https://github.com"),
                                 site("Gmail", "https://mail.google.com"),
                                 site("Яндекс", "https://ya.ru"),
                                 site("Telegram", "https://web.telegram.org"),
                                 site("Reddit", "https://www.reddit.com"),
                                 site("Карты", "https://www.google.com/maps")}},
      // Вкладки
      {"tabsPosition", "top"},   // top | side
      {"tabsHeight", 40},
      {"tabsRadius", 12},
      {"tabsIcons", true},
      {"tabsGroups", true},
      // Панель
      {"toolbarButtons", QVariantList{"back", "forward", "reload", "home", "extensions", "rewind", "downloads", "theme", "settings"}},
      {"toolbarRewindAdded", false},
      // Расширения: закреплённые на панели (id)
      {"extPinned", QVariantList{}},
      {"toolbarCompact", false},
      // Сайты
      {"pagesFontSize", 16},
      // Rewind (заработает на этапе 7)
      {"rewindEnabled", true},
      {"rewindLimitMB", 2048},
      {"rewindDays", 90},
      {"rewindBlacklist", "*bank*\nmail.google.com\nmail.yandex.ru\noutlook.live.com\n*.gosuslugi.ru"},
      // Приватность
      {"privacyDnt", false},
      {"privacyBlock3p", false},
      // Загрузки
      {"downloadsDir", QDir::toNativeSeparators(downloads)},
      {"downloadsAsk", false},
      // Запуск: restore — открыть вкладки прошлого сеанса, home — новая вкладка
      {"startupMode", "ask"},   // ask | restore | home
      {"sessionState", ""},      // вкладки и группы (JSON), пишет главное окно
      // История и пароли
      {"historyEnabled", true},
      {"passwordsOffer", true},
      {"passwordsAutofill", true},
      {"passwordsNever", QVariantList{}},
      // Предлагать сделать Crumb браузером по умолчанию
      {"askDefaultBrowser", true},
      // Диагностика: журнал ошибок страниц
      {"debugConsoleLog", false},
      // Аккаунт: свой Client ID приложения GitHub (если не вшит в сборку)
      {"syncClientId", ""},
      // Горячие клавиши (как в Chrome)
      {"shortcutNewTab", "Ctrl+T"},
      {"shortcutCloseTab", "Ctrl+W"},
      {"shortcutReopenTab", "Ctrl+Shift+T"},
      {"shortcutNextTab", "Ctrl+Tab"},
      {"shortcutPrevTab", "Ctrl+Shift+Tab"},
      {"shortcutAddress", "Ctrl+L"},
      {"shortcutReload", "F5"},
      {"shortcutBack", "Alt+Left"},
      {"shortcutForward", "Alt+Right"},
      {"shortcutHome", "Alt+Home"},
      {"shortcutSettings", "Ctrl+,"},
      {"shortcutDownloads", "Ctrl+J"},
      {"shortcutRewind", "Ctrl+Shift+Y"},
      {"shortcutHistory", "Ctrl+H"},
      {"shortcutFind", "Ctrl+F"},
  };
}

// ----------------------------------------------------------------- база

void SettingsStore::load() {
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb";
  QDir().mkpath(dir);
  db_ = QSqlDatabase::addDatabase("QSQLITE", "crumb-settings");
  db_.setDatabaseName(dir + "/crumb.db");
  if (!db_.open()) {
    qWarning("Settings: не удалось открыть базу: %s", qUtf8Printable(db_.lastError().text()));
  } else {
    QSqlQuery(db_).exec("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT NOT NULL)");
  }

  // Тема и внешний вид — свойства объектов Crumb.Ui; их тоже храним здесь.
  bindObject(Theme::instance(), "theme",
             {"themeMode", "darkFrom", "darkTo", "sourceType", "customColor", "amoled", "contrast",
              "transitionMs"});
  bindObject(Prefs::instance(), "",
             {"cookieWaves", "cookieDepth", "cookieScale", "cookieSpin", "cookieBreathe", "handCursor",
              "clickSounds", "clickVolume", "animations", "animSpeed", "springStrength", "fontFamily",
              "textScale"});

  QVariantMap stored;
  QSqlQuery q(db_);
  if (q.exec("SELECT key, value FROM settings"))
    while (q.next()) stored.insert(q.value(0).toString(), decode(q.value(1).toString()));
  applyAll(stored);
}

void SettingsStore::bindObject(QObject* obj, const QString& prefix, const QStringList& props) {
  const QMetaObject* mo = obj->metaObject();
  for (const QString& p : props) {
    const int idx = mo->indexOfProperty(p.toLatin1().constData());
    if (idx < 0) continue;
    // Ключ: themeMode → themeMode, darkFrom → themeDarkFrom, cookieWaves → cookieWaves
    QString key = p;
    if (!prefix.isEmpty() && !p.startsWith(prefix)) key = prefix + p.left(1).toUpper() + p.mid(1);
    bound_.insert(key, {obj, p.toLatin1()});
    defaults_.insert(key, mo->property(idx).read(obj));   // текущее значение = по умолчанию
    const QMetaMethod notify = mo->property(idx).notifySignal();
    if (notify.isValid())
      connect(obj, notify, this, metaObject()->method(metaObject()->indexOfSlot("onBoundChanged()")),
              Qt::UniqueConnection);
  }
}

void SettingsStore::save(const QString& key, const QVariant& v) {
  if (applying_ || !db_.isOpen()) return;
  QSqlQuery q(db_);
  q.prepare("INSERT INTO settings(key, value) VALUES(?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value");
  q.addBindValue(key);
  q.addBindValue(encode(v));
  if (!q.exec()) qWarning("Settings: ошибка записи %s: %s", qUtf8Printable(key), qUtf8Printable(q.lastError().text()));
}

void SettingsStore::onBoundChanged() {
  if (applying_) return;
  QObject* src = sender();
  for (auto it = bound_.cbegin(); it != bound_.cend(); ++it) {
    if (it->obj != src) continue;
    save(it.key(), it->obj->property(it->prop.constData()));
    emit settingChanged(it.key());
  }
}

// --------------------------------------------------------- чтение/запись

QVariant SettingsStore::updateValue(const QString& key, const QVariant& input) {
  QVariant v = input;
  // Массивы/объекты из QML приходят как QJSValue — приводим к обычным типам.
  if (v.metaType() == QMetaType::fromType<QJSValue>()) v = v.value<QJSValue>().toVariant();
  save(key, v);
  QMetaObject::invokeMethod(this, [this, key] { emit settingChanged(key); }, Qt::QueuedConnection);
  return v;
}

void SettingsStore::set(const QString& key, const QVariant& v) {
  if (bound_.contains(key)) {
    const Bound& b = bound_[key];
    b.obj->setProperty(b.prop.constData(), v);
    return;   // сохранит onBoundChanged
  }
  insert(key, v);
  save(key, v);
  emit settingChanged(key);
}

void SettingsStore::applyAll(const QVariantMap& values) {
  for (auto it = values.cbegin(); it != values.cend(); ++it) {
    if (bound_.contains(it.key())) {
      const Bound& b = bound_[it.key()];
      b.obj->setProperty(b.prop.constData(), it.value());
      save(it.key(), b.obj->property(b.prop.constData()));
    } else if (defaults_.contains(it.key())) {
      insert(it.key(), it.value());
      save(it.key(), it.value());
    }
    emit settingChanged(it.key());
  }
}

QVariantMap SettingsStore::snapshot() const {
  QVariantMap m;
  for (auto it = defaults_.cbegin(); it != defaults_.cend(); ++it) {
    if (bound_.contains(it.key())) {
      const Bound& b = bound_[it.key()];
      QVariant v = b.obj->property(b.prop.constData());
      if (v.metaType().id() == QMetaType::QColor) v = v.value<QColor>().name(QColor::HexArgb);
      m.insert(it.key(), v);
    } else {
      m.insert(it.key(), value(it.key()));
    }
  }
  return m;
}

void SettingsStore::reset(const QString& key) {
  if (defaults_.contains(key)) set(key, defaults_.value(key));
}

void SettingsStore::resetAll() {
  if (db_.isOpen()) QSqlQuery(db_).exec("DELETE FROM settings");
  applyAll(defaults_);
}

bool SettingsStore::exportTo(const QUrl& url) const {
  QJsonObject root;
  root.insert("crumbSettings", 1);
  root.insert("values", QJsonObject::fromVariantMap(snapshot()));
  QFile f(url.isLocalFile() ? url.toLocalFile() : url.toString());
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}

bool SettingsStore::importFrom(const QUrl& url) {
  QFile f(url.isLocalFile() ? url.toLocalFile() : url.toString());
  if (!f.open(QIODevice::ReadOnly)) return false;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject() || !doc.object().contains("crumbSettings")) return false;
  applyAll(doc.object().value("values").toObject().toVariantMap());
  return true;
}

QString SettingsStore::keyToString(int key, int modifiers) const {
  switch (key) {
    case Qt::Key_Control: case Qt::Key_Shift: case Qt::Key_Alt: case Qt::Key_Meta:
    case Qt::Key_unknown: case 0:
      return {};
    default:
      break;
  }
  if (key == Qt::Key_Backtab) key = Qt::Key_Tab;
  const auto mods = Qt::KeyboardModifiers(modifiers) & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
  return QKeySequence(QKeyCombination(mods, Qt::Key(key))).toString(QKeySequence::PortableText);
}

QString SettingsStore::searchTemplate() const {
  static const QHash<QString, QString> engines = {
      {"google", "https://www.google.com/search?q=%s"},
      {"yandex", "https://yandex.ru/search/?text=%s"},
      {"duckduckgo", "https://duckduckgo.com/?q=%s"},
      {"bing", "https://www.bing.com/search?q=%s"},
      {"brave", "https://search.brave.com/search?q=%s"},
      {"ecosia", "https://www.ecosia.org/search?q=%s"},
  };
  const QString engine = value("searchEngine").toString();
  if (engine == "custom") {
    const QString t = value("searchCustom").toString();
    if (t.contains("%s")) return t;
  }
  return engines.value(engine, engines.value("google"));
}

QString SettingsStore::searchUrl(const QString& query) const {
  QString t = searchTemplate();
  return t.replace("%s", QString::fromUtf8(QUrl::toPercentEncoding(query)));
}

}  // namespace crumb
