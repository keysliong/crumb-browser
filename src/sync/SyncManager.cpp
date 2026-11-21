#include "sync/SyncManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUrlQuery>

#include "extensions/ExtensionManager.h"
#include "rewind/RewindManager.h"
#include "storage/SettingsStore.h"
#include "sync/SyncCrypto.h"

#ifndef CRUMB_GITHUB_CLIENT_ID
#define CRUMB_GITHUB_CLIENT_ID ""
#endif

namespace crumb {

namespace {
constexpr int kIterations = 200000;
const char* kFile = "crumb-sync.json";
const char* kDescription = "Crumb sync (зашифровано, не редактировать)";

QString localPath() {
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb/sync.dat";
}

// Эти настройки относятся к конкретному компьютеру и не синхронизируются.
bool syncable(const QString& key) {
  static const QStringList prefixes = {"sync", "debug", "window", "downloadsDir", "toolbarRewindAdded", "session"};
  for (const QString& p : prefixes)
    if (key.startsWith(p)) return false;
  return true;
}
}  // namespace

SyncManager* SyncManager::instance() {
  static SyncManager* self = new SyncManager();
  return self;
}

SyncManager* SyncManager::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

SyncManager::SyncManager() {
  net_ = new QNetworkAccessManager(this);
  pollTimer_ = new QTimer(this);
  pollTimer_->setSingleShot(true);
  connect(pollTimer_, &QTimer::timeout, this, &SyncManager::pollToken);
  pushTimer_ = new QTimer(this);
  pushTimer_->setSingleShot(true);
  pushTimer_->setInterval(8000);   // изменения копятся 8 секунд, затем одна отправка
  connect(pushTimer_, &QTimer::timeout, this, &SyncManager::syncNow);
  periodic_ = new QTimer(this);
  periodic_->setInterval(10 * 60 * 1000);
  connect(periodic_, &QTimer::timeout, this, &SyncManager::syncNow);

  connect(SettingsStore::instance(), &SettingsStore::settingChanged, this, &SyncManager::markDirty);
  connect(ExtensionManager::instance(), &ExtensionManager::itemsChanged, this, [this] {
    if (state_ != QLatin1String("ready")) return;
    // Список расширений тоже часть аккаунта; пропавшие из «недостающих» — уже установлены.
    QVariantList still;
    for (const QVariant& v : missingExt_)
      if (ExtensionManager::instance()->find(v.toMap().value("id").toString()).isEmpty()) still << v;
    if (still.size() != missingExt_.size()) { missingExt_ = still; emit syncedChanged(); }
    dirtyOther_ = true;
    pushTimer_->start();
  });
  connect(RewindManager::instance(), &RewindManager::watchesChanged, this, [this] {
    if (applying_ || state_ != QLatin1String("ready")) return;
    dirtyOther_ = true;
    pushTimer_->start();
  });

  loadLocal();
  if (!token_.isEmpty()) {
    fetchUser();
    if (!key_.isEmpty()) {
      setState("ready");
      QTimer::singleShot(4000, this, &SyncManager::syncNow);
      periodic_->start();
    }
  }
}

QString SyncManager::clientId() const {
  const QString fromSettings = SettingsStore::instance()->get("syncClientId").toString().trimmed();
  return fromSettings.isEmpty() ? QStringLiteral(CRUMB_GITHUB_CLIENT_ID) : fromSettings;
}

void SyncManager::setState(const QString& s) {
  if (s == state_) return;
  state_ = s;
  emit stateChanged();
}

void SyncManager::setBusy(bool b) {
  if (b == busy_) return;
  busy_ = b;
  emit busyChanged();
}

void SyncManager::setError(const QString& e) {
  error_ = e;
  emit errorChanged();
  if (!e.isEmpty()) qWarning("Sync: %s", qUtf8Printable(e));
}

QString SyncManager::lastSync() const {
  if (!lastSyncAt_) return {};
  const QDateTime d = QDateTime::fromMSecsSinceEpoch(lastSyncAt_);
  if (d.date() == QDate::currentDate()) return QStringLiteral("сегодня в ") + d.toString("HH:mm");
  return QLocale(QLocale::Russian).toString(d, "d MMMM в HH:mm");
}

// ------------------------------------------------------- локальное хранение

void SyncManager::loadLocal() {
  QFile f(localPath());
  if (!f.open(QIODevice::ReadOnly)) return;
  const QJsonObject o = QJsonDocument::fromJson(crypto::unprotect(f.readAll())).object();
  token_ = o.value("token").toString();
  key_ = QByteArray::fromBase64(o.value("key").toString().toLatin1());
  salt_ = QByteArray::fromBase64(o.value("salt").toString().toLatin1());
  gistId_ = o.value("gist").toString();
  login_ = o.value("login").toString();
  name_ = o.value("name").toString();
  avatar_ = o.value("avatar").toString();
  lastSyncAt_ = qint64(o.value("lastSyncAt").toDouble());
  for (const QJsonValue& v : o.value("dismissed").toArray()) dismissedExt_ << v.toString();
  for (const QJsonValue& v : o.value("missing").toArray()) missingExt_ << v.toObject().toVariantMap();
}

void SyncManager::saveLocal() {
  QJsonObject o{{"token", token_}, {"key", QString::fromLatin1(key_.toBase64())},
                {"salt", QString::fromLatin1(salt_.toBase64())}, {"gist", gistId_}, {"login", login_},
                {"name", name_}, {"avatar", avatar_}, {"lastSyncAt", double(lastSyncAt_)},
                {"dismissed", QJsonArray::fromStringList(dismissedExt_)},
                {"missing", QJsonArray::fromVariantList(missingExt_)}};
  QDir().mkpath(QFileInfo(localPath()).path());
  QFile f(localPath());
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    f.write(crypto::protect(QJsonDocument(o).toJson(QJsonDocument::Compact)));
}

// ------------------------------------------------------------------ сеть

QNetworkReply* SyncManager::api(const QByteArray& verb, const QString& path, const QJsonObject& body) {
  QNetworkRequest r(QUrl(path.startsWith("http") ? path : "https://api.github.com" + path));
  r.setRawHeader("Accept", "application/vnd.github+json");
  r.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
  r.setRawHeader("User-Agent", "Crumb-Browser");
  if (!token_.isEmpty()) r.setRawHeader("Authorization", "Bearer " + token_.toUtf8());
  r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  r.setTransferTimeout(30000);
  const QByteArray data = body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact);
  return net_->sendCustomRequest(r, verb, data);
}

