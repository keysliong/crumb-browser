#include "extensions/ExtensionManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QUrl>
#include <QWindow>

#include "cef/CefRuntime.h"
#include "cef/CefTask.h"
#include "include/cef_client.h"
#include "include/cef_values.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace crumb {

namespace {

QString qs(const CefString& s) { return QString::fromStdWString(s.ToWString()); }

// Скрипт моста: выполняется на странице chrome://extensions.
const char* kBridgeJs = R"JS(
(function () {
  if (window.__crumbBridge) { window.__crumbList && window.__crumbList(); return; }
  window.__crumbBridge = true;
  const send = (type, data) => console.log('CRUMB_EXT:' + JSON.stringify({ type, data }));
  const fail = (e) => send('error', String((e && e.message) || e));
  async function list() {
    try {
      const infos = await chrome.developerPrivate.getExtensionsInfo({ includeDisabled: true, includeTerminated: true });
      send('list', infos.filter(i => i.type === 'EXTENSION' || i.type === 'LEGACY_PACKAGED_APP' || !i.type).map(i => ({
        id: i.id, name: i.name, description: i.description || '', version: i.version || '',
        enabled: i.state === 'ENABLED', state: i.state || '', location: i.location || '',
        path: i.prettifiedPath || '',
        permissions: ((i.permissions && i.permissions.simplePermissions) || []).map(p => p.message),
        hostAccess: (i.permissions && i.permissions.runtimeHostPermissions &&
                     i.permissions.runtimeHostPermissions.hostAccess) || '',
        optionsUrl: (i.optionsPage && i.optionsPage.url) || '',
        homepage: (i.homePage && i.homePage.specified && i.homePage.url) || '',
        mayModify: !i.mustRemainInstalled && i.userMayModify !== false
      })));
    } catch (e) { fail(e); }
  }
  window.__crumbList = list;
  window.__crumbSetEnabled = (id, on) => chrome.management.setEnabled(id, on).then(list).catch(fail);
  window.__crumbRemove = (id) => chrome.management.uninstall(id, { showConfirmDialog: false }).then(list).catch(fail);
  window.__crumbLoadUnpacked = () =>
    chrome.developerPrivate.updateProfileConfiguration({ inDeveloperMode: true })
      .then(() => chrome.developerPrivate.loadUnpacked({ failQuietly: false, populateErrorForUnpacked: true }))
      .then(list).catch(fail);
  try { chrome.developerPrivate.onItemStateChanged.addListener(() => list()); } catch (e) {}
  try { chrome.management.onInstalled.addListener(() => list()); } catch (e) {}
  list();
})();
)JS";

// Клиент скрытого браузера-моста.
class BridgeClient final : public CefClient, public CefLifeSpanHandler, public CefLoadHandler,
                           public CefDisplayHandler {
 public:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

  void OnAfterCreated(CefRefPtr<CefBrowser> b) override {
    CefRuntime::instance()->browserCreated();
#if defined(_WIN32)
    ::ShowWindow(b->GetHost()->GetWindowHandle(), SW_HIDE);   // мост невидим
#endif
    ExtensionManager::instance()->onBridgeCreated(b);
  }
  void OnBeforeClose(CefRefPtr<CefBrowser>) override {
    ExtensionManager::instance()->onBridgeClosed();
    CefRuntime::instance()->browserClosed();
  }
  void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int) override {
    if (frame->IsMain()) ExtensionManager::instance()->onBridgeLoaded();
  }
  bool OnConsoleMessage(CefRefPtr<CefBrowser>, cef_log_severity_t, const CefString& message,
                        const CefString&, int) override {
    const QString m = qs(message);
    if (!m.startsWith(QLatin1String("CRUMB_EXT:"))) return false;
    ExtensionManager::instance()->onBridgeMessage(m.mid(10));
    return true;
  }

 private:
  IMPLEMENT_REFCOUNTING(BridgeClient);
};

QString profileDir() {
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb/User Data/Default";
}

// Папка расширения: из магазина — Extensions/<id>/<версия>, распакованное — его путь.
QString extensionRoot(const QString& id, const QString& prettifiedPath) {
  QDir store(profileDir() + "/Extensions/" + id);
  if (store.exists()) {
    const QStringList versions = store.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (!versions.isEmpty()) return store.filePath(versions.last());
  }
  QString p = prettifiedPath;
  if (p.startsWith("~")) p = QDir::homePath() + p.mid(1);
  return QDir(p).exists() ? QDir(p).absolutePath() : QString();
}

QString pickIcon(const QJsonValue& v, int want) {
  if (v.isString()) return v.toString();
  const QJsonObject o = v.toObject();
  QString best;
  int bestSize = 0;
  for (auto it = o.begin(); it != o.end(); ++it) {
    const int s = it.key().toInt();
    if ((bestSize < want && s > bestSize) || (s >= want && s < bestSize)) { bestSize = s; best = it.value().toString(); }
  }
  return best;
}

}  // namespace

ExtensionManager* ExtensionManager::instance() {
  static ExtensionManager* self = new ExtensionManager();
  return self;
}

ExtensionManager* ExtensionManager::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

ExtensionManager::ExtensionManager() {
  connect(CefRuntime::instance(), &CefRuntime::closeAllBrowsers, this, [this] {
    if (bridge_) bridge_->GetHost()->CloseBrowser(true);
  });
}

