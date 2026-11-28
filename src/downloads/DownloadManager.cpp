#include "downloads/DownloadManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QQmlEngine>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>

#include "storage/SettingsStore.h"

namespace crumb {

DownloadManager* DownloadManager::instance() {
  static DownloadManager* self = new DownloadManager();
  return self;
}

DownloadManager* DownloadManager::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

DownloadManager::DownloadManager() {
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb";
  QDir().mkpath(dir);
  db_ = QSqlDatabase::addDatabase("QSQLITE", "crumb-downloads");
  db_.setDatabaseName(dir + "/crumb.db");
  if (!db_.open()) return;
  QSqlQuery(db_).exec(
      "CREATE TABLE IF NOT EXISTS downloads (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, path TEXT, "
      "url TEXT, total INTEGER, state TEXT, started INTEGER)");
  // История: последние 200 загрузок, новые сверху.
  QSqlQuery q(db_);
  q.exec("SELECT name, path, url, total, state, started FROM downloads ORDER BY id DESC LIMIT 200");
  while (q.next()) {
    Item it;
    it.uid = nextUid_++;
    it.persisted = true;
    it.info.name = q.value(0).toString();
    it.info.path = q.value(1).toString();
    it.info.url = q.value(2).toString();
    it.info.total = q.value(3).toLongLong();
    it.info.received = it.info.total;
    it.info.state = q.value(4).toString();
    it.info.percent = it.info.state == "done" ? 100 : -1;
    it.info.started = QDateTime::fromMSecsSinceEpoch(q.value(5).toLongLong());
    items_.append(it);
  }
}

QHash<int, QByteArray> DownloadManager::roleNames() const {
  return {{UidRole, "uid"}, {NameRole, "name"}, {PathRole, "path"}, {UrlRole, "url"},
          {ReceivedRole, "received"}, {TotalRole, "total"}, {SpeedRole, "speed"},
          {PercentRole, "percent"}, {StateRole, "status"}, {StartedRole, "started"}};
}

QVariant DownloadManager::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() >= items_.size()) return {};
  const Item& it = items_.at(index.row());
  switch (role) {
    case UidRole: return it.uid;
    case NameRole: return it.info.name;
    case PathRole: return it.info.path;
    case UrlRole: return it.info.url;
    case ReceivedRole: return double(it.info.received);
    case TotalRole: return double(it.info.total);
    case SpeedRole: return double(it.info.speed);
    case PercentRole: return it.info.percent;
    case StateRole: return it.info.state;
    case StartedRole: return it.info.started;
    default: return {};
  }
}

int DownloadManager::activeCount() const {
  int n = 0;
  for (const Item& it : items_)
    if (it.info.state == "progress" || it.info.state == "paused") ++n;
  return n;
}

double DownloadManager::activeProgress() const {
  qint64 rec = 0, tot = 0;
  for (const Item& it : items_) {
    if (it.info.state != "progress" && it.info.state != "paused") continue;
    if (it.info.total <= 0) return -1;
    rec += it.info.received;
    tot += it.info.total;
  }
  return tot > 0 ? double(rec) / double(tot) : -1;
}

int DownloadManager::rowOfUid(int uid) const {
  for (int i = 0; i < items_.size(); ++i)
    if (items_[i].uid == uid) return i;
  return -1;
}

int DownloadManager::rowOfCef(quint32 id) const {
  for (int i = 0; i < items_.size(); ++i)
    if (items_[i].info.cefId == id && !items_[i].persisted) return i;
  return -1;
}

void DownloadManager::update(const DownloadInfo& info, CefRefPtr<CefDownloadItemCallback> callback) {
  {
    std::lock_guard lock(mutex_);
    if (info.state == "progress" || info.state == "paused") callbacks_.insert(info.cefId, callback);
    else callbacks_.remove(info.cefId);
  }
  QMetaObject::invokeMethod(this, [this, info] { applyUpdate(info); }, Qt::QueuedConnection);
}

void DownloadManager::applyUpdate(const DownloadInfo& info) {
  int row = rowOfCef(info.cefId);
  if (row < 0) {
    if (info.path.isEmpty() && info.state == "progress" && info.received == 0 && info.name.isEmpty()) return;
    beginInsertRows({}, 0, 0);   // новые — сверху
    Item it;
    it.uid = nextUid_++;
    it.info = info;
    items_.prepend(it);
    endInsertRows();
    row = 0;
    emit downloadStarted(info.name);
  } else {
    DownloadInfo& cur = items_[row].info;
    const QDateTime started = cur.started;
    cur = info;
    if (started.isValid()) cur.started = started;
    emit dataChanged(index(row), index(row));
  }
  if (info.state == "done" || info.state == "canceled" || info.state == "failed") persist(row);
  emit statsChanged();
}