// ------------------------------------------------------------------ вход

void SyncManager::signIn() {
  if (clientId().isEmpty()) {
    setError(QStringLiteral("Не задан Client ID приложения GitHub (Настройки → Аккаунт)."));
    return;
  }
  setError({});
  setBusy(true);
  QNetworkRequest r(QUrl("https://github.com/login/device/code"));
  r.setRawHeader("Accept", "application/json");
  r.setRawHeader("User-Agent", "Crumb-Browser");
  r.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  QUrlQuery q;
  q.addQueryItem("client_id", clientId());
  q.addQueryItem("scope", "gist read:user");
  QNetworkReply* reply = net_->post(r, q.query(QUrl::FullyEncoded).toUtf8());
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    setBusy(false);
    const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
    if (reply->error() != QNetworkReply::NoError || !o.contains("device_code")) {
      setError(QStringLiteral("GitHub не выдал код входа: %1")
                   .arg(o.value("error_description").toString(reply->errorString())));
      return;
    }
    deviceCode_ = o.value("device_code").toString();
    userCode_ = o.value("user_code").toString();
    verifyUrl_ = o.value("verification_uri").toString("https://github.com/login/device");
    pollInterval_ = qMax(5, o.value("interval").toInt(5));
    setState("waitingCode");
    emit stateChanged();
    emit openUrlRequested(verifyUrl_);
    pollTimer_->start(pollInterval_ * 1000);
  });
}

void SyncManager::cancelSignIn() {
  pollTimer_->stop();
  deviceCode_.clear();
  userCode_.clear();
  setState(token_.isEmpty() ? "signedOut" : state_);
}

void SyncManager::pollToken() {
  if (deviceCode_.isEmpty()) return;
  QNetworkRequest r(QUrl("https://github.com/login/oauth/access_token"));
  r.setRawHeader("Accept", "application/json");
  r.setRawHeader("User-Agent", "Crumb-Browser");
  r.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  QUrlQuery q;
  q.addQueryItem("client_id", clientId());
  q.addQueryItem("device_code", deviceCode_);
  q.addQueryItem("grant_type", "urn:ietf:params:oauth:grant-type:device_code");
  QNetworkReply* reply = net_->post(r, q.query(QUrl::FullyEncoded).toUtf8());
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
    const QString err = o.value("error").toString();
    if (err == "authorization_pending" || (err.isEmpty() && !o.contains("access_token"))) {
      pollTimer_->start(pollInterval_ * 1000);
      return;
    }
    if (err == "slow_down") {
      pollInterval_ += 5;
      pollTimer_->start(pollInterval_ * 1000);
      return;
    }
    if (!err.isEmpty()) {
      setError(err == "expired_token" ? QStringLiteral("Код устарел — начните вход заново.")
               : err == "access_denied" ? QStringLiteral("Вход отменён на GitHub.")
                                        : o.value("error_description").toString(err));
      cancelSignIn();
      return;
    }
    token_ = o.value("access_token").toString();
    deviceCode_.clear();
    userCode_.clear();
    saveLocal();
    fetchUser();
    setBusy(true);
    findGist();
  });
}

