#include "cef/CefRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>
#if defined(_WIN32)
#include <windows.h>
#endif

#include <algorithm>

#include "cef/CefTask.h"
#include "include/cef_cookie.h"
#include "include/cef_request_context.h"
#include "include/cef_values.h"

namespace crumb {

namespace {
CefString toCef(const QString& s) { return CefString(s.toStdWString()); }
}  // namespace

CefRuntime* CefRuntime::instance() {
  static CefRuntime* self = new CefRuntime();
  return self;
}

CefRuntime* CefRuntime::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

bool CefRuntime::initialize(const CefMainArgs& args, CefRefPtr<CefApp> app) {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString root =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb/User Data";
  QDir().mkpath(root);

  CefSettings settings;
  settings.no_sandbox = true;
#if defined(_WIN32)
  settings.multi_threaded_message_loop = true;
  const QString helper = appDir + "/crumb_helper.exe";
#else
  settings.external_message_pump = true;
  const QString helper = appDir + "/crumb_helper";
#endif
  CefString(&settings.browser_subprocess_path) = toCef(QDir::toNativeSeparators(helper));
  CefString(&settings.root_cache_path) = toCef(QDir::toNativeSeparators(root));
  CefString(&settings.cache_path) = toCef(QDir::toNativeSeparators(root + "/Default"));
  CefString(&settings.log_file) = toCef(QDir::toNativeSeparators(root + "/crumb_cef.log"));
  settings.log_severity = LOGSEVERITY_WARNING;
  // Язык интерфейса Chromium (диалоги, страницы ошибок) — как в системе.
  {
    const QString full = QLocale::system().name().replace('_', '-');   // ru-RU
    const QString lang = full.section('-', 0, 0);                        // ru
    const QString locales = appDir + "/locales/";
    const QString pick = QFile::exists(locales + full + ".pak") ? full
                       : QFile::exists(locales + lang + ".pak") ? lang : QStringLiteral("en-US");
    CefString(&settings.locale) = toCef(pick);
  }
  settings.persist_session_cookies = true;

  if (!CefInitialize(args, settings, app, nullptr)) return false;

#if !defined(_WIN32)
  pump_ = new QTimer(this);
  connect(pump_, &QTimer::timeout, [] { CefDoMessageLoopWork(); });
  pump_->start(10);
#endif
  return true;
}

void CefRuntime::shutdown() {
  if (pump_) pump_->stop();
  CefShutdown();
}

bool CefRuntime::isForeground(QWindow* window) const {
  if (!window) return false;
#if defined(_WIN32)
  HWND fg = GetForegroundWindow();
  const HWND own = reinterpret_cast<HWND>(window->winId());
  return fg == own || GetAncestor(fg, GA_ROOTOWNER) == own || IsChild(own, fg);
#else
  return window->isActive();
#endif
}

QString CefRuntime::consoleLogPath() const {
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb/User Data/console.log";
}

void CefRuntime::setConsoleLog(bool on) {
  std::lock_guard lock(mutex_);
  consoleLog_ = on || qEnvironmentVariableIsSet("CRUMB_CONSOLE_LOG");
}

void CefRuntime::logConsole(int level, const QString& message, const QString& source, int line) {
  {
    std::lock_guard lock(mutex_);
    if (!consoleLog_) return;
  }
  static const char* names[] = {"verbose", "info", "warning", "error", "fatal"};
  const QString lvl = QString::fromLatin1(level >= 1 && level <= 5 ? names[level - 1] : "log");
  QFile f(consoleLogPath());
  if (f.open(QIODevice::Append | QIODevice::Text))
    f.write(QStringLiteral("%1 [%2] %3:%4  %5\n")
                .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"), lvl, source)
                .arg(line).arg(message).toUtf8());
}

QStringList CefRuntime::startupUrls() const {
  QStringList out;
  const QStringList args = QCoreApplication::arguments();
  for (int i = 1; i < args.size(); ++i)
    if (!args[i].startsWith('-')) out << args[i];
  return out;
}

void CefRuntime::requestQuit() {
  if (browsers_.load() == 0) {
    QCoreApplication::quit();
    return;
  }
  quitting_ = true;
  emit closeAllBrowsers();
}

void CefRuntime::setPageColorScheme(bool dark, quint32 argb) {
  if (!ready_) return;
  postToCefUi([dark, argb] {
    CefRefPtr<CefRequestContext> ctx = CefRequestContext::GetGlobalContext();
    if (!ctx) return;
    ctx->SetChromeColorScheme(dark ? CEF_COLOR_VARIANT_DARK : CEF_COLOR_VARIANT_LIGHT, argb);
    ctx->SetChromeColorScheme(CEF_COLOR_VARIANT_TONAL_SPOT, argb);
  });
}

void CefRuntime::setPreference(const QString& name, const QVariant& value) {
  if (!ready_) return;
  postToCefUi([name, value] {
    CefRefPtr<CefRequestContext> ctx = CefRequestContext::GetGlobalContext();
    if (!ctx) return;
    CefRefPtr<CefValue> v = CefValue::Create();
    switch (value.metaType().id()) {
      case QMetaType::Bool: v->SetBool(value.toBool()); break;
      case QMetaType::Int: case QMetaType::LongLong: case QMetaType::UInt: v->SetInt(value.toInt()); break;
      case QMetaType::Double: v->SetDouble(value.toDouble()); break;
      default: v->SetString(toCef(value.toString())); break;
    }
    CefString error;
    if (!ctx->SetPreference(toCef(name), v, error))
      qWarning("CEF pref %s: %s", qUtf8Printable(name), qUtf8Printable(QString::fromStdWString(error.ToWString())));
  });
}

void CefRuntime::clearBrowsingData() {
  postToCefUi([this] {
    CefCookieManager::GetGlobalManager(nullptr)->DeleteCookies("", "", nullptr);
    std::lock_guard lock(mutex_);
    if (!live_.empty())   // кэш чистится командой DevTools через любой браузер
      live_.front()->GetHost()->ExecuteDevToolsMethod(0, "Network.clearBrowserCache", nullptr);
  });
}

void CefRuntime::setShortcuts(const QHash<QString, QString>& seqToAction) {
  std::lock_guard lock(mutex_);
  shortcuts_ = seqToAction;
}

QString CefRuntime::actionForShortcut(const QString& seq) const {
  std::lock_guard lock(mutex_);
  return shortcuts_.value(seq);
}

void CefRuntime::setDownloads(const QString& dir, bool ask) {
  std::lock_guard lock(mutex_);
  downloadDir_ = dir;
  downloadAsk_ = ask;
}

void CefRuntime::downloads(QString* dir, bool* ask) const {
  std::lock_guard lock(mutex_);
  *dir = downloadDir_;
  *ask = downloadAsk_;
}

void CefRuntime::trackBrowser(CefRefPtr<CefBrowser> b, bool alive) {
  std::lock_guard lock(mutex_);
  if (alive) {
    live_.push_back(b);
  } else {
    live_.erase(std::remove_if(live_.begin(), live_.end(),
                               [&](const CefRefPtr<CefBrowser>& x) { return x->IsSame(b); }),
                live_.end());
  }
}

void CefRuntime::notifyContextReady() {
  QMetaObject::invokeMethod(this, [this] {
    ready_ = true;
    emit readyChanged();
  }, Qt::QueuedConnection);
}

void CefRuntime::notifyRelaunch(const QStringList& args) {
  QMetaObject::invokeMethod(this, [this, args] { emit relaunchRequested(args); }, Qt::QueuedConnection);
}

void CefRuntime::notifyStrayUrl(const QString& url) {
  QMetaObject::invokeMethod(this, [this, url] { emit strayUrlRequested(url); }, Qt::QueuedConnection);
}

void CefRuntime::browserCreated() {
  ++browsers_;
  QMetaObject::invokeMethod(this, [this] { emit browserCountChanged(); }, Qt::QueuedConnection);
}

void CefRuntime::browserClosed() {
  --browsers_;
  QMetaObject::invokeMethod(this, [this] {
    emit browserCountChanged();
    if (quitting_ && browsers_.load() == 0) QCoreApplication::quit();
  }, Qt::QueuedConnection);
}

}  // namespace crumb