void DownloadManager::persist(int row) {
  Item& it = items_[row];
  if (it.persisted || !db_.isOpen()) return;
  QSqlQuery q(db_);
  q.prepare("INSERT INTO downloads(name, path, url, total, state, started) VALUES(?, ?, ?, ?, ?, ?)");
  q.addBindValue(it.info.name);
  q.addBindValue(it.info.path);
  q.addBindValue(it.info.url);
  q.addBindValue(it.info.total > 0 ? it.info.total : it.info.received);
  q.addBindValue(it.info.state);
  q.addBindValue(it.info.started.toMSecsSinceEpoch());
  q.exec();
  it.persisted = true;
}

namespace {
template <class F>
void withCallback(std::mutex& m, QHash<quint32, CefRefPtr<CefDownloadItemCallback>>& map, quint32 id, F&& f) {
  CefRefPtr<CefDownloadItemCallback> cb;
  {
    std::lock_guard lock(m);
    cb = map.value(id);
  }
  if (cb) f(cb);
}
}  // namespace

void DownloadManager::pause(int uid) {
  const int r = rowOfUid(uid);
  if (r >= 0) withCallback(mutex_, callbacks_, items_[r].info.cefId, [](auto cb) { cb->Pause(); });
}
void DownloadManager::resume(int uid) {
  const int r = rowOfUid(uid);
  if (r >= 0) withCallback(mutex_, callbacks_, items_[r].info.cefId, [](auto cb) { cb->Resume(); });
}
void DownloadManager::cancel(int uid) {
  const int r = rowOfUid(uid);
  if (r >= 0) withCallback(mutex_, callbacks_, items_[r].info.cefId, [](auto cb) { cb->Cancel(); });
}

void DownloadManager::open(int uid) {
  const int r = rowOfUid(uid);
  if (r >= 0 && QFileInfo::exists(items_[r].info.path))
    QDesktopServices::openUrl(QUrl::fromLocalFile(items_[r].info.path));
}

void DownloadManager::showInFolder(int uid) {
  const int r = rowOfUid(uid);
  if (r < 0) return;
  const QString p = QDir::toNativeSeparators(items_[r].info.path);
  if (QFileInfo::exists(p)) {
#if defined(_WIN32)
    QProcess::startDetached("explorer.exe", {"/select,", p});
#else
    // Файловый менеджер с выделенным файлом (Dolphin, Nautilus…); если нет — просто папка.
    const QString uri = QUrl::fromLocalFile(items_[r].info.path).toString(QUrl::FullyEncoded);
    if (!QProcess::startDetached("dbus-send", {"--session", "--dest=org.freedesktop.FileManager1", "--type=method_call",
                                               "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1.ShowItems",
                                               "array:string:" + uri, "string:"}))
      QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(p).absolutePath()));
#endif
  } else {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(p).absolutePath()));
  }
}

void DownloadManager::openFolder() {
  QDesktopServices::openUrl(QUrl::fromLocalFile(SettingsStore::instance()->get("downloadsDir").toString()));
}

void DownloadManager::remove(int uid) {
  const int r = rowOfUid(uid);
  if (r < 0) return;
  const DownloadInfo info = items_[r].info;
  if (info.state == "progress" || info.state == "paused") cancel(uid);
  if (db_.isOpen()) {
    QSqlQuery q(db_);
    q.prepare("DELETE FROM downloads WHERE path = ? AND started = ?");
    q.addBindValue(info.path);
    q.addBindValue(info.started.toMSecsSinceEpoch());
    q.exec();
  }
  beginRemoveRows({}, r, r);
  items_.removeAt(r);
  endRemoveRows();
  emit statsChanged();
}

void DownloadManager::clearFinished() {
  for (int i = items_.size() - 1; i >= 0; --i) {
    const QString st = items_[i].info.state;
    if (st == "progress" || st == "paused") continue;
    beginRemoveRows({}, i, i);
    items_.removeAt(i);
    endRemoveRows();
  }
  if (db_.isOpen()) QSqlQuery(db_).exec("DELETE FROM downloads");
  emit statsChanged();
}

QString DownloadManager::formatSize(double bytes) const {
  if (bytes < 0) return QStringLiteral("?");
  const QLocale ru(QLocale::Russian);
  if (bytes < 1024) return QStringLiteral("%1 Б").arg(qint64(bytes));
  if (bytes < 1024 * 1024) return ru.toString(bytes / 1024, 'f', 0) + QStringLiteral(" КБ");
  if (bytes < 1024.0 * 1024 * 1024) return ru.toString(bytes / 1024 / 1024, 'f', 1) + QStringLiteral(" МБ");
  return ru.toString(bytes / 1024 / 1024 / 1024, 'f', 2) + QStringLiteral(" ГБ");
}

}  // namespace crumb
