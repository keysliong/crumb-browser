#include "rewind/RewindManager.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QJsonObject>
#include <QLocale>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QTimer>
#include <QUrl>
#include <vector>

#include "cef/CefRuntime.h"
#include "cef/CefTask.h"
#include "include/cef_client.h"
#include "include/cef_values.h"
#include "storage/SettingsStore.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace crumb {

namespace {

QString qs(const CefString& s) { return QString::fromStdWString(s.ToWString()); }
CefString cs(const QString& s) { return CefString(s.toStdWString()); }

// Скрипт снимка. TOKEN подставляется. Страницы с полем пароля не сохраняются.
const char* kCaptureJs = R"JS(
(function (tok) {
  const send = (s) => console.log(s);
  try {
    if (document.querySelector('input[type=password]')) { send('CRUMB_RW:' + tok + ':skip'); return; }
    const text = (document.body ? document.body.innerText : '').slice(0, 400000);
    if (text.trim().length < 20) { send('CRUMB_RW:' + tok + ':skip'); return; }
    const doc = document.documentElement.cloneNode(true);
    doc.querySelectorAll('script,noscript,iframe,object,embed,link[rel=preload],link[rel=modulepreload]')
       .forEach(e => e.remove());
    doc.querySelectorAll('*').forEach(e => {
      for (const a of [...e.attributes]) if (a.name.startsWith('on')) e.removeAttribute(a.name);
    });
    // стили — внутрь снимка, чтобы прошлая версия выглядела как тогда
    let css = '';
    for (const sh of document.styleSheets) {
      try { for (const r of sh.cssRules) css += r.cssText + '\n'; } catch (e) {}
      if (css.length > 900000) break;
    }
    const head = doc.querySelector('head') || doc;
    const base = document.createElement('base'); base.href = location.href; head.prepend(base);
    const st = document.createElement('style'); st.textContent = css; head.appendChild(st);
    let html = '<!doctype html>' + doc.outerHTML;
    if (html.length > 2000000) html = html.slice(0, 2000000);
    const payload = JSON.stringify({ url: location.href, title: document.title, text, html });
    const size = 300000, n = Math.ceil(payload.length / size);
    for (let i = 0; i < n; i++) send('CRUMB_RW:' + tok + ':' + i + ':' + n + ':' + payload.substr(i * size, size));
  } catch (e) { send('CRUMB_RW:' + tok + ':skip'); }
})('TOKEN');
)JS";

// Выбор фрагмента страницы для подписки.
const char* kPickerJs = R"JS(
(function () {
  if (window.__crumbPicker) return; window.__crumbPicker = true;
  const box = document.createElement('div');
  box.style.cssText = 'position:fixed;pointer-events:none;z-index:2147483647;border:3px solid #8C6CF0;' +
    'background:rgba(140,108,240,.15);border-radius:12px;transition:all .08s;';
  const tip = document.createElement('div');
  tip.textContent = 'Crumb Rewind: нажмите на фрагмент, за которым следить · Esc — отмена';
  tip.style.cssText = 'position:fixed;top:12px;left:50%;transform:translateX(-50%);z-index:2147483647;' +
    'background:#2A1460;color:#fff;font:14px Segoe UI,sans-serif;padding:10px 18px;border-radius:20px;' +
    'box-shadow:0 6px 24px rgba(0,0,0,.35)';
  document.documentElement.append(box, tip);
  let cur = null;
  const sel = (el) => {
    if (el.id && document.querySelectorAll('#' + CSS.escape(el.id)).length === 1) return '#' + CSS.escape(el.id);
    const parts = [];
    for (let e = el; e && e.nodeType === 1 && e !== document.body; e = e.parentElement) {
      if (e.id && document.querySelectorAll('#' + CSS.escape(e.id)).length === 1) { parts.unshift('#' + CSS.escape(e.id)); break; }
      let i = 1; for (let s = e.previousElementSibling; s; s = s.previousElementSibling) if (s.tagName === e.tagName) i++;
      parts.unshift(e.tagName.toLowerCase() + ':nth-of-type(' + i + ')');
    }
    return parts.join(' > ');
  };
  const move = (ev) => {
    cur = ev.target; const r = cur.getBoundingClientRect();
    Object.assign(box.style, { left: r.left - 3 + 'px', top: r.top - 3 + 'px', width: r.width + 'px', height: r.height + 'px' });
  };
  const done = () => {
    document.removeEventListener('mousemove', move, true);
    document.removeEventListener('click', click, true);
    document.removeEventListener('keydown', key, true);
    box.remove(); tip.remove(); window.__crumbPicker = false;
  };
  const click = (ev) => {
    ev.preventDefault(); ev.stopPropagation();
    if (!cur) return;
    const text = (cur.innerText || '').trim().slice(0, 4000);
    console.log('CRUMB_RW_PICK:' + JSON.stringify({ url: location.href, title: document.title, selector: sel(cur), text }));
    done();
  };
  const key = (ev) => { if (ev.key === 'Escape') done(); };
  document.addEventListener('mousemove', move, true);
  document.addEventListener('click', click, true);
  document.addEventListener('keydown', key, true);
})();
)JS";