void SyncManager::fetchUser() {
  QNetworkReply* reply = api("GET", "/user");
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (code == 401) {   // токен отозван на GitHub
      setError(QStringLiteral("Доступ к GitHub отозван — войдите снова."));
      signOut(false);
      return;
    }
    const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
    if (!o.contains("login")) return;
    login_ = o.value("login").toString();
    name_ = o.value("name").toString(login_);
    if (name_.isEmpty()) name_ = login_;
    avatar_ = o.value("avatar_url").toString();
    saveLocal();
    emit accountChanged();
  });
}

// Ищем свой Gist среди Gist'ов пользователя (по имени файла).
void SyncManager::findGist(int page) {
  QNetworkReply* reply = api("GET", QStringLiteral("/gists?per_page=100&page=%1").arg(page));
  connect(reply, &QNetworkReply::finished, this, [this, reply, page] {
    reply->deleteLater();
    const QJsonArray list = QJsonDocument::fromJson(reply->readAll()).array();
    for (const QJsonValue& v : list) {
      const QJsonObject g = v.toObject();
      if (g.value("files").toObject().contains(kFile)) {
        gistId_ = g.value("id").toString();
        break;
      }
    }
    if (gistId_.isEmpty() && list.size() == 100) { findGist(page + 1); return; }
    saveLocal();
    if (gistId_.isEmpty()) {   // первое устройство — придумать пароль
      remoteBlob_.clear();
      setBusy(false);
      setState("needPassphrase");
      return;
    }
    // Загрузим данные, чтобы узнать соль и проверить пароль.
    QNetworkReply* g = api("GET", "/gists/" + gistId_);
    connect(g, &QNetworkReply::finished, this, [this, g] {
      g->deleteLater();
      const QJsonObject file = QJsonDocument::fromJson(g->readAll()).object()
                                   .value("files").toObject().value(kFile).toObject();
      remoteBlob_ = file.value("content").toString().toUtf8();
      const QJsonObject env = QJsonDocument::fromJson(remoteBlob_).object();
      salt_ = QByteArray::fromBase64(env.value("salt").toString().toLatin1());
      setBusy(false);
      setState(key_.isEmpty() ? "needPassphrase" : "ready");
      emit stateChanged();
      if (!key_.isEmpty()) pull(true);
    });
  });
}

bool SyncManager::setPassphrase(const QString& pass) {
  if (pass.size() < 6) {
    setError(QStringLiteral("Пароль синхронизации — не короче 6 символов."));
    return false;
  }
  if (remoteExists()) {
    const QByteArray key = crypto::deriveKey(pass.toUtf8(), salt_, kIterations);
    key_ = key;
    QJsonObject data;
    if (!decryptRemote(&data)) {
      key_.clear();
      setError(QStringLiteral("Неверный пароль синхронизации."));
      return false;
    }
    setError({});
    saveLocal();
    setState("ready");
    // Новое устройство: облако главнее — берём всё оттуда, затем отправляем объединённое.
    lastSyncAt_ = 0;
    dirtyKeys_.clear();
    applyRemote(data);
    push();
  } else {
    salt_ = crypto::randomBytes(16);
    key_ = crypto::deriveKey(pass.toUtf8(), salt_, kIterations);
    setError({});
    saveLocal();
    setState("ready");
    push();   // первое устройство — создаём Gist
  }
  periodic_->start();
  return true;
}

bool SyncManager::resetPassphrase(const QString& pass) {
  if (token_.isEmpty()) return false;
  if (pass.size() < 6) {
    setError(QStringLiteral("Пароль синхронизации — не короче 6 символов."));
    return false;
  }
  // Новая соль и ключ; содержимое Gist заменится данными с этого компьютера (тот же Gist).
  // Другие компьютеры не смогут расшифровать новое — попросят ввести новый пароль.
  salt_ = crypto::randomBytes(16);
  key_ = crypto::deriveKey(pass.toUtf8(), salt_, kIterations);
  remoteBlob_.clear();
  lastSyncAt_ = 0;
  dirtyKeys_.clear();
  dirtyOther_ = false;
  setError({});
  saveLocal();
  setState("ready");
  emit stateChanged();
  push();
  periodic_->start();
  return true;
}

