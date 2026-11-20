#include "passwords/PasswordManager.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QPointer>
#include <QQmlEngine>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <thread>

#include "history/HistoryManager.h"
#include "passwords/WinVerify.h"
#include "storage/SettingsStore.h"
#include "sync/SyncCrypto.h"

namespace crumb {

namespace {
qint64 now() { return QDateTime::currentMSecsSinceEpoch(); }

// Сайт записи: хост без «www.» (вход на www.site.ru и site.ru — одно и то же).
QString siteFor(const QString& url) {
  QString h = QUrl(url).host().toLower();
  if (h.startsWith(QLatin1String("www."))) h = h.mid(4);
  return h;
}
QString originFor(const QString& url) {
  const QUrl u(url);
  return u.scheme() + "://" + u.host() + (u.port() > 0 ? ":" + QString::number(u.port()) : QString());
}

QString jsString(const QString& s) {
  // JSON-строка — безопасный литерал JavaScript (кавычки, переводы строк, </script>).
  QString j = QString::fromUtf8(QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact));
  j = j.mid(1, j.size() - 2);
  return j.replace("<", "\\u003c").replace(QChar(0x2028), "\\u2028").replace(QChar(0x2029), "\\u2029");
}

// Скрипт в главном фрейме сайта. Ставится один раз на документ.
const char* kCaptureJs = R"JS((function(){
if (window.__crumbPw) return;
const S = window.__crumbPw = {};
const log = console.log.bind(console);
let lastUser = '', sent = '', lastFocus = null, quiet = false;
const TEXT = /^(text|email|tel|)$/;
const USERISH = /user|login|mail|e-mail|phone|account|ident|логин|почт/i;
const vis = el => !!(el && (el.offsetWidth || el.offsetHeight || el.getClientRects().length));
const pwFields = () => Array.from(document.querySelectorAll('input[type=password]')).filter(vis);
function userFor(pw) {
  const scope = (pw && pw.form) || document;
  let cand = null;
  for (const i of scope.querySelectorAll('input')) {
    if (!vis(i) || !TEXT.test(i.type) || i.readOnly) continue;
    if (!pw || (i.compareDocumentPosition(pw) & Node.DOCUMENT_POSITION_FOLLOWING)) cand = i;
  }
  return cand;
}
function capture() {
  const filled = pwFields().filter(p => p.value);
  if (!filled.length) return;
  const pw = filled[0];
  // Регистрация/смена пароля: берём новый (последний заполненный) пароль.
  const value = filled.length >= 2 ? filled[filled.length - 1].value : pw.value;
  const uf = userFor(pw);
  const user = (uf && uf.value) || lastUser || '';
  const key = user + '\u0000' + value;
  if (key === sent) return;
  sent = key;
  log('CRUMB_PW_SAVE:' + JSON.stringify({o: location.origin, u: user, p: value}));
}
document.addEventListener('input', e => {
  const t = e.target;
  if (t && t.tagName === 'INPUT' && TEXT.test(t.type) &&
      (USERISH.test((t.name || '') + ' ' + (t.id || '') + ' ' + (t.autocomplete || '') + ' ' + t.type) || pwFields().length))
    lastUser = t.value;
}, true);
document.addEventListener('submit', capture, true);
document.addEventListener('click', e => {
  const b = e.target && e.target.closest && e.target.closest('button,input[type=submit],input[type=button],[role=button]');
  if (b && pwFields().some(p => p.value)) setTimeout(capture, 0);
}, true);
document.addEventListener('keydown', e => {
  if (e.key === 'Enter' && e.target && e.target.tagName === 'INPUT' && pwFields().some(p => p.value)) capture();
}, true);
window.addEventListener('pagehide', capture, true);
function loginField(t) {
  if (!t || t.tagName !== 'INPUT' || t.readOnly || t.disabled) return '';
  if (t.type === 'password') return 'p';
  if (!TEXT.test(t.type)) return '';
  const pws = pwFields();
  if (pws.length && userFor(pws[0]) === t) return 'u';
  if (USERISH.test((t.name || '') + ' ' + (t.id || '') + ' ' + (t.autocomplete || ''))) return 'u';
  return '';
}
function report(t) {
  const r = t.getBoundingClientRect();
  log('CRUMB_PW_FOCUS:' + JSON.stringify({x: r.left, y: r.top, w: r.width, h: r.height,
                                           dpr: window.devicePixelRatio, k: loginField(t)}));
}
document.addEventListener('focusin', e => {
  if (quiet) return;
  const k = loginField(e.target);
  if (!k) return;
  lastFocus = e.target;
  report(e.target);
}, true);
document.addEventListener('mousedown', e => {
  if (e.target === lastFocus && document.activeElement === e.target) report(e.target);
}, true);
document.addEventListener('focusout', e => {
  if (e.relatedTarget) log('CRUMB_PW_BLUR:');
}, true);
window.addEventListener('scroll', () => { if (lastFocus) log('CRUMB_PW_BLUR:'); lastFocus = null; }, {passive: true, capture: true});
function setVal(el, v) {
  const d = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value');
  el.focus();
  d.set.call(el, v);
  el.dispatchEvent(new Event('input', {bubbles: true}));
  el.dispatchEvent(new Event('change', {bubbles: true}));
}
S.fill = function(u, p) {
  quiet = true;
  setTimeout(() => { quiet = false; }, 400);
  const pws = pwFields();
  const pw = (lastFocus && lastFocus.type === 'password') ? lastFocus : pws[0];
  const uf = pw ? userFor(pw) : (lastFocus && lastFocus.type !== 'password' ? lastFocus : userFor(null));
  if (uf && u) setVal(uf, u);
  if (pw) setVal(pw, p);
  sent = (u || '') + '\u0000' + p;
  lastUser = u || lastUser;
  if (pw) pw.focus();
};
})();)JS";
}  // namespace

