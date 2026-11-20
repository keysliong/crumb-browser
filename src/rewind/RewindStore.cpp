#include "rewind/RewindStore.h"

#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>

namespace crumb {

namespace {
qint64 now() { return QDateTime::currentMSecsSinceEpoch(); }
QString hashOf(const QString& text) {
  // Сравниваем без пробельного шума: одинаковый текст — одинаковый снимок.
  const QString norm = text.simplified();
  return QString::fromLatin1(QCryptographicHash::hash(norm.toUtf8(), QCryptographicHash::Sha1).toHex());
}
}  // namespace

bool RewindStore::open(const QString& path) {
  db_ = QSqlDatabase::addDatabase("QSQLITE", "crumb-rewind");
  db_.setDatabaseName(path);
  if (!db_.open()) return false;
  QSqlQuery q(db_);
  q.exec("PRAGMA journal_mode=WAL");
  q.exec("CREATE TABLE IF NOT EXISTS snapshots (id INTEGER PRIMARY KEY, url TEXT, host TEXT, title TEXT, "
         "ts INTEGER, ts_last INTEGER, hash TEXT, text_z BLOB, html_z BLOB, thumb BLOB, bytes INTEGER)");
  q.exec("CREATE INDEX IF NOT EXISTS snapshots_url ON snapshots(url, ts)");
  q.exec("CREATE TABLE IF NOT EXISTS watches (id INTEGER PRIMARY KEY, url TEXT, selector TEXT, label TEXT, "
         "last_text TEXT, prev_text TEXT, created INTEGER, last_check INTEGER, changed_at INTEGER, unseen INTEGER DEFAULT 0)");
  fts_ = q.exec("CREATE VIRTUAL TABLE IF NOT EXISTS rewind_fts USING fts5(title, url, body, "
                "tokenize='unicode61 remove_diacritics 2')");
  if (!fts_) qWarning("Rewind: FTS5 недоступен (%s) — поиск будет медленнее", qUtf8Printable(q.lastError().text()));
  return true;
}

qint64 RewindStore::add(const QString& url, const QString& title, const QString& text, const QString& html,
                        bool* dup) {
  const QString h = hashOf(text);
  QSqlQuery last(db_);
  last.prepare("SELECT id, hash FROM snapshots WHERE url = ? ORDER BY ts DESC LIMIT 1");
  last.addBindValue(url);
  if (last.exec() && last.next() && last.value(1).toString() == h) {
    const qint64 id = last.value(0).toLongLong();
    QSqlQuery u(db_);
    u.prepare("UPDATE snapshots SET ts_last = ?, title = ? WHERE id = ?");
    u.addBindValue(now());
    u.addBindValue(title);
    u.addBindValue(id);
    u.exec();
    if (dup) *dup = true;
    return id;
  }
  if (dup) *dup = false;
  const QByteArray tz = qCompress(text.toUtf8(), 9), hz = qCompress(html.toUtf8(), 9);
  QSqlQuery q(db_);
  q.prepare("INSERT INTO snapshots(url, host, title, ts, ts_last, hash, text_z, html_z, bytes) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)");
  const qint64 t = now();
  q.addBindValue(url);
  q.addBindValue(QUrl(url).host());
  q.addBindValue(title);
  q.addBindValue(t);
  q.addBindValue(t);
  q.addBindValue(h);
  q.addBindValue(tz);
  q.addBindValue(hz);
  q.addBindValue(qint64(tz.size() + hz.size() + text.size()));
  if (!q.exec()) {
    qWarning("Rewind: не удалось сохранить снимок: %s", qUtf8Printable(q.lastError().text()));
    return 0;
  }
  const qint64 id = q.lastInsertId().toLongLong();
  if (fts_) {
    QSqlQuery f(db_);
    f.prepare("INSERT INTO rewind_fts(rowid, title, url, body) VALUES(?, ?, ?, ?)");
    f.addBindValue(id);
    f.addBindValue(title);
    f.addBindValue(url);
    f.addBindValue(text);
    f.exec();
  }
  return id;
}

void RewindStore::setThumb(qint64 id, const QByteArray& jpeg) {
  QSqlQuery q(db_);
  q.prepare("UPDATE snapshots SET thumb = ?, bytes = bytes + ? WHERE id = ?");
  q.addBindValue(jpeg);
  q.addBindValue(qint64(jpeg.size()));
  q.addBindValue(id);
  q.exec();
}

QByteArray RewindStore::thumb(qint64 id) {
  QSqlQuery q(db_);
  q.prepare("SELECT thumb FROM snapshots WHERE id = ?");
  q.addBindValue(id);
  return q.exec() && q.next() ? q.value(0).toByteArray() : QByteArray();
}

bool RewindStore::load(qint64 id, Snapshot* s) {
  QSqlQuery q(db_);
  q.prepare("SELECT url, host, title, ts, ts_last, text_z, html_z, bytes FROM snapshots WHERE id = ?");
  q.addBindValue(id);
  if (!q.exec() || !q.next()) return false;
  s->id = id;
  s->url = q.value(0).toString();
  s->host = q.value(1).toString();
  s->title = q.value(2).toString();
  s->ts = q.value(3).toLongLong();
  s->tsLast = q.value(4).toLongLong();
  s->text = QString::fromUtf8(qUncompress(q.value(5).toByteArray()));
  s->html = QString::fromUtf8(qUncompress(q.value(6).toByteArray()));
  s->bytes = q.value(7).toLongLong();
  return true;
}

QVariantList RewindStore::listForUrl(const QString& url, int limit) {
  QVariantList out;
  QSqlQuery q(db_);
  q.prepare("SELECT id, title, ts, ts_last, bytes, thumb IS NOT NULL FROM snapshots WHERE url = ? ORDER BY ts ASC LIMIT ?");
  q.addBindValue(url);
  q.addBindValue(limit);
  if (q.exec())
    while (q.next())
      out << QVariantMap{{"id", q.value(0)}, {"title", q.value(1)}, {"ts", q.value(2)}, {"tsLast", q.value(3)},
                         {"bytes", q.value(4)}, {"hasThumb", q.value(5).toBool()}};
  return out;
}

QVariantList RewindStore::search(const QString& query, int limit) {
  QVariantList out;
  const QString qtext = query.trimmed();
  if (qtext.isEmpty()) return out;
  QSqlQuery q(db_);
  if (fts_) {
    // «слово1 слово2» → "слово1"* "слово2"* (префиксы, без синтаксиса FTS в запросе)
    QStringList terms;
    for (const QString& w : qtext.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts))
      terms << "\"" + QString(w).replace('"', "\"\"") + "\"*";
    q.prepare("SELECT s.id, s.url, s.title, s.ts, snippet(rewind_fts, 2, '<b>', '</b>', '…', 14) "
              "FROM rewind_fts JOIN snapshots s ON s.id = rewind_fts.rowid "
              "WHERE rewind_fts MATCH ? ORDER BY s.ts DESC LIMIT ?");
    q.addBindValue(terms.join(' '));
  } else {
    q.prepare("SELECT id, url, title, ts, '' FROM snapshots WHERE title LIKE ? ORDER BY ts DESC LIMIT ?");
    q.addBindValue("%" + qtext + "%");
  }
  q.addBindValue(limit);
  if (q.exec())
    while (q.next())
      out << QVariantMap{{"id", q.value(0)}, {"url", q.value(1)}, {"title", q.value(2)}, {"ts", q.value(3)},
                         {"snippet", q.value(4)}};
  return out;
}

