#include "system/DefaultBrowser.h"

#include <QCoreApplication>
#include <QDir>
#include <QQmlEngine>
#if !defined(_WIN32)
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace crumb {

namespace {

#if defined(_WIN32)
constexpr wchar_t kProgId[] = L"CrumbHTML";
constexpr wchar_t kClientKey[] = L"Software\\Clients\\StartMenuInternet\\Crumb";

void setValue(const std::wstring& subkey, const wchar_t* name, const std::wstring& value) {
  HKEY key;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, subkey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) !=
      ERROR_SUCCESS)
    return;
  RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                 DWORD((value.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
}

std::wstring readValue(const std::wstring& subkey, const wchar_t* name) {
  wchar_t buf[512] = {};
  DWORD size = sizeof(buf);
  if (RegGetValueW(HKEY_CURRENT_USER, subkey.c_str(), name, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
    return {};
  return buf;
}
#endif

}  // namespace

DefaultBrowser* DefaultBrowser::instance() {
  static DefaultBrowser* self = new DefaultBrowser();
  return self;
}

DefaultBrowser* DefaultBrowser::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

DefaultBrowser::DefaultBrowser() {
  registerBrowser();
  recheck();
}

void DefaultBrowser::registerBrowser() {
#if defined(_WIN32)
  const std::wstring exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
  const std::wstring icon = L"\"" + exe + L"\",0";
  const std::wstring open = L"\"" + exe + L"\" \"%1\"";

  // ProgID для ссылок и html-файлов
  const std::wstring prog = std::wstring(L"Software\\Classes\\") + kProgId;
  setValue(prog, nullptr, L"Crumb HTML Document");
  setValue(prog + L"\\DefaultIcon", nullptr, icon);
  setValue(prog + L"\\shell\\open\\command", nullptr, open);
  setValue(prog + L"\\Application", L"ApplicationName", L"Crumb");
  setValue(prog + L"\\Application", L"ApplicationIcon", icon);
  setValue(prog + L"\\Application", L"AppUserModelId", L"Crumb.Browser");

  // Описание браузера для «Приложений по умолчанию»
  const std::wstring client = kClientKey;
  setValue(client, nullptr, L"Crumb");
  setValue(client + L"\\DefaultIcon", nullptr, icon);
  setValue(client + L"\\shell\\open\\command", nullptr, L"\"" + exe + L"\"");
  const std::wstring caps = client + L"\\Capabilities";
  setValue(caps, L"ApplicationName", L"Crumb");
  setValue(caps, L"ApplicationIcon", icon);
  setValue(caps, L"ApplicationDescription", L"Crumb — браузер в стиле Material You");
  setValue(caps + L"\\StartMenu", L"StartMenuInternet", L"Crumb");
  for (const wchar_t* ext : {L".htm", L".html", L".shtml", L".xht", L".xhtml", L".svg", L".webp", L".pdf"})
    setValue(caps + L"\\FileAssociations", ext, kProgId);
  for (const wchar_t* proto : {L"http", L"https"})
    setValue(caps + L"\\URLAssociations", proto, kProgId);
  setValue(L"Software\\RegisteredApplications", L"Crumb", caps);

  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#else
  // Linux: ярлык ~/.local/share/applications/crumb.desktop и значок — для меню,
  // панели задач и «браузера по умолчанию». В AppImage запускаем сам .AppImage.
  const QString exe = qEnvironmentVariableIsSet("APPIMAGE") ? qEnvironmentVariable("APPIMAGE")
                                                            : QCoreApplication::applicationFilePath();
  const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  const QString iconDir = data + "/icons/hicolor/256x256/apps";
  QDir().mkpath(iconDir);
  if (!QFile::exists(iconDir + "/crumb.png")) QImage(":/crumb/icon.png").save(iconDir + "/crumb.png");
  const QString desktop = QStringLiteral(
      "[Desktop Entry]\nType=Application\nName=Crumb\nGenericName=Web Browser\nGenericName[ru]=Веб-браузер\n"
      "Comment=Browser in Material You style\nComment[ru]=Браузер в стиле Material You\n"
      "Exec=\"%1\" %U\nIcon=crumb\nTerminal=false\nStartupWMClass=crumb\nCategories=Network;WebBrowser;\n"
      "MimeType=text/html;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;\n").arg(exe);
  const QString appDir = data + "/applications";
  QDir().mkpath(appDir);
  QFile f(appDir + "/crumb.desktop");
  const bool same = f.open(QIODevice::ReadOnly) && f.readAll() == desktop.toUtf8();
  f.close();
  if (!same && f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    f.write(desktop.toUtf8());
    f.close();
    QProcess::startDetached("update-desktop-database", {appDir});
  }
#endif
}

void DefaultBrowser::recheck() {
  bool def = false;
#if defined(_WIN32)
  const std::wstring base = L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\";
  def = readValue(base + L"https\\UserChoice", L"ProgId") == kProgId &&
        readValue(base + L"http\\UserChoice", L"ProgId") == kProgId;
#else
  // xdg-settings — скрипт, отвечает не мгновенно: спрашиваем асинхронно.
  auto* p = new QProcess(this);
  connect(p, &QProcess::finished, this, [this, p] {
    const bool d = QString::fromUtf8(p->readAllStandardOutput()).trimmed() == QLatin1String("crumb.desktop");
    p->deleteLater();
    if (d != isDefault_) {
      isDefault_ = d;
      emit changed();
    }
  });
  connect(p, &QProcess::errorOccurred, p, &QObject::deleteLater);
  p->start("xdg-settings", {"get", "default-web-browser"});
  return;
#endif
  if (def != isDefault_) {
    isDefault_ = def;
    emit changed();
  }
}

void DefaultBrowser::openSystemSettings() {
  registerBrowser();
#if defined(_WIN32)
  // Windows 11: сразу страница Crumb; если не сработает — общий список.
  const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open",
      L"ms-settings:defaultapps?registeredAppUser=Crumb", nullptr, nullptr, SW_SHOWNORMAL));
  if (r <= 32) ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
#else
  // На Linux программе можно назначить себя самой — без окна настроек.
  auto* p = new QProcess(this);
  connect(p, &QProcess::finished, this, [this, p] { p->deleteLater(); recheck(); });
  connect(p, &QProcess::errorOccurred, p, &QObject::deleteLater);
  p->start("xdg-settings", {"set", "default-web-browser", "crumb.desktop"});
#endif
}

}  // namespace crumb