void SyncManager::signOut(bool deleteCloud) {
  if (deleteCloud && !gistId_.isEmpty() && !token_.isEmpty()) {
    QNetworkReply* r = api("DELETE", "/gists/" + gistId_);
    connect(r, &QNetworkReply::finished, r, &QObject::deleteLater);
  }
  pollTimer_->stop();
  pushTimer_->stop();
  periodic_->stop();
  token_.clear();
  key_.clear();
  salt_.clear();
  gistId_.clear();
  remoteBlob_.clear();
  login_.clear();
  name_.clear();
  avatar_.clear();
  lastSyncAt_ = 0;
  missingExt_.clear();
  QFile::remove(localPath());
  setState("signedOut");
  emit accountChanged();
  emit syncedChanged();
}

// --------------------------------------------------------- синхронизация

void SyncManager::markDirty(const QString& key) {
  if (applying_ || !syncable(key)) return;
  dirtyKeys_.insert(key);
  if (state_ == QLatin1String("ready")) pushTimer_->start();
}

void SyncManager::syncNow() {
  if (state_ != QLatin1String("ready") || busy_) return;
  pull(true);
}

bool SyncManager::decryptRemote(QJsonObject* out) {
  const QJsonObject env = QJsonDocument::fromJson(remoteBlob_).object();
  bool ok = false;
  const QByteArray plain = crypto::open(key_, QByteArray::fromBase64(env.value("data").toString().toLatin1()), &ok);
  if (!ok) return false;
  *out = QJsonDocument::fromJson(plain).object();
  return true;
}

void SyncManager::pull(bool thenPush) {
  if (gistId_.isEmpty()) { push(); return; }
  setBusy(true);
  QNetworkReply* reply = api("GET", "/gists/" + gistId_);
  connect(reply, &QNetworkReply::finished, this, [this, reply, thenPush] {
    reply->deleteLater();
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (code == 404) {   // Gist удалили на сайте — создадим заново
      gistId_.clear();
      setBusy(false);
      push();
      return;
    }
    if (reply->error() != QNetworkReply::NoError) {
      setBusy(false);
      setError(QStringLiteral("Нет связи с GitHub: %1").arg(reply->errorString()));
      return;
    }
    const QJsonObject file = QJsonDocument::fromJson(reply->readAll()).object()
                                 .value("files").toObject().value(kFile).toObject();
    remoteBlob_ = file.value("content").toString().toUtf8();
    QJsonObject data;
    if (!decryptRemote(&data)) {
      setBusy(false);
      // Пароль сменили на другом устройстве.
      key_.clear();
      saveLocal();
      setState("needPassphrase");
      setError(QStringLiteral("Пароль синхронизации изменился — введите его снова."));
      return;
    }
    const qint64 remoteAt = qint64(data.value("updatedAt").toDouble());
    const bool remoteNewer = remoteAt > lastSyncAt_;
    if (remoteNewer) applyRemote(data);
    setBusy(false);
    setError({});
    if (thenPush && (!dirtyKeys_.isEmpty() || dirtyOther_)) {
      push();
    } else {
      if (remoteNewer) lastSyncAt_ = remoteAt;
      saveLocal();
      emit syncedChanged();
    }
  });
}

QJsonObject SyncManager::buildLocal() const {
  QJsonObject settings;
  const QVariantMap snap = SettingsStore::instance()->snapshot();
  for (auto it = snap.cbegin(); it != snap.cend(); ++it)
    if (syncable(it.key())) settings.insert(it.key(), QJsonValue::fromVariant(it.value()));

  QJsonArray exts;
  for (const QVariant& v : ExtensionManager::instance()->items()) {
    const QVariantMap e = v.toMap();
    exts.append(QJsonObject{{"id", e.value("id").toString()}, {"name", e.value("name").toString()},
                            {"enabled", e.value("enabled").toBool()}});
  }
  // Расширения, которые есть в аккаунте, но ещё не поставлены здесь, не теряем.
  for (const QVariant& v : missingExt_) {
    const QVariantMap e = v.toMap();
    if (!dismissedExt_.contains(e.value("id").toString())) exts.append(QJsonObject::fromVariantMap(e));
  }

  QJsonArray watches;
  for (const QVariant& v : RewindManager::instance()->watches()) {
    const QVariantMap w = v.toMap();
    watches.append(QJsonObject{{"url", w.value("url").toString()}, {"selector", w.value("selector").toString()},
                               {"label", w.value("label").toString()}, {"text", w.value("text").toString()}});
  }
  return QJsonObject{{"format", 1},
                     {"updatedAt", double(QDateTime::currentMSecsSinceEpoch())},
                     {"device", QSysInfo::machineHostName()},
                     {"settings", settings},
                     {"extensions", exts},
                     {"rewindWatches", watches}};
}