qint64 RewindStore::latestFor(const QString& url) {
  QSqlQuery q(db_);
  q.prepare("SELECT id FROM snapshots WHERE url = ? ORDER BY ts DESC LIMIT 1");
  q.addBindValue(url);
  return q.exec() && q.next() ? q.value(0).toLongLong() : 0;
}

qint64 RewindStore::previousOf(qint64 id) {
  QSqlQuery q(db_);
  q.prepare("SELECT p.id FROM snapshots s JOIN snapshots p ON p.url = s.url AND p.ts < s.ts "
            "WHERE s.id = ? ORDER BY p.ts DESC LIMIT 1");
  q.addBindValue(id);
  return q.exec() && q.next() ? q.value(0).toLongLong() : 0;
}

int RewindStore::count() {
  QSqlQuery q(db_);
  return q.exec("SELECT COUNT(*) FROM snapshots") && q.next() ? q.value(0).toInt() : 0;
}

qint64 RewindStore::totalBytes() {
  QSqlQuery q(db_);
  return q.exec("SELECT COALESCE(SUM(bytes), 0) FROM snapshots") && q.next() ? q.value(0).toLongLong() : 0;
}

void RewindStore::cleanup(int days, qint64 limitBytes) {
  QSqlQuery q(db_);
  auto drop = [this](const QString& where, const QVariant& bind) {
    QSqlQuery d(db_);
    if (fts_) {
      d.prepare("DELETE FROM rewind_fts WHERE rowid IN (SELECT id FROM snapshots WHERE " + where + ")");
      d.addBindValue(bind);
      d.exec();
    }
    d.prepare("DELETE FROM snapshots WHERE " + where);
    d.addBindValue(bind);
    d.exec();
  };
  if (days > 0) drop("ts < ?", now() - qint64(days) * 24 * 3600 * 1000);
  // Лимит места: удаляем самые старые, пока не уложимся.
  for (int guard = 0; guard < 200 && totalBytes() > limitBytes; ++guard) {
    QSqlQuery o(db_);
    if (!o.exec("SELECT ts FROM snapshots ORDER BY ts ASC LIMIT 1 OFFSET 20") || !o.next()) {
      drop("ts <= ?", now());   // осталось мало — удаляем всё сверх лимита
      break;
    }
    drop("ts <= ?", o.value(0));
  }
  q.exec("PRAGMA wal_checkpoint(TRUNCATE)");
}