// Наблюдатель DevTools: результаты Page.captureScreenshot.
class ShotObserver final : public CefDevToolsMessageObserver {
 public:
  void OnDevToolsMethodResult(CefRefPtr<CefBrowser>, int message_id, bool success, const void* result,
                              size_t size) override {
    RewindManager::instance()->onScreenshot(message_id, success,
                                            QByteArray(static_cast<const char*>(result), qsizetype(size)));
  }

 private:
  IMPLEMENT_REFCOUNTING(ShotObserver);
};

// Скрытый браузер для фоновой проверки подписок.
class WatchClient final : public CefClient, public CefLifeSpanHandler, public CefLoadHandler,
                          public CefDisplayHandler {
 public:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  void OnAfterCreated(CefRefPtr<CefBrowser> b) override {
    CefRuntime::instance()->browserCreated();
#if defined(_WIN32)
    ::ShowWindow(b->GetHost()->GetWindowHandle(), SW_HIDE);
#endif
    RewindManager::instance()->onWatchBrowserCreated(b);
  }
  void OnBeforeClose(CefRefPtr<CefBrowser>) override {
    RewindManager::instance()->onWatchBrowserClosed();
    CefRuntime::instance()->browserClosed();
  }
  void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int) override {
    if (frame->IsMain()) RewindManager::instance()->onWatchPageLoaded();
  }
  bool OnConsoleMessage(CefRefPtr<CefBrowser> b, cef_log_severity_t, const CefString& message, const CefString&,
                        int) override {
    return RewindManager::instance()->handleConsole(b, qs(message));
  }

 private:
  IMPLEMENT_REFCOUNTING(WatchClient);
};

QString tempDir() {
  const QString d = QDir::tempPath() + "/crumb-rewind";
  QDir().mkpath(d);
  return d;
}

// Глоб «*bank*» → регулярное выражение по «хост/путь».
QRegularExpression globToRegex(const QString& glob) {
  QString re = QRegularExpression::escape(glob.trimmed());
  re.replace("\\*", ".*");
  return QRegularExpression("^(.*\\.)?" + re + "(/.*)?$", QRegularExpression::CaseInsensitiveOption);
}

}  // namespace

// ------------------------------------------------------------------------

RewindManager* RewindManager::instance() {
  static RewindManager* self = new RewindManager();
  return self;
}

RewindManager* RewindManager::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

RewindManager::RewindManager() {
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb";
  QDir().mkpath(dir);
  store_.open(dir + "/rewind.db");
  loadSettings();
  connect(SettingsStore::instance(), &SettingsStore::settingChanged, this, [this](const QString& key) {
    if (key.startsWith("rewind")) loadSettings();
  });
  watchTimer_ = new QTimer(this);
  connect(watchTimer_, &QTimer::timeout, this, &RewindManager::nextWatch);
  watchTimer_->start(60 * 1000);
  watchTimeout_ = new QTimer(this);
  watchTimeout_->setSingleShot(true);
  connect(watchTimeout_, &QTimer::timeout, this, [this] { watchBusy_ = false; });
  connect(CefRuntime::instance(), &CefRuntime::closeAllBrowsers, this, [this] {
    if (watchBrowser_) watchBrowser_->GetHost()->CloseBrowser(true);
  });
  QTimer::singleShot(5000, this, [this] {   // уборка старого при запуске
    auto* s = SettingsStore::instance();
    store_.cleanup(s->get("rewindDays").toInt(), qint64(s->get("rewindLimitMB").toInt()) * 1024 * 1024);
    refreshStats();
  });
  watches_ = store_.watches();
  refreshStats();
}

