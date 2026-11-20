#pragma once
// Аккаунт Crumb через GitHub — синхронизация без собственного сервера (QML-синглтон Sync).
//
// Вход: GitHub Device Flow (код + github.com/login/device), право только «gist».
// Хранилище: секретный Gist «crumb-sync.json» в аккаунте пользователя.
// Шифрование: AES-256-GCM, ключ = PBKDF2-SHA256(пароль синхронизации, соль, 200 000).
//   GitHub видит только зашифрованные байты. Пароль знает только пользователь.
// Локально токен и ключ защищены DPAPI (расшифровать может только эта учётная запись Windows).
//
// Что синхронизируется: все настройки (тема, печеньки, домашняя страница, быстрые сайты,
// поисковик, клавиши…), список расширений и подписки Rewind.
// Слияние: изменённые на этом устройстве ключи важнее, остальное берётся из облака.

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;
class QJSEngine;
class QTimer;

namespace crumb {

class SyncManager final : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Sync)
  QML_SINGLETON
  // signedOut | waitingCode | needPassphrase | ready
  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString login READ login NOTIFY accountChanged)
  Q_PROPERTY(QString name READ name NOTIFY accountChanged)
  Q_PROPERTY(QString avatar READ avatar NOTIFY accountChanged)
  Q_PROPERTY(QString userCode READ userCode NOTIFY stateChanged)
  Q_PROPERTY(QString verifyUrl READ verifyUrl NOTIFY stateChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(bool remoteExists READ remoteExists NOTIFY stateChanged)   // в облаке уже есть данные
  Q_PROPERTY(QString lastSync READ lastSync NOTIFY syncedChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(QVariantList missingExtensions READ missingExtensions NOTIFY syncedChanged)
  Q_PROPERTY(bool configured READ configured CONSTANT)                  // есть Client ID

 public:
  static SyncManager* instance();
  static SyncManager* create(QQmlEngine*, QJSEngine*);

  QString state() const { return state_; }
  QString login() const { return login_; }
  QString name() const { return name_; }
  QString avatar() const { return avatar_; }
  QString userCode() const { return userCode_; }
  QString verifyUrl() const { return verifyUrl_; }
  bool busy() const { return busy_; }
  bool remoteExists() const { return !gistId_.isEmpty() && !remoteBlob_.isEmpty(); }
  QString lastSync() const;
  QString error() const { return error_; }
  QVariantList missingExtensions() const { return missingExt_; }
  bool configured() const { return !clientId().isEmpty(); }

  Q_INVOKABLE void signIn();                              // начать вход (получить код)
  Q_INVOKABLE void cancelSignIn();
  Q_INVOKABLE bool setPassphrase(const QString& pass);    // создать / ввести пароль синхронизации
  Q_INVOKABLE void syncNow();
  Q_INVOKABLE void signOut(bool deleteCloud = false);
  Q_INVOKABLE void dismissExtension(const QString& id);

 signals:
  void stateChanged();
  void accountChanged();
  void busyChanged();
  void syncedChanged();
  void errorChanged();
  void openUrlRequested(const QString& url);

 private:
  SyncManager();
  QString clientId() const;
  void setState(const QString& s);
  void setBusy(bool b);
  void setError(const QString& e);
  void loadLocal();
  void saveLocal();

  QNetworkReply* api(const QByteArray& verb, const QString& path, const QJsonObject& body = {});
  void pollToken();
  void fetchUser();
  void findGist(int page = 1);
  void pull(bool thenPush);
  void push();
  void applyRemote(const QJsonObject& data);
  QJsonObject buildLocal() const;
  bool decryptRemote(QJsonObject* out);
  void markDirty(const QString& key);

  QNetworkAccessManager* net_ = nullptr;
  QTimer* pollTimer_ = nullptr;
  QTimer* pushTimer_ = nullptr;
  QTimer* periodic_ = nullptr;

  QString state_ = QStringLiteral("signedOut");
  QString token_, login_, name_, avatar_;
  QString deviceCode_, userCode_, verifyUrl_;
  int pollInterval_ = 5;
  bool busy_ = false;
  QString error_;

  QString gistId_;
  QByteArray remoteBlob_;      // содержимое crumb-sync.json как есть (зашифрованное)
  QByteArray key_, salt_;      // ключ AES и соль
  qint64 lastSyncAt_ = 0;      // updatedAt последней синхронизированной версии
  QSet<QString> dirtyKeys_;    // изменённые здесь с последней синхронизации
  bool dirtyOther_ = false;    // изменились расширения / подписки
  bool applying_ = false;
  QVariantList missingExt_;
  QStringList dismissedExt_;
};

}  // namespace crumb