void ExtensionManager::start(QWindow* window) {
  if (bridge_ || starting_ || !window || !CefRuntime::instance()->ready()) return;
  starting_ = true;
  CefWindowInfo info;
#if defined(_WIN32)
  info.SetAsChild(reinterpret_cast<CefWindowHandle>(window->winId()), CefRect(0, 0, 1, 1));
#endif
  info.runtime_style = CEF_RUNTIME_STYLE_CHROME;
  CefBrowserSettings settings;
  CefBrowserHost::CreateBrowser(info, new BridgeClient(), "chrome://extensions/", settings, nullptr, nullptr);
}

void ExtensionManager::onBridgeCreated(CefRefPtr<CefBrowser> b) {
  QMetaObject::invokeMethod(this, [this, b] { bridge_ = b; starting_ = false; }, Qt::QueuedConnection);
}

void ExtensionManager::onBridgeClosed() {
  QMetaObject::invokeMethod(this, [this] {
    bridge_ = nullptr;
    ready_ = false;
    emit readyChanged();
  }, Qt::QueuedConnection);
}

void ExtensionManager::onBridgeLoaded() {
  QMetaObject::invokeMethod(this, [this] {
    ready_ = true;
    emit readyChanged();
    run(QString::fromUtf8(kBridgeJs));
  }, Qt::QueuedConnection);
}

void ExtensionManager::run(const QString& js) {
  if (bridge_) bridge_->GetMainFrame()->ExecuteJavaScript(CefString(js.toStdWString()), "crumb://extensions-bridge", 0);
}

void ExtensionManager::refresh() { run("window.__crumbList && window.__crumbList()"); }

void ExtensionManager::setEnabled(const QString& id, bool enabled) {
  run(QStringLiteral("window.__crumbSetEnabled(%1, %2)")
          .arg(QString::fromUtf8(QJsonDocument(QJsonArray{id}).toJson(QJsonDocument::Compact)).mid(1).chopped(1),
               enabled ? "true" : "false"));
}

void ExtensionManager::remove(const QString& id) {
  // chrome.management.uninstall работает только «по нажатию человека» — обычный
  // ExecuteJavaScript таким не считается. DevTools-команда Runtime.evaluate с
  // userGesture: true выполняет код как действие пользователя.
  const QString js = QStringLiteral("window.__crumbRemove(%1)")
                         .arg(QString::fromUtf8(QJsonDocument(QJsonArray{id}).toJson(QJsonDocument::Compact)).mid(1).chopped(1));
  CefRefPtr<CefBrowser> b = bridge_;
  if (!b) return;
  postToCefUi([b, js] {
    CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
    params->SetString("expression", CefString(js.toStdWString()));
    params->SetBool("userGesture", true);
    params->SetBool("awaitPromise", false);
    b->GetHost()->ExecuteDevToolsMethod(0, "Runtime.evaluate", params);
  });
}

void ExtensionManager::loadUnpacked() { run("window.__crumbLoadUnpacked()"); }

QVariantMap ExtensionManager::find(const QString& id) const {
  for (const QVariant& v : items_)
    if (v.toMap().value("id").toString() == id) return v.toMap();
  return {};
}

// Значок и всплывающее окно — из manifest.json расширения.
QVariantMap ExtensionManager::enrich(const QVariantMap& info) const {
  QVariantMap m = info;
  const QString id = info.value("id").toString();
  const QString root = extensionRoot(id, info.value("path").toString());
  m.insert("fromStore", info.value("location").toString() == "FROM_STORE");
  m.insert("icon", QString());
  m.insert("hasPopup", false);
  if (root.isEmpty()) return m;
  QFile f(root + "/manifest.json");
  if (!f.open(QIODevice::ReadOnly)) return m;
  const QJsonObject man = QJsonDocument::fromJson(f.readAll()).object();
  QJsonObject action = man.value("action").toObject();
  if (action.isEmpty()) action = man.value("browser_action").toObject();
  if (action.isEmpty()) action = man.value("page_action").toObject();
  QString icon = pickIcon(action.value("default_icon"), 48);
  if (icon.isEmpty()) icon = pickIcon(man.value("icons"), 64);
  if (!icon.isEmpty()) {
    if (icon.startsWith('/')) icon = icon.mid(1);
    m.insert("icon", QUrl::fromLocalFile(root + "/" + icon).toString());
  }
  const QString popup = action.value("default_popup").toString();
  m.insert("hasAction", !man.value("action").isUndefined() || !man.value("browser_action").isUndefined() ||
                        !man.value("page_action").isUndefined());
  m.insert("hasPopup", !popup.isEmpty());
  if (!popup.isEmpty())
    m.insert("popupUrl", QStringLiteral("chrome-extension://%1/%2").arg(id, popup.startsWith('/') ? popup.mid(1) : popup));
  m.insert("manifestVersion", man.value("manifest_version").toInt());
  return m;
}

void ExtensionManager::onBridgeMessage(const QString& json) {
  QMetaObject::invokeMethod(this, [this, json] {
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    const QString type = o.value("type").toString();
    if (type == "list") {
      QVariantList list;
      for (const QJsonValue& v : o.value("data").toArray()) list << enrich(v.toObject().toVariantMap());
      std::sort(list.begin(), list.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value("name").toString().localeAwareCompare(b.toMap().value("name").toString()) < 0;
      });
      items_ = list;
      emit itemsChanged();
    } else if (type == "error") {
      lastError_ = o.value("data").toString();
      emit errorOccurred(lastError_);
    }
  }, Qt::QueuedConnection);
}

}  // namespace crumb