PasswordManager* PasswordManager::instance() {
  static PasswordManager* self = new PasswordManager();
  return self;
}

PasswordManager* PasswordManager::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

PasswordManager::PasswordManager() {
  lockTimer_ = new QTimer(this);
  lockTimer_->setSingleShot(true);
  lockTimer_->setInterval(5 * 60 * 1000);
  connect(lockTimer_, &QTimer::timeout, this, &PasswordManager::lock);
  clipTimer_ = new QTimer(this);
  clipTimer_->setSingleShot(true);
  clipTimer_->setInterval(30 * 1000);
  connect(clipTimer_, &QTimer::timeout, this, [this] {
    // Стираем пароль из буфера, только если там всё ещё он.
    if (QGuiApplication::clipboard()->text() == clipValue_) QGuiApplication::clipboard()->clear();
    clipValue_.clear();
  });
}

void PasswordManager::open() {
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb";
  QDir().mkpath(dir);
  db_ = QSqlDatabase::addDatabase("QSQLITE", "crumb-passwords");
  db_.setDatabaseName(dir + "/passwords.db");
  if (!db_.open()) {
    qWarning("Passwords: не удалось открыть базу: %s", qUtf8Printable(db_.lastError().text()));
    return;
  }
  QSqlQuery q(db_);
  q.exec("CREATE TABLE IF NOT EXISTS logins (id INTEGER PRIMARY KEY, site TEXT NOT NULL, origin TEXT, "
         "username TEXT, password BLOB, created INTEGER, updated INTEGER, last_used INTEGER, times_used INTEGER DEFAULT 0)");
  q.exec("CREATE INDEX IF NOT EXISTS logins_site ON logins(site)");
}

void PasswordManager::bump() {
  ++revision_;
  emit changed();
}

int PasswordManager::count() const {
  QSqlQuery q(db_);
  return q.exec("SELECT COUNT(*) FROM logins") && q.next() ? q.value(0).toInt() : 0;
}

QString PasswordManager::siteOf(const QString& url) const { return siteFor(url); }

QByteArray PasswordManager::decrypt(const QByteArray& blob) const { return crypto::unprotect(blob); }

QString PasswordManager::captureJs() const { return QString::fromUtf8(kCaptureJs); }