void RewindManager::loadSettings() {
  auto* s = SettingsStore::instance();
  std::lock_guard lock(mutex_);
  enabled_ = s->get("rewindEnabled").toBool();
  blacklist_.clear();
  for (const QString& line : s->get("rewindBlacklist").toString().split('\n', Qt::SkipEmptyParts))
    if (!line.trimmed().isEmpty()) blacklist_ << globToRegex(line);
}

bool RewindManager::eligible(const QString& url) const {
  const QUrl u(url);
  if (u.scheme() != "https" && u.scheme() != "http") return false;
  const QString hostPath = u.host() + u.path();
  std::lock_guard lock(mutex_);
  if (!enabled_) return false;
  for (const auto& re : blacklist_)
    if (re.match(u.host()).hasMatch() || re.match(hostPath).hasMatch()) return false;
  return true;
}

void RewindManager::refreshStats() {
  count_ = store_.count();
  sizeMB_ = store_.totalBytes() / 1048576.0;
  emit statsChanged();
}

int RewindManager::unseenWatches() const {
  int n = 0;
  for (const QVariant& w : watches_) n += w.toMap().value("unseen").toBool() ? 1 : 0;
  return n;
}

QString RewindManager::formatDate(double ms) const {
  const QDateTime d = QDateTime::fromMSecsSinceEpoch(qint64(ms));
  const QDate today = QDate::currentDate();
  const QLocale ru(QLocale::Russian);
  if (d.date() == today) return QStringLiteral("сегодня, ") + d.toString("HH:mm");
  if (d.date() == today.addDays(-1)) return QStringLiteral("вчера, ") + d.toString("HH:mm");
  return ru.toString(d, d.date().year() == today.year() ? "d MMMM, HH:mm" : "d MMMM yyyy, HH:mm");
}

// ------------------------------------------------------------- снимки (CEF)

void RewindManager::onPageLoaded(CefRefPtr<CefBrowser> browser) {
  const QString url = qs(browser->GetMainFrame()->GetURL());
  if (!eligible(url)) return;
  const QString token = QString::number(++tokenCounter_);
  {
    std::lock_guard lock(mutex_);
    pending_.insert(token, Pending{browser, {}, 0});
  }
  const QString js = QString::fromUtf8(kCaptureJs).replace("TOKEN", token);
  // Даём странице дорисоваться (ленивая подгрузка, шрифты).
  CefPostDelayedTask(TID_UI, new FunctionTask([browser, js] {
    browser->GetMainFrame()->ExecuteJavaScript(cs(js), "crumb://rewind", 0);
  }), 1800);
}

