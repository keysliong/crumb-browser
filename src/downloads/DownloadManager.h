#pragma once
// Downloads — менеджер загрузок (QML-синглтон, модель списка).
// Данные приходят из CefDownloadHandler::OnDownloadUpdated (поток CEF) и
// пересылаются в поток Qt. Завершённые загрузки сохраняются в SQLite
// (таблица downloads в crumb.db), чтобы список пережил перезапуск.

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QSqlDatabase>
#include <QtQml/qqmlregistration.h>
#include <mutex>

#include "include/cef_download_handler.h"

class QQmlEngine;
class QJSEngine;

namespace crumb {

struct DownloadInfo {
  quint32 cefId = 0;
  QString name, path, url;
  qint64 received = 0, total = -1, speed = 0;
  int percent = -1;
  QString state;   // progress | paused | done | canceled | failed
  QDateTime started;
};

class DownloadManager final : public QAbstractListModel {
  Q_OBJECT
  QML_NAMED_ELEMENT(Downloads)
  QML_SINGLETON
  Q_PROPERTY(int activeCount READ activeCount NOTIFY statsChanged)
  Q_PROPERTY(double activeProgress READ activeProgress NOTIFY statsChanged)   // 0…1, -1 — неизвестно
  Q_PROPERTY(int count READ rowCount NOTIFY statsChanged)

 public:
  enum Roles { UidRole = Qt::UserRole + 1, NameRole, PathRole, UrlRole, ReceivedRole, TotalRole,
               SpeedRole, PercentRole, StateRole, StartedRole };

  static DownloadManager* instance();
  static DownloadManager* create(QQmlEngine*, QJSEngine*);

  int rowCount(const QModelIndex& = {}) const override { return int(items_.size()); }
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  int activeCount() const;
  double activeProgress() const;

  // Из потока CEF.
  void update(const DownloadInfo& info, CefRefPtr<CefDownloadItemCallback> callback);

  Q_INVOKABLE void pause(int uid);
  Q_INVOKABLE void resume(int uid);
  Q_INVOKABLE void cancel(int uid);
  Q_INVOKABLE void open(int uid);
  Q_INVOKABLE void showInFolder(int uid);
  Q_INVOKABLE void remove(int uid);
  Q_INVOKABLE void clearFinished();
  Q_INVOKABLE void openFolder();
  // «12,3 МБ», «850 КБ»
  Q_INVOKABLE QString formatSize(double bytes) const;

 signals:
  void statsChanged();
  void downloadStarted(const QString& name);

 private:
  DownloadManager();
  void applyUpdate(const DownloadInfo& info);
  void persist(int row);
  int rowOfUid(int uid) const;
  int rowOfCef(quint32 id) const;

  struct Item { int uid = 0; DownloadInfo info; bool persisted = false; };
  QList<Item> items_;
  int nextUid_ = 1;
  QSqlDatabase db_;

  std::mutex mutex_;
  QHash<quint32, CefRefPtr<CefDownloadItemCallback>> callbacks_;   // по id CEF
};

}  // namespace crumb