void RewindStore::clear() {
  QSqlQuery q(db_);
  q.exec("DELETE FROM snapshots");
  if (fts_) q.exec("DELETE FROM rewind_fts");
  q.exec("VACUUM");
}

qint64 RewindStore::addWatch(const QString& url, const QString& selector, const QString& label, const QString& text) {
  QSqlQuery q(db_);
  q.prepare("INSERT INTO watches(url, selector, label, last_text, created, last_check) VALUES(?, ?, ?, ?, ?, ?)");
  q.addBindValue(url);
  q.addBindValue(selector);
  q.addBindValue(label);
  q.addBindValue(text);
  q.addBindValue(now());
  q.addBindValue(now());
  return q.exec() ? q.lastInsertId().toLongLong() : 0;
}

void RewindStore::removeWatch(qint64 id) {
  QSqlQuery q(db_);
  q.prepare("DELETE FROM watches WHERE id = ?");
  q.addBindValue(id);
  q.exec();
}

QVariantList RewindStore::watches() {
  QVariantList out;
  QSqlQuery q(db_);
  if (q.exec("SELECT id, url, selector, label, last_text, prev_text, created, last_check, changed_at, unseen "
             "FROM watches ORDER BY created DESC"))
    while (q.next())
      out << QVariantMap{{"id", q.value(0)}, {"url", q.value(1)}, {"selector", q.value(2)}, {"label", q.value(3)},
                         {"text", q.value(4)}, {"prevText", q.value(5)}, {"created", q.value(6)},
                         {"lastCheck", q.value(7)}, {"changedAt", q.value(8)}, {"unseen", q.value(9).toInt() != 0}};
  return out;
}

void RewindStore::updateWatch(qint64 id, const QString& text, bool changed) {
  QSqlQuery q(db_);
  if (changed) {
    q.prepare("UPDATE watches SET prev_text = last_text, last_text = ?, last_check = ?, changed_at = ?, unseen = 1 WHERE id = ?");
    q.addBindValue(text);
    q.addBindValue(now());
    q.addBindValue(now());
  } else {
    q.prepare("UPDATE watches SET last_check = ? WHERE id = ?");
    q.addBindValue(now());
  }
  q.addBindValue(id);
  q.exec();
}

void RewindStore::markWatchesSeen() { QSqlQuery(db_).exec("UPDATE watches SET unseen = 0"); }

}  // namespace crumb
