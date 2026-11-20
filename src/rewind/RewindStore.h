#pragma once
// Хранилище Rewind: снимки страниц и подписки на изменения (SQLite rewind.db).
// Текст и HTML сжаты (qCompress), миниатюра — JPEG. Полнотекстовый поиск — FTS5.
// Всё хранится только на этом компьютере.

#include <QByteArray>
#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

namespace crumb {

struct Snapshot {
  qint64 id = 0;
  QString url, host, title;
  qint64 ts = 0, tsLast = 0;      // когда снят и когда видели последний раз (мс)
  QString text, html;
  QByteArray thumb;
  qint64 bytes = 0;
};

class RewindStore {
 public:
  bool open(const QString& path);

  // Новый снимок. Если текст не изменился с прошлого снимка этой страницы —
  // обновляется только «видели последний раз», возвращается id прежнего (и dup=true).
  qint64 add(const QString& url, const QString& title, const QString& text, const QString& html, bool* dup);
  void setThumb(qint64 id, const QByteArray& jpeg);
  QByteArray thumb(qint64 id);
  bool load(qint64 id, Snapshot* out);

  QVariantList listForUrl(const QString& url, int limit = 200);
  QVariantList search(const QString& query, int limit = 60);
  qint64 latestFor(const QString& url);
  qint64 previousOf(qint64 id);

  int count();
  qint64 totalBytes();
  void cleanup(int days, qint64 limitBytes);
  void clear();

  // Подписки
  qint64 addWatch(const QString& url, const QString& selector, const QString& label, const QString& text);
  void removeWatch(qint64 id);
  QVariantList watches();
  void updateWatch(qint64 id, const QString& text, bool changed);
  void markWatchesSeen();

 private:
  QSqlDatabase db_;
  bool fts_ = false;
};

}  // namespace crumb