QString PasswordManager::fillJs(int id) {
  QSqlQuery q(db_);
  q.prepare("SELECT username, password FROM logins WHERE id = ?");
  q.addBindValue(id);
  if (!q.exec() || !q.next()) return {};
  const QString user = q.value(0).toString();
  const QString pass = QString::fromUtf8(decrypt(q.value(1).toByteArray()));
  QSqlQuery u(db_);
  u.prepare("UPDATE logins SET last_used = ?, times_used = times_used + 1 WHERE id = ?");
  u.addBindValue(now());
  u.addBindValue(id);
  u.exec();
  return QStringLiteral("window.__crumbPw && window.__crumbPw.fill(%1, %2)").arg(jsString(user), jsString(pass));
}

QString PasswordManager::check(const QString& url, const QString& username, const QString& password) const {
  if (!SettingsStore::instance()->get("passwordsOffer").toBool()) return "off";
  const QString site = siteFor(url);
  if (site.isEmpty() || password.isEmpty()) return "off";
  if (SettingsStore::instance()->get("passwordsNever").toStringList().contains(site)) return "never";
  QSqlQuery q(db_);
  q.prepare("SELECT password FROM logins WHERE site = ? AND username = ?");
  q.addBindValue(site);
  q.addBindValue(username);
  if (q.exec() && q.next())
    return QString::fromUtf8(decrypt(q.value(0).toByteArray())) == password ? "same" : "update";
  return "new";
}

void PasswordManager::save(const QString& url, const QString& username, const QString& password) {
  const QString site = siteFor(url);
  if (site.isEmpty() || password.isEmpty()) return;
  const QByteArray enc = crypto::protect(password.toUtf8());
  QSqlQuery q(db_);
  q.prepare("UPDATE logins SET password = ?, updated = ?, origin = ? WHERE site = ? AND username = ?");
  q.addBindValue(enc);
  q.addBindValue(now());
  q.addBindValue(originFor(url));
  q.addBindValue(site);
  q.addBindValue(username);
  q.exec();
  if (q.numRowsAffected() <= 0) {
    QSqlQuery i(db_);
    i.prepare("INSERT INTO logins(site, origin, username, password, created, updated, last_used) VALUES(?, ?, ?, ?, ?, ?, ?)");
    i.addBindValue(site);
    i.addBindValue(originFor(url));
    i.addBindValue(username);
    i.addBindValue(enc);
    i.addBindValue(now());
    i.addBindValue(now());
    i.addBindValue(now());
    if (!i.exec()) qWarning("Passwords: ошибка записи: %s", qUtf8Printable(i.lastError().text()));
  }
  bump();
}

void PasswordManager::neverFor(const QString& url) {
  const QString site = siteFor(url);
  QStringList list = SettingsStore::instance()->get("passwordsNever").toStringList();
  if (site.isEmpty() || list.contains(site)) return;
  list.append(site);
  SettingsStore::instance()->set("passwordsNever", list);
}

void PasswordManager::allowSite(const QString& site) {
  QStringList list = SettingsStore::instance()->get("passwordsNever").toStringList();
  list.removeAll(site);
  SettingsStore::instance()->set("passwordsNever", list);
}

QVariantList PasswordManager::forSite(const QString& url) const {
  QVariantList out;
  const QString site = siteFor(url);
  if (site.isEmpty()) return out;
  QSqlQuery q(db_);
  q.prepare("SELECT id, username FROM logins WHERE site = ? ORDER BY last_used DESC");
  q.addBindValue(site);
  if (q.exec())
    while (q.next()) out.append(QVariantMap{{"id", q.value(0).toInt()}, {"username", q.value(1).toString()}, {"site", site}});
  return out;
}

QVariantList PasswordManager::list(const QString& filter) const {
  QVariantList out;
  const QString f = filter.trimmed().toLower();
  QSqlQuery q(db_);
  if (!q.exec("SELECT id, site, origin, username, updated, times_used FROM logins ORDER BY site, username")) return out;
  static const QLocale ru(QLocale::Russian);
  while (q.next()) {
    const QString site = q.value(1).toString(), user = q.value(3).toString();
    if (!f.isEmpty() && !site.contains(f) && !user.toLower().contains(f)) continue;
    const QDateTime upd = QDateTime::fromMSecsSinceEpoch(q.value(4).toLongLong());
    out.append(QVariantMap{{"id", q.value(0).toInt()},
                           {"site", site},
                           {"origin", q.value(2).toString()},
                           {"username", user},
                           {"updated", ru.toString(upd.date(), QStringLiteral("d MMMM yyyy"))},
                           {"used", q.value(5).toInt()},
                           {"icon", HistoryManager::instance()->iconFor(q.value(2).toString())}});
  }
  return out;
}

