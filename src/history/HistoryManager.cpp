#include "history/HistoryManager.h"

#include <QDateTime>
#include <QDir>
#include <QLocale>
#include <QQmlEngine>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>

#include "storage/SettingsStore.h"

namespace crumb {

namespace {
qint64 now() { return QDateTime::currentMSecsSinceEpoch(); }

// «www.youtube.com» → «youtube.com»: так один сайт — один значок и одна группа.
QString siteHost(const QString& url) {
  QString h = QUrl(url).host().toLower();
  if (h.startsWith(QLatin1String("www."))) h = h.mid(4);
  return h;
}

QString dayLabel(const QDateTime& dt) {
  static const QLocale ru(QLocale::Russian);
  const QDate d = dt.date(), today = QDate::currentDate();
  QString s = ru.toString(d, d.year() == today.year() ? QStringLiteral("dddd, d MMMM") : QStringLiteral("dddd, d MMMM yyyy"));
  if (!s.isEmpty()) s[0] = s[0].toUpper();
  if (d == today) return QObject::tr("Сегодня — %1").arg(s);
  if (d == today.addDays(-1)) return QObject::tr("Вчера — %1").arg(s);
  return s;
}
}  // namespace

HistoryManager* HistoryManager::instance() {
  static HistoryManager* self = new HistoryManager();
  return self;
}

HistoryManager* HistoryManager::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

void HistoryManager::open() {
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb";
  QDir().mkpath(dir);
  db_ = QSqlDatabase::addDatabase("QSQLITE", "crumb-history");
  db_.setDatabaseName(dir + "/history.db");
  if (!db_.open()) {
    qWarning("History: не удалось открыть базу: %s", qUtf8Printable(db_.lastError().text()));
    return;
  }
  QSqlQuery q(db_);
  q.exec("PRAGMA journal_mode=WAL");
  q.exec("CREATE TABLE IF NOT EXISTS visits (id INTEGER PRIMARY KEY, url TEXT NOT NULL, title TEXT, host TEXT, ts INTEGER)");
  q.exec("CREATE INDEX IF NOT EXISTS visits_ts ON visits(ts)");
  q.exec("CREATE INDEX IF NOT EXISTS visits_host ON visits(host)");
  q.exec("CREATE TABLE IF NOT EXISTS urls (url TEXT PRIMARY KEY, title TEXT, host TEXT, visits INTEGER, last_ts INTEGER)");
  q.exec("CREATE TABLE IF NOT EXISTS icons (host TEXT PRIMARY KEY, data TEXT)");
  // Храним полгода — как Chrome.
  QSqlQuery old(db_);
  old.prepare("DELETE FROM visits WHERE ts < ?");
  old.addBindValue(now() - 180LL * 24 * 3600 * 1000);
  old.exec();
  QSqlQuery(db_).exec("DELETE FROM urls WHERE url NOT IN (SELECT DISTINCT url FROM visits)");
}

bool HistoryManager::eligible(const QString& url) {
  return url.startsWith(QLatin1String("https://")) || url.startsWith(QLatin1String("http://"));
}

void HistoryManager::bump() {
  ++revision_;
  emit changed();
}

int HistoryManager::count() const {
  QSqlQuery q(db_);
  return q.exec("SELECT COUNT(*) FROM visits") && q.next() ? q.value(0).toInt() : 0;
}

QString HistoryManager::hostOf(const QString& url) const { return siteHost(url); }

void HistoryManager::visit(const QString& url, const QString& title) {
  if (!db_.isOpen() || !eligible(url)) return;
  if (!SettingsStore::instance()->get("historyEnabled").toBool()) return;
  const qint64 t = now();
  // Перезагрузка или редирект на тот же адрес в течение 30 с — то же посещение.
  if (url == lastUrl_ && t - lastTs_ < 30000) return;
  QSqlQuery q(db_);
  q.prepare("INSERT INTO visits(url, title, host, ts) VALUES(?, ?, ?, ?)");
  q.addBindValue(url);
  q.addBindValue(title);
  q.addBindValue(siteHost(url));
  q.addBindValue(t);
  if (!q.exec()) return;
  lastUrl_ = url;
  lastTs_ = t;
  lastId_ = q.lastInsertId().toLongLong();
  QSqlQuery u(db_);
  u.prepare("INSERT INTO urls(url, title, host, visits, last_ts) VALUES(?, ?, ?, 1, ?) "
            "ON CONFLICT(url) DO UPDATE SET visits = visits + 1, last_ts = excluded.last_ts, "
            "title = CASE WHEN excluded.title <> '' THEN excluded.title ELSE urls.title END");
  u.addBindValue(url);
  u.addBindValue(title);
  u.addBindValue(siteHost(url));
  u.addBindValue(t);
  u.exec();
  bump();
}

void HistoryManager::setTitle(const QString& url, const QString& title) {
  if (!db_.isOpen() || !eligible(url) || title.trimmed().isEmpty()) return;
  QSqlQuery q(db_);
  // Заголовок приходит после адреса — дописываем его в последнее посещение этого адреса.
  q.prepare("UPDATE visits SET title = ? WHERE id = (SELECT id FROM visits WHERE url = ? ORDER BY ts DESC LIMIT 1)");
  q.addBindValue(title);
  q.addBindValue(url);
  q.exec();
  QSqlQuery u(db_);
  u.prepare("UPDATE urls SET title = ? WHERE url = ?");
  u.addBindValue(title);
  u.addBindValue(url);
  u.exec();
  if (q.numRowsAffected() > 0) bump();
}

void HistoryManager::setIcon(const QString& url, const QString& dataUrl) {
  if (!db_.isOpen() || !eligible(url) || dataUrl.isEmpty() || dataUrl.size() > 64 * 1024) return;
  QSqlQuery q(db_);
  q.prepare("INSERT INTO icons(host, data) VALUES(?, ?) ON CONFLICT(host) DO UPDATE SET data = excluded.data");
  q.addBindValue(siteHost(url));
  q.addBindValue(dataUrl);
  q.exec();
}

QString HistoryManager::iconFor(const QString& url) const {
  if (!db_.isOpen()) return {};
  QSqlQuery q(db_);
  q.prepare("SELECT data FROM icons WHERE host = ?");
  q.addBindValue(siteHost(url));
  return q.exec() && q.next() ? q.value(0).toString() : QString();
}

QVariantList HistoryManager::query(const QString& text, int limit, double beforeTs) const {
  QVariantList out;
  if (!db_.isOpen()) return out;
  const QString t = text.trimmed();
  QSqlQuery q(db_);
  q.prepare(QStringLiteral(
      "SELECT v.id, v.url, v.title, v.host, v.ts, i.data FROM visits v LEFT JOIN icons i ON i.host = v.host "
      "WHERE (? = '' OR v.url LIKE ? ESCAPE '\\' OR v.title LIKE ? ESCAPE '\\') AND (? <= 0 OR v.ts < ?) "
      "ORDER BY v.ts DESC LIMIT ?"));
  QString like = t;
  like.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
  like = "%" + like + "%";
  q.addBindValue(t);
  q.addBindValue(like);
  q.addBindValue(like);
  q.addBindValue(beforeTs);
  q.addBindValue(beforeTs);
  q.addBindValue(limit);
  if (!q.exec()) return out;
  static const QLocale ru(QLocale::Russian);
  while (q.next()) {
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(q.value(4).toLongLong());
    const QString title = q.value(2).toString();
    out.append(QVariantMap{{"vid", q.value(0).toInt()},
                           {"url", q.value(1).toString()},
                           {"title", title.isEmpty() ? q.value(1).toString() : title},
                           {"host", q.value(3).toString()},
                           {"ts", double(q.value(4).toLongLong())},
                           {"time", ru.toString(dt.time(), QStringLiteral("HH:mm"))},
                           {"day", dayLabel(dt)},
                           {"icon", q.value(5).toString()}});
  }
  return out;
}

QVariantList HistoryManager::suggest(const QString& text, int limit) const {
  QVariantList out;
  const QString t = text.trimmed().toLower();
  if (!db_.isOpen() || t.isEmpty()) return out;
  QString like = t;
  like.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
  QSqlQuery q(db_);
  q.prepare("SELECT u.url, u.title, u.host, u.visits, u.last_ts, i.data FROM urls u LEFT JOIN icons i ON i.host = u.host "
            "WHERE u.url LIKE ? ESCAPE '\\' OR u.title LIKE ? ESCAPE '\\' ORDER BY u.visits DESC LIMIT 300");
  q.addBindValue("%" + like + "%");
  q.addBindValue("%" + like + "%");
  if (!q.exec()) return out;
  struct Hit { double score; QVariantMap m; };
  std::vector<Hit> hits;
  const qint64 n = now();
  while (q.next()) {
    const QString url = q.value(0).toString(), host = q.value(2).toString();
    QString bare = url.mid(url.indexOf("://") + 3).toLower();
    if (bare.startsWith("www.")) bare = bare.mid(4);
    // Совпадение с началом адреса важнее всего, потом — начало слова в заголовке.
    double score = 0;
    if (bare.startsWith(t)) score += 100;
    else if (host.startsWith(t)) score += 80;
    else if (q.value(1).toString().toLower().startsWith(t)) score += 40;
    else score += 10;
    if (bare.count('/') <= 1 || bare.endsWith('/')) score += 15;   // главные страницы сайтов выше
    score += std::min(40.0, q.value(3).toInt() * 2.0);
    const double days = (n - q.value(4).toLongLong()) / 86400000.0;
    score += std::max(0.0, 20.0 - days);
    hits.push_back({score, QVariantMap{{"url", url}, {"title", q.value(1).toString()}, {"host", host},
                                       {"icon", q.value(5).toString()}}});
  }
  std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.score > b.score; });
  for (int i = 0; i < int(hits.size()) && i < limit; ++i) out.append(hits[i].m);
  return out;
}

