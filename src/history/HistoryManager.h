#pragma once
// History — история посещений Crumb (QML-синглтон).
//
// Каждый переход главного фрейма записывается в history.db (%LOCALAPPDATA%\Crumb):
//   visits — каждое посещение (адрес, заголовок, время);
//   urls   — сводка по адресу (сколько раз, когда последний) для подсказок в адресной строке;
//   icons  — значок сайта по хосту (для истории, подсказок и восстановленных вкладок).
// Пишет BrowserView (поток Qt). Не записываются не-http(s) адреса и всё,
// если история выключена в настройках.

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace crumb {

class HistoryManager final : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(History)
  QML_SINGLETON
  Q_PROPERTY(int revision READ revision NOTIFY changed)   // меняется при любом изменении
  Q_PROPERTY(int count READ count NOTIFY changed)

 public:
  static HistoryManager* instance();
  static HistoryManager* create(QQmlEngine*, QJSEngine*);

  void open();
  int revision() const { return revision_; }
  int count() const;

  // --- из BrowserView
  void visit(const QString& url, const QString& title);
  void setTitle(const QString& url, const QString& title);
  void setIcon(const QString& url, const QString& dataUrl);

  // --- для QML
  // Посещения (новые сверху), с фильтром по тексту. Каждый элемент:
  // { id, url, title, host, ts, time, day, icon }.
  Q_INVOKABLE QVariantList query(const QString& text, int limit = 400, double beforeTs = 0) const;
  // Подсказки адресной строки: { url, title, host, icon }.
  Q_INVOKABLE QVariantList suggest(const QString& text, int limit = 6) const;
  Q_INVOKABLE QString iconFor(const QString& url) const;
  Q_INVOKABLE void remove(int id);
  Q_INVOKABLE void removeSite(const QString& host);
  // Удалить посещения за последние minutes минут (0 — всё).
  Q_INVOKABLE void clear(int minutes);
  Q_INVOKABLE QString hostOf(const QString& url) const;

 signals:
  void changed();

 private:
  HistoryManager() = default;
  void bump();
  static bool eligible(const QString& url);

  QSqlDatabase db_;
  int revision_ = 0;
  QString lastUrl_;
  qint64 lastTs_ = 0, lastId_ = 0;
};

}  // namespace crumb