bool RewindManager::handleConsole(CefRefPtr<CefBrowser> browser, const QString& m) {
  if (m.startsWith(QLatin1String("CRUMB_RW_PICK:"))) {
    const QByteArray json = m.mid(14).toUtf8();
    QMetaObject::invokeMethod(this, [this, json] { addWatchFromPick(json); }, Qt::QueuedConnection);
    return true;
  }
  if (m.startsWith(QLatin1String("CRUMB_RW_W:"))) {
    const int c = m.indexOf(':', 11);
    const qint64 id = m.mid(11, c - 11).toLongLong();
    const QJsonValue v = QJsonDocument::fromJson("[" + m.mid(c + 1).toUtf8() + "]").array().at(0);
    QMetaObject::invokeMethod(this, [this, id, v] { watchResult(id, v); }, Qt::QueuedConnection);
    return true;
  }
  if (!m.startsWith(QLatin1String("CRUMB_RW:"))) return false;

  // CRUMB_RW:<tok>:skip  |  CRUMB_RW:<tok>:<i>:<n>:<часть>
  const int p1 = m.indexOf(':', 9);
  const QString token = m.mid(9, p1 - 9);
  if (m.mid(p1 + 1) == QLatin1String("skip")) {
    std::lock_guard lock(mutex_);
    pending_.remove(token);
    return true;
  }
  const int p2 = m.indexOf(':', p1 + 1), p3 = m.indexOf(':', p2 + 1);
  const int i = m.mid(p1 + 1, p2 - p1 - 1).toInt(), n = m.mid(p2 + 1, p3 - p2 - 1).toInt();
  QByteArray payload;
  CefRefPtr<CefBrowser> b;
  {
    std::lock_guard lock(mutex_);
    auto it = pending_.find(token);
    if (it == pending_.end() || n <= 0 || i < 0 || i >= n) return true;
    if (it->parts.size() != n) it->parts = QStringList(QList<QString>(n));
    if (it->parts[i].isEmpty()) ++it->got;
    it->parts[i] = m.mid(p3 + 1);
    if (it->got < n) return true;
    payload = it->parts.join(QString()).toUtf8();
    b = it->browser;
    pending_.erase(it);
  }
  QMetaObject::invokeMethod(this, [this, token, payload] { ingest(token, payload); }, Qt::QueuedConnection);

  // Мини-скриншот видимой части страницы — снимком окна вкладки (PrintWindow).
  // Раньше брали через DevTools (Page.captureScreenshot), но подключённый к вкладке
  // отладчик выдаёт «управляемый браузер»: капчи Cloudflare Turnstile отказывали
  // (client-error). Скрытая вкладка снимок не даёт — тогда снимок без картинки.
  if (b) {
    const quintptr hwnd = reinterpret_cast<quintptr>(b->GetHost()->GetWindowHandle());
    QMetaObject::invokeMethod(this, [this, token, hwnd] { captureWindow(token, hwnd); }, Qt::QueuedConnection);
  }
  return true;
}

void RewindManager::captureWindow(const QString& token, quintptr hwndValue) {
#if defined(_WIN32)
  HWND hwnd = reinterpret_cast<HWND>(hwndValue);
  if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(GetAncestor(hwnd, GA_ROOT))) return;
  RECT r{};
  GetClientRect(hwnd, &r);
  const int w = r.right - r.left, h = r.bottom - r.top;
  if (w < 50 || h < 50) return;
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
  HGDIOBJ old = SelectObject(mem, bmp);
  const BOOL ok = PrintWindow(hwnd, mem, 2 /* PW_RENDERFULLCONTENT */);
  SelectObject(mem, old);
  QImage img = ok ? QImage::fromHBITMAP(bmp) : QImage();
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  if (img.isNull()) return;
  img = img.scaledToWidth(360, Qt::SmoothTransformation);
  QByteArray jpeg;
  QBuffer buf(&jpeg);
  buf.open(QIODevice::WriteOnly);
  img.save(&buf, "JPEG", 72);
  thumbArrived(token, jpeg);
#else
  Q_UNUSED(token);
  Q_UNUSED(hwndValue);
#endif
}

void RewindManager::onScreenshot(int messageId, bool ok, const QByteArray& json) {
  QString token;
  {
    std::lock_guard lock(mutex_);
    token = shotToken_.take(messageId);
  }
  if (token.isEmpty() || !ok) return;
  const QByteArray b64 = QJsonDocument::fromJson(json).object().value("data").toString().toLatin1();
  QImage img;
  if (!img.loadFromData(QByteArray::fromBase64(b64))) return;
  img = img.scaledToWidth(360, Qt::SmoothTransformation);
  QByteArray jpeg;
  QBuffer buf(&jpeg);
  buf.open(QIODevice::WriteOnly);
  img.save(&buf, "JPEG", 72);
  QMetaObject::invokeMethod(this, [this, token, jpeg] { thumbArrived(token, jpeg); }, Qt::QueuedConnection);
}

// ------------------------------------------------------------- снимки (Qt)

void RewindManager::ingest(const QString& token, const QByteArray& payload) {
  const QJsonObject o = QJsonDocument::fromJson(payload).object();
  const QString url = o.value("url").toString();
  if (url.isEmpty() || !eligible(url)) return;
  bool dup = false;
  const qint64 id = store_.add(url, o.value("title").toString(), o.value("text").toString(),
                               o.value("html").toString(), &dup);
  if (!id) return;
  tokenId_.insert(token, id);
  if (pendingThumb_.contains(token)) store_.setThumb(id, pendingThumb_.take(token));
  if (!dup && ++sinceCleanup_ >= 25) {
    sinceCleanup_ = 0;
    auto* s = SettingsStore::instance();
    store_.cleanup(s->get("rewindDays").toInt(), qint64(s->get("rewindLimitMB").toInt()) * 1024 * 1024);
  }
  ++revision_;
  refreshStats();
  emit snapshotsChanged(url);
}