void HistoryManager::remove(int id) {
  QSqlQuery q(db_);
  q.prepare("DELETE FROM visits WHERE id = ?");
  q.addBindValue(id);
  q.exec();
  QSqlQuery(db_).exec("DELETE FROM urls WHERE url NOT IN (SELECT DISTINCT url FROM visits)");
  bump();
}

void HistoryManager::removeSite(const QString& host) {
  QSqlQuery q(db_);
  q.prepare("DELETE FROM visits WHERE host = ?");
  q.addBindValue(host);
  q.exec();
  QSqlQuery u(db_);
  u.prepare("DELETE FROM urls WHERE host = ?");
  u.addBindValue(host);
  u.exec();
  bump();
}

void HistoryManager::clear(int minutes) {
  QSqlQuery q(db_);
  if (minutes <= 0) {
    q.exec("DELETE FROM visits");
    QSqlQuery(db_).exec("DELETE FROM urls");
    QSqlQuery(db_).exec("DELETE FROM icons");
  } else {
    q.prepare("DELETE FROM visits WHERE ts >= ?");
    q.addBindValue(now() - qint64(minutes) * 60000);
    q.exec();
    QSqlQuery(db_).exec("DELETE FROM urls WHERE url NOT IN (SELECT DISTINCT url FROM visits)");
  }
  lastUrl_.clear();
  QSqlQuery(db_).exec("VACUUM");
  bump();
}

}  // namespace crumb