// ------------------------------------------------------ проверка личности

void PasswordManager::unlock(QWindow* window) {
  if (unlocked_) {
    lockTimer_->start();
    return;
  }
  if (verifying_) return;
  verifying_ = true;
  emit unlockedChanged();
  void* hwnd = window ? reinterpret_cast<void*>(window->winId()) : nullptr;
  QPointer<PasswordManager> self(this);
  std::thread([self, hwnd] {
    const bool ok = winverify::verify(hwnd, L"Crumb хочет показать сохранённые пароли. Подтвердите, что это вы.");
    QMetaObject::invokeMethod(qApp, [self, ok] {
      if (!self) return;
      self->verifying_ = false;
      self->unlocked_ = ok;
      if (ok) self->lockTimer_->start();
      emit self->unlockedChanged();
      if (!ok) emit self->unlockFailed();
    }, Qt::QueuedConnection);
  }).detach();
}

void PasswordManager::lock() {
  lockTimer_->stop();
  if (!unlocked_) return;
  unlocked_ = false;
  emit unlockedChanged();
}

QString PasswordManager::password(int id) const {
  if (!unlocked_) return {};
  QSqlQuery q(db_);
  q.prepare("SELECT password FROM logins WHERE id = ?");
  q.addBindValue(id);
  return q.exec() && q.next() ? QString::fromUtf8(decrypt(q.value(0).toByteArray())) : QString();
}

bool PasswordManager::copyPassword(int id) {
  const QString p = password(id);
  if (p.isEmpty()) return false;
  QGuiApplication::clipboard()->setText(p);
  clipValue_ = p;
  clipTimer_->start();
  lockTimer_->start();
  return true;
}

void PasswordManager::copyText(const QString& text) { QGuiApplication::clipboard()->setText(text); }

bool PasswordManager::update(int id, const QString& username, const QString& password) {
  if (!unlocked_ || password.isEmpty()) return false;
  QSqlQuery q(db_);
  q.prepare("UPDATE logins SET username = ?, password = ?, updated = ? WHERE id = ?");
  q.addBindValue(username);
  q.addBindValue(crypto::protect(password.toUtf8()));
  q.addBindValue(now());
  q.addBindValue(id);
  const bool ok = q.exec();
  bump();
  return ok;
}

void PasswordManager::add(const QString& url, const QString& username, const QString& password) {
  QString u = url.trimmed();
  if (!u.contains("://")) u = "https://" + u;
  save(u, username, password);
}

void PasswordManager::remove(int id) {
  QSqlQuery q(db_);
  q.prepare("DELETE FROM logins WHERE id = ?");
  q.addBindValue(id);
  q.exec();
  bump();
}

QString PasswordManager::generate(int length) const {
  // Без похожих символов (l/1/I, O/0), обязательно все четыре группы.
  static const QString groups[] = {"abcdefghijkmnpqrstuvwxyz", "ABCDEFGHJKLMNPQRSTUVWXYZ", "23456789", "-_!@#$%*?"};
  auto* rng = QRandomGenerator::system();
  QString out;
  for (const QString& g : groups) out += g.at(rng->bounded(int(g.size())));
  const QString all = groups[0] + groups[0] + groups[1] + groups[2] + groups[3];
  while (out.size() < std::max(length, 8)) out += all.at(rng->bounded(int(all.size())));
  for (int i = int(out.size()) - 1; i > 0; --i) std::swap(out[i], out[rng->bounded(i + 1)]);
  return out;
}

int PasswordManager::strength(const QString& p) const {
  int classes = 0;
  bool lower = false, upper = false, digit = false, other = false;
  for (const QChar c : p) {
    if (c.isLower()) lower = true;
    else if (c.isUpper()) upper = true;
    else if (c.isDigit()) digit = true;
    else other = true;
  }
  classes = int(lower) + int(upper) + int(digit) + int(other);
  if (p.size() < 8 || classes <= 1) return 0;
  if (p.size() < 10 || classes == 2) return 1;
  if (p.size() < 14 || classes == 3) return 2;
  return 3;
}

}  // namespace crumb