void RewindManager::thumbArrived(const QString& token, const QByteArray& jpeg) {
  if (tokenId_.contains(token)) {
    store_.setThumb(tokenId_.take(token), jpeg);
    ++revision_;
    emit snapshotsChanged(QString());
  } else {
    pendingThumb_.insert(token, jpeg);
  }
}

QVariantList RewindManager::snapshotsFor(const QString& url) {
  QVariantList list = store_.listForUrl(url);
  for (QVariant& v : list) {
    QVariantMap m = v.toMap();
    m.insert("thumb", m.value("hasThumb").toBool()
                          ? QStringLiteral("image://rewind/%1?r=%2").arg(m.value("id").toLongLong()).arg(revision_)
                          : QString());
    m.insert("date", formatDate(m.value("ts").toDouble()));
    v = m;
  }
  return list;
}

QString RewindManager::pageUrl(int id) {
  Snapshot s;
  if (!store_.load(id, &s)) return {};
  const QString path = tempDir() + QStringLiteral("/snapshot-%1.html").arg(id);
  QFile f(path);
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(s.html.toUtf8());
  return QUrl::fromLocalFile(path).toString();
}

// ------------------------------------------------------- «что изменилось»

namespace {

QStringList linesOf(const QString& text) {
  QStringList out;
  for (const QString& l : text.split('\n')) {
    const QString t = l.simplified();
    if (!t.isEmpty()) out << t;
  }
  return out;
}

struct Op { char kind; QString line; };   // '=' без изменений, '+' новое, '-' удалённое

QList<Op> diffLines(const QStringList& a, const QStringList& b) {
  QList<Op> ops;
  const int n = int(a.size()), m = int(b.size());
  if (qint64(n) * m > 16'000'000) {   // очень длинные страницы — по множествам строк
    const QSet<QString> sa(a.begin(), a.end()), sb(b.begin(), b.end());
    for (const QString& l : b) ops << Op{sa.contains(l) ? '=' : '+', l};
    for (const QString& l : a) if (!sb.contains(l)) ops << Op{'-', l};
    return ops;
  }
  // Наибольшая общая подпоследовательность строк
  std::vector<std::vector<quint16>> L(n + 1, std::vector<quint16>(m + 1, 0));
  for (int i = n - 1; i >= 0; --i)
    for (int j = m - 1; j >= 0; --j)
      L[i][j] = a[i] == b[j] ? L[i + 1][j + 1] + 1 : std::max(L[i + 1][j], L[i][j + 1]);
  int i = 0, j = 0;
  while (i < n && j < m) {
    if (a[i] == b[j]) { ops << Op{'=', a[i]}; ++i; ++j; }
    else if (L[i + 1][j] >= L[i][j + 1]) ops << Op{'-', a[i++]};
    else ops << Op{'+', b[j++]};
  }
  while (i < n) ops << Op{'-', a[i++]};
  while (j < m) ops << Op{'+', b[j++]};
  return ops;
}

}  // namespace