void SyncManager::applyRemote(const QJsonObject& data) {
  applying_ = true;
  // Настройки: всё из облака, кроме изменённого здесь с прошлой синхронизации.
  QVariantMap values;
  const QJsonObject settings = data.value("settings").toObject();
  for (auto it = settings.begin(); it != settings.end(); ++it)
    if (syncable(it.key()) && !dirtyKeys_.contains(it.key())) values.insert(it.key(), it.value().toVariant());
  const QVariantMap current = SettingsStore::instance()->snapshot();
  QVariantMap changed;
  for (auto it = values.cbegin(); it != values.cend(); ++it)
    if (current.value(it.key()) != it.value()) changed.insert(it.key(), it.value());
  if (!changed.isEmpty()) SettingsStore::instance()->applyAll(changed);

  // Расширения: каких нет здесь — предложим установить из Интернет-магазина.
  QVariantList missing;
  for (const QJsonValue& v : data.value("extensions").toArray()) {
    const QJsonObject e = v.toObject();
    const QString id = e.value("id").toString();
    if (id.isEmpty() || dismissedExt_.contains(id)) continue;
    if (ExtensionManager::instance()->ready() && !ExtensionManager::instance()->find(id).isEmpty()) continue;
    missing << e.toVariantMap();
  }
  missingExt_ = missing;

  // Подписки Rewind: добавить недостающие.
  RewindManager::instance()->importWatches(data.value("rewindWatches").toArray().toVariantList());
  applying_ = false;
  emit syncedChanged();
}

void SyncManager::push() {
  if (key_.isEmpty() || token_.isEmpty()) return;
  setBusy(true);
  const QJsonObject local = buildLocal();
  const QByteArray sealed = crypto::seal(key_, QJsonDocument(local).toJson(QJsonDocument::Compact));
  const QJsonObject env{{"crumbSync", 1}, {"cipher", "AES-256-GCM"}, {"kdf", "PBKDF2-SHA256"},
                        {"iterations", kIterations}, {"salt", QString::fromLatin1(salt_.toBase64())},
                        {"data", QString::fromLatin1(sealed.toBase64())}};
  const QString content = QString::fromUtf8(QJsonDocument(env).toJson(QJsonDocument::Indented));
  const QJsonObject body{{"description", kDescription},
                         {"files", QJsonObject{{kFile, QJsonObject{{"content", content}}}}}};
  QJsonObject createBody = body;
  createBody.insert("public", false);
  QNetworkReply* reply = gistId_.isEmpty() ? api("POST", "/gists", createBody) : api("PATCH", "/gists/" + gistId_, body);
  const qint64 at = qint64(local.value("updatedAt").toDouble());
  const QSet<QString> sent = dirtyKeys_;
  connect(reply, &QNetworkReply::finished, this, [this, reply, at, sent, content] {
    reply->deleteLater();
    setBusy(false);
    const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
    if (reply->error() != QNetworkReply::NoError || !o.contains("id")) {
      setError(QStringLiteral("Не удалось сохранить в GitHub: %1").arg(o.value("message").toString(reply->errorString())));
      return;
    }
    gistId_ = o.value("id").toString();
    remoteBlob_ = content.toUtf8();
    lastSyncAt_ = at;
    dirtyKeys_.subtract(sent);
    dirtyOther_ = false;
    setError({});
    saveLocal();
    emit stateChanged();
    emit syncedChanged();
  });
}

void SyncManager::dismissExtension(const QString& id) {
  dismissedExt_ << id;
  QVariantList still;
  for (const QVariant& v : missingExt_)
    if (v.toMap().value("id").toString() != id) still << v;
  missingExt_ = still;
  saveLocal();
  emit syncedChanged();
  dirtyOther_ = true;
  pushTimer_->start();
}

}  // namespace crumb
