#pragma once
// Passwords — собственный менеджер паролей Crumb (QML-синглтон).
//
// • Хранение: passwords.db (%LOCALAPPDATA%\Crumb), пароль зашифрован DPAPI —
//   прочитать его может только эта учётная запись Windows.
// • Сайт: скрипт captureJs() в главном фрейме замечает отправку формы входа
//   («CRUMB_PW_SAVE:…») и фокус в поле логина/пароля («CRUMB_PW_FOCUS:…»).
//   Crumb предлагает сохранить пароль и подставить сохранённый — свои окна.
// • Показать/скопировать пароль на странице crumb://passwords можно только
//   после Windows Hello (или пароля Windows); доступ открыт на 5 минут.
// • Встроенный менеджер паролей Chromium выключен, чтобы не было двух.

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class QTimer;

namespace crumb {

class PasswordManager final : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Passwords)
  QML_SINGLETON
  Q_PROPERTY(int revision READ revision NOTIFY changed)
  Q_PROPERTY(int count READ count NOTIFY changed)
  Q_PROPERTY(bool unlocked READ unlocked NOTIFY unlockedChanged)
  Q_PROPERTY(bool verifying READ verifying NOTIFY unlockedChanged)

 public:
  static PasswordManager* instance();
  static PasswordManager* create(QQmlEngine*, QJSEngine*);

  void open();
  int revision() const { return revision_; }
  int count() const;
  bool unlocked() const { return unlocked_; }
  bool verifying() const { return verifying_; }

  // --- скрипты для страницы
  Q_INVOKABLE QString captureJs() const;
  // JS, который подставит логин и пароль записи id в форму на странице.
  Q_INVOKABLE QString fillJs(int id);

  // --- сохранение с сайта
  // "new" — предложить сохранить, "update" — предложить обновить пароль,
  // "same" — уже сохранён, "never" — сайт в списке «никогда», "off" — выключено.
  Q_INVOKABLE QString check(const QString& url, const QString& username, const QString& password) const;
  Q_INVOKABLE void save(const QString& url, const QString& username, const QString& password);
  Q_INVOKABLE void neverFor(const QString& url);
  Q_INVOKABLE void allowSite(const QString& site);   // убрать из «никогда»

  // --- просмотр
  // Записи для сайта страницы (без паролей): { id, username, site }.
  Q_INVOKABLE QVariantList forSite(const QString& url) const;
  // Все записи (без паролей): { id, site, origin, username, updated, used, icon }.
  Q_INVOKABLE QVariantList list(const QString& filter) const;
  Q_INVOKABLE QString siteOf(const QString& url) const;

  // --- действия, которым нужна проверка личности
  Q_INVOKABLE void unlock(QWindow* window);
  Q_INVOKABLE void lock();
  Q_INVOKABLE QString password(int id) const;          // "" если не разблокировано
  Q_INVOKABLE bool copyPassword(int id);               // в буфер обмена, очистится через 30 с
  Q_INVOKABLE void copyText(const QString& text);
  Q_INVOKABLE bool update(int id, const QString& username, const QString& password);
  Q_INVOKABLE void add(const QString& url, const QString& username, const QString& password);
  Q_INVOKABLE void remove(int id);
  Q_INVOKABLE QString generate(int length = 18) const;
  // Насколько надёжен пароль: 0 — слабый … 3 — надёжный.
  Q_INVOKABLE int strength(const QString& password) const;

 signals:
  void changed();
  void unlockedChanged();
  void unlockFailed();

 private:
  PasswordManager();
  void bump();
  QByteArray decrypt(const QByteArray& blob) const;

  QSqlDatabase db_;
  int revision_ = 0;
  bool unlocked_ = false, verifying_ = false;
  QTimer* lockTimer_ = nullptr;
  QTimer* clipTimer_ = nullptr;
  QString clipValue_;
};

}  // namespace crumb