QString RewindManager::diffUrl(int oldId, int newId) {
  Snapshot older, newer;
  if (!store_.load(oldId, &older)) return {};
  qint64 nid = newId > 0 ? newId : store_.latestFor(older.url);
  if (nid == oldId) {   // выбран последний снимок — сравним с предыдущим
    const qint64 prev = store_.previousOf(oldId);
    if (prev) { nid = oldId; store_.load(prev, &older); }
  }
  if (!store_.load(nid, &newer)) newer = older;

  const QList<Op> ops = diffLines(linesOf(older.text), linesOf(newer.text));
  int added = 0, removed = 0;
  for (const Op& o : ops) { added += o.kind == '+'; removed += o.kind == '-'; }

  QString body;
  int run = 0;
  auto flushRun = [&](int count) {
    if (count > 0) body += QStringLiteral("<div class=skip>… %1 строк без изменений</div>").arg(count);
  };
  // Показываем изменения с контекстом в 2 строки; длинные одинаковые участки сворачиваем.
  for (int k = 0; k < ops.size(); ++k) {
    const Op& o = ops[k];
    if (o.kind == '=') {
      bool nearChange = false;
      for (int d = -2; d <= 2 && !nearChange; ++d)
        if (k + d >= 0 && k + d < ops.size() && ops[k + d].kind != '=') nearChange = true;
      if (!nearChange) { ++run; continue; }
      flushRun(run); run = 0;
      body += "<p>" + o.line.toHtmlEscaped() + "</p>";
    } else {
      flushRun(run); run = 0;
      body += (o.kind == '+' ? "<p class=add>" : "<p class=del>") + o.line.toHtmlEscaped() + "</p>";
    }
  }
  flushRun(run);
  if (added == 0 && removed == 0) body = "<div class=same>Текст страницы не изменился.</div>";

  const QString html = QStringLiteral(R"(<!doctype html><html lang=ru><meta charset=utf-8>
<title>Что изменилось — %1</title>
<style>
:root{--bg:#fdf7ff;--fg:#1d1b20;--muted:#625b71;--card:#f3edf7;--add:#c4f2cf;--addfg:#0b4d22;--del:#ffdad6;--delfg:#8c1d18}
@media (prefers-color-scheme:dark){:root{--bg:#141218;--fg:#e6e0e9;--muted:#cac4d0;--card:#211f26;--add:#0f3d22;--addfg:#a6f0bb;--del:#4a1714;--delfg:#ffb4ab}}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.55 'Segoe UI',Roboto,sans-serif}
header{position:sticky;top:0;background:var(--bg);padding:20px 32px 12px;border-bottom:1px solid var(--card)}
h1{font-size:22px;margin:0 0 4px}.meta{color:var(--muted);font-size:13px}
.legend span{display:inline-block;margin:8px 10px 0 0;padding:3px 12px;border-radius:12px;font-size:13px}
main{max-width:860px;margin:0 auto;padding:18px 32px 60px}
p{margin:4px 0;padding:6px 12px;border-radius:10px}
.add{background:var(--add);color:var(--addfg)}
.del{background:var(--del);color:var(--delfg);opacity:.6;text-decoration:line-through}
.skip{color:var(--muted);font-size:13px;padding:6px 12px;margin:6px 0;border-left:3px solid var(--card)}
.same{padding:40px;text-align:center;color:var(--muted)}
</style>
<header><h1>Что изменилось</h1>
<div class=meta>%2<br>было: %3 &nbsp;→&nbsp; стало: %4</div>
<div class=legend><span class=add>+ %5 новых строк</span><span class=del>− %6 удалено</span></div></header>
<main>%7</main></html>)")
                           .arg(newer.title.toHtmlEscaped(), newer.url.toHtmlEscaped(), formatDate(older.ts),
                                formatDate(newer.ts))
                           .arg(added).arg(removed).arg(body);
  const QString path = tempDir() + QStringLiteral("/diff-%1-%2.html").arg(older.id).arg(newer.id);
  QFile f(path);
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(html.toUtf8());
  return QUrl::fromLocalFile(path).toString();
}

QVariantList RewindManager::search(const QString& query) {
  QVariantList list = store_.search(query);
  for (QVariant& v : list) {
    QVariantMap m = v.toMap();
    m.insert("thumb", QStringLiteral("image://rewind/%1").arg(m.value("id").toLongLong()));
    m.insert("date", formatDate(m.value("ts").toDouble()));
    v = m;
  }
  return list;
}

void RewindManager::clearAll() {
  store_.clear();
  ++revision_;
  refreshStats();
  emit snapshotsChanged(QString());
}

// ------------------------------------------------------------- подписки

QString RewindManager::pickerJs() const { return QString::fromUtf8(kPickerJs); }

void RewindManager::addWatchFromPick(const QByteArray& json) {
  const QJsonObject o = QJsonDocument::fromJson(json).object();
  const QString text = o.value("text").toString();
  QString label = text.simplified().left(60);
  if (label.isEmpty()) label = o.value("title").toString();
  store_.addWatch(o.value("url").toString(), o.value("selector").toString(), label, text);
  watches_ = store_.watches();
  emit watchesChanged();
  emit watchAdded(label);
}

void RewindManager::removeWatch(int id) {
  store_.removeWatch(id);
  watches_ = store_.watches();
  emit watchesChanged();
}

void RewindManager::markWatchesSeen() {
  store_.markWatchesSeen();
  watches_ = store_.watches();
  emit watchesChanged();
}

void RewindManager::start(QWindow* window) { window_ = window; }

void RewindManager::importWatches(const QVariantList& list) {
  QSet<QString> have;
  for (const QVariant& v : store_.watches())
    have.insert(v.toMap().value("url").toString() + QStringLiteral(" | ") + v.toMap().value("selector").toString());
  bool added = false;
  for (const QVariant& v : list) {
    const QVariantMap w = v.toMap();
    const QString k = w.value("url").toString() + QStringLiteral(" | ") + w.value("selector").toString();
    if (w.value("url").toString().isEmpty() || have.contains(k)) continue;
    store_.addWatch(w.value("url").toString(), w.value("selector").toString(), w.value("label").toString(),
                    w.value("text").toString());
    have.insert(k);
    added = true;
  }
  if (added) {
    watches_ = store_.watches();
    emit watchesChanged();
  }
}

void RewindManager::checkWatchesNow() {
  watchForce_ = true;
  nextWatch();
}

void RewindManager::nextWatch() {
  if (watchBusy_ || !window_ || !CefRuntime::instance()->ready()) return;
  const qint64 due = QDateTime::currentMSecsSinceEpoch() - 30 * 60 * 1000;   // каждые 30 минут
  for (const QVariant& v : store_.watches()) {
    const QVariantMap w = v.toMap();
    if (!watchForce_ && w.value("lastCheck").toLongLong() > due) continue;
    if (watchForce_ && w.value("lastCheck").toLongLong() > QDateTime::currentMSecsSinceEpoch() - 20000) continue;
    watchBusy_ = true;
    currentWatch_ = w.value("id").toLongLong();
    currentSelector_ = w.value("selector").toString();
    currentWatchUrl_ = w.value("url").toString();
    watchTimeout_->start(45000);
    if (watchBrowser_) {
      watchBrowser_->GetMainFrame()->LoadURL(cs(currentWatchUrl_));
    } else if (!watchCreating_) {
      watchCreating_ = true;
      CefWindowInfo info;
#if defined(_WIN32)
      info.SetAsChild(reinterpret_cast<CefWindowHandle>(window_->winId()), CefRect(0, 0, 1280, 800));
#endif
      info.runtime_style = CEF_RUNTIME_STYLE_CHROME;
      CefBrowserSettings settings;
      CefBrowserHost::CreateBrowser(info, new WatchClient(), cs(currentWatchUrl_), settings, nullptr, nullptr);
    }
    return;
  }
  watchForce_ = false;
}

void RewindManager::onWatchBrowserCreated(CefRefPtr<CefBrowser> b) {
  QMetaObject::invokeMethod(this, [this, b] { watchBrowser_ = b; watchCreating_ = false; }, Qt::QueuedConnection);
}

void RewindManager::onWatchBrowserClosed() {
  QMetaObject::invokeMethod(this, [this] { watchBrowser_ = nullptr; watchBusy_ = false; }, Qt::QueuedConnection);
}

void RewindManager::onWatchPageLoaded() {
  QMetaObject::invokeMethod(this, [this] {
    if (!watchBusy_ || !watchBrowser_) return;
    const QString sel = QString::fromUtf8(QJsonDocument(QJsonArray{currentSelector_}).toJson(QJsonDocument::Compact));
    const QString js = QStringLiteral(
        "setTimeout(function(){var s=%1[0];var e=null;try{e=document.querySelector(s)}catch(x){}"
        "console.log('CRUMB_RW_W:%2:'+JSON.stringify(e?(e.innerText||'').trim().slice(0,4000):null))},2500)")
                           .arg(sel).arg(currentWatch_);
    CefRefPtr<CefBrowser> b = watchBrowser_;
    postToCefUi([b, js] { b->GetMainFrame()->ExecuteJavaScript(cs(js), "crumb://rewind-watch", 0); });
  }, Qt::QueuedConnection);
}

void RewindManager::watchResult(qint64 id, const QJsonValue& v) {
  watchBusy_ = false;
  watchTimeout_->stop();
  QVariantMap w;
  for (const QVariant& x : store_.watches())
    if (x.toMap().value("id").toLongLong() == id) w = x.toMap();
  if (w.isEmpty()) return;
  const QString oldText = w.value("text").toString();
  if (v.isNull()) {   // фрагмент не найден (сайт поменял вёрстку) — просто отметим проверку
    store_.updateWatch(id, oldText, false);
  } else {
    const QString newText = v.toString();
    const bool changed = newText.simplified() != oldText.simplified();
    store_.updateWatch(id, newText, changed);
    if (changed) {
      emit watchTriggered(w.value("label").toString(), oldText, newText, w.value("url").toString());
      notify(QStringLiteral("Изменилось: %1").arg(QUrl(w.value("url").toString()).host()),
             QStringLiteral("Было: %1\nСтало: %2").arg(oldText.simplified().left(90), newText.simplified().left(90)),
             w.value("url").toString());
    }
  }
  watches_ = store_.watches();
  emit watchesChanged();
  QTimer::singleShot(1500, this, &RewindManager::nextWatch);   // следующая (если пора)
}

#if defined(_WIN32)
namespace {
constexpr UINT kTrayMsg = WM_APP + 0x52;   // сообщения от значка в трее
// Клик по уведомлению приходит окну Crumb — ловим его фильтром.
class TrayFilter final : public QAbstractNativeEventFilter {
 public:
  explicit TrayFilter(std::function<void()> onClick) : onClick_(std::move(onClick)) {}
  bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override {
    const MSG* m = static_cast<const MSG*>(message);
    if (m->message == kTrayMsg && (LOWORD(m->lParam) == NIN_BALLOONUSERCLICK || LOWORD(m->lParam) == WM_LBUTTONUP)) {
      onClick_();
      return true;
    }
    return false;
  }

 private:
  std::function<void()> onClick_;
};
}  // namespace
#endif

// Уведомление Windows (на Windows 10/11 показывается как всплывающий тост).
void RewindManager::notify(const QString& title, const QString& body, const QString& url) {
#if defined(_WIN32)
  if (!window_) return;
  trayUrl_ = url;
  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = reinterpret_cast<HWND>(window_->winId());
  nid.uID = 0xC0;
  if (!trayAdded_) {
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = kTrayMsg;
    nid.hIcon = static_cast<HICON>(::LoadImageW(::GetModuleHandleW(nullptr), L"IDI_ICON1", IMAGE_ICON,
                                                ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0));
    wcscpy_s(nid.szTip, L"Crumb Rewind");
    if (!::Shell_NotifyIconW(NIM_ADD, &nid)) return;
    trayAdded_ = true;
    QCoreApplication::instance()->installNativeEventFilter(new TrayFilter([this] {
      if (!trayUrl_.isEmpty()) CefRuntime::instance()->openUrlRequest(trayUrl_);
    }));
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
      NOTIFYICONDATAW d{};
      d.cbSize = sizeof(d);
      d.hWnd = reinterpret_cast<HWND>(window_->winId());
      d.uID = 0xC0;
      ::Shell_NotifyIconW(NIM_DELETE, &d);
    });
  }
  nid.uFlags = NIF_INFO;
  nid.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
  wcsncpy_s(nid.szInfoTitle, reinterpret_cast<const wchar_t*>(title.left(63).utf16()), _TRUNCATE);
  wcsncpy_s(nid.szInfo, reinterpret_cast<const wchar_t*>(body.left(255).utf16()), _TRUNCATE);
  ::Shell_NotifyIconW(NIM_MODIFY, &nid);
#else
  Q_UNUSED(title) Q_UNUSED(body) Q_UNUSED(url)
#endif
}

// ------------------------------------------------------------- миниатюры

QImage RewindImageProvider::requestImage(const QString& id, QSize* size, const QSize& requested) {
  const qint64 sid = id.section('?', 0, 0).toLongLong();
  QImage img = QImage::fromData(RewindManager::instance()->store().thumb(sid));
  if (img.isNull()) {
    img = QImage(16, 9, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
  }
  if (requested.isValid() && !requested.isEmpty()) img = img.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  if (size) *size = img.size();
  return img;
}

}  // namespace crumb
