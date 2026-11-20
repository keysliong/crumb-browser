#include "SetupBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QTextStream>

#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <wrl/client.h>

#include <algorithm>
#include <map>
#include <set>

#include "../common/Payload.h"

namespace crumb {

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kUninstallKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Crumb";
constexpr char kManifest[] = "install.manifest";
SetupBackend::Options g_options;

std::wstring w(const QString& s) { return QDir::toNativeSeparators(s).toStdWString(); }

QString knownFolder(REFKNOWNFOLDERID id) {
  PWSTR p = nullptr;
  QString out;
  if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) out = QString::fromWCharArray(p);
  CoTaskMemFree(p);
  return out;
}

QString defaultDir() {
  return QDir::fromNativeSeparators(knownFolder(FOLDERID_LocalAppData)) + "/Programs/Crumb";
}

QString registryString(const wchar_t* name) {
  HKEY key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kUninstallKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return {};
  wchar_t buf[2048] = {};
  DWORD size = sizeof(buf), type = 0;
  QString out;
  if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS &&
      type == REG_SZ)
    out = QString::fromWCharArray(buf);
  RegCloseKey(key);
  return out;
}

QString installedDir() {
  const QString loc = registryString(L"InstallLocation");
  return loc.isEmpty() ? QString() : QDir::fromNativeSeparators(loc);
}

// Запущен ли Crumb из этой папки (сам браузер или его процессы).
bool crumbRunningIn(const QString& dir) {
  const QString prefix = QDir::toNativeSeparators(QDir::cleanPath(dir)).toLower() + "\\";
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W pe{sizeof(pe)};
  bool found = false;
  for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) {
    const QString name = QString::fromWCharArray(pe.szExeFile).toLower();
    if (name != "crumb.exe" && name != "crumb_helper.exe") continue;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
    if (!h) continue;
    wchar_t path[MAX_PATH * 2];
    DWORD len = DWORD(std::size(path));
    if (QueryFullProcessImageNameW(h, 0, path, &len) &&
        QString::fromWCharArray(path, int(len)).toLower().startsWith(prefix))
      found = true;
    CloseHandle(h);
  }
  CloseHandle(snap);
  return found;
}

const QString kRunningError = QStringLiteral(
    "Crumb сейчас открыт. Закройте все окна Crumb и попробуйте снова.");

struct ManifestEntry {
  bool ui;
  QString path;
};

QList<ManifestEntry> readManifest(const QString& dir) {
  QList<ManifestEntry> out;
  QFile f(dir + "/" + kManifest);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;
  QTextStream in(&f);
  in.setEncoding(QStringConverter::Utf8);
  while (!in.atEnd()) {
    const QString line = in.readLine();
    if (line.size() < 3 || line[1] != '\t') continue;
    const QString path = line.mid(2);
    // Только относительные пути внутри папки — защита от испорченного манифеста.
    if (path.contains("..") || QDir::isAbsolutePath(path) || path.contains(':')) continue;
    out.push_back({line[0] == 'u', path});
  }
  return out;
}

bool createShortcut(const QString& linkPath, const QString& target, const QString& args,
                    const QString& workDir, const QString& description) {
  ComPtr<IShellLinkW> link;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))))
    return false;
  link->SetPath(w(target).c_str());
  link->SetArguments(args.toStdWString().c_str());
  link->SetWorkingDirectory(w(workDir).c_str());
  link->SetIconLocation(w(target).c_str(), 0);
  link->SetDescription(description.toStdWString().c_str());
  ComPtr<IPersistFile> file;
  if (FAILED(link.As(&file))) return false;
  return SUCCEEDED(file->Save(w(linkPath).c_str(), TRUE));
}

bool setRegString(HKEY key, const wchar_t* name, const QString& value) {
  const std::wstring v = value.toStdWString();
  return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()),
                        DWORD((v.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}
bool setRegDword(HKEY key, const wchar_t* name, DWORD value) {
  return RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)) ==
         ERROR_SUCCESS;
}

void removeShortcuts() {
  for (const QString& folder : {knownFolder(FOLDERID_Programs), knownFolder(FOLDERID_Desktop)})
    for (const char* name : {"Crumb.lnk", "Удалить Crumb.lnk"})
      QFile::remove(folder + "/" + QString::fromUtf8(name));
}

// Crumb как браузер Windows (для текущего пользователя): ProgID CrumbHTML,
// StartMenuInternet\Crumb с Capabilities и RegisteredApplications.
// Сам браузер выбирает пользователь в «Приложениях по умолчанию» — Windows не
// разрешает программам назначать себя браузером по умолчанию.
void registerAsBrowser(const QString& exePath) {
  const QString exe = QDir::toNativeSeparators(exePath);
  const QString icon = "\"" + exe + "\",0";
  auto set = [](const QString& sub, const wchar_t* name, const QString& value) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, w(sub).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
      return;
    setRegString(key, name, value);
    RegCloseKey(key);
  };
  const QString prog = "Software\\Classes\\CrumbHTML";
  set(prog, nullptr, "Crumb HTML Document");
  set(prog + "\\DefaultIcon", nullptr, icon);
  set(prog + "\\shell\\open\\command", nullptr, "\"" + exe + "\" \"%1\"");
  set(prog + "\\Application", L"ApplicationName", "Crumb");
  set(prog + "\\Application", L"ApplicationIcon", icon);
  const QString client = "Software\\Clients\\StartMenuInternet\\Crumb";
  set(client, nullptr, "Crumb");
  set(client + "\\DefaultIcon", nullptr, icon);
  set(client + "\\shell\\open\\command", nullptr, "\"" + exe + "\"");
  const QString caps = client + "\\Capabilities";
  set(caps, L"ApplicationName", "Crumb");
  set(caps, L"ApplicationIcon", icon);
  set(caps, L"ApplicationDescription", QStringLiteral("Crumb — браузер в стиле Material You"));
  set(caps + "\\StartMenu", L"StartMenuInternet", "Crumb");
  for (const char* ext : {".htm", ".html", ".shtml", ".xht", ".xhtml", ".svg", ".webp", ".pdf"})
    set(caps + "\\FileAssociations", QString::fromLatin1(ext).toStdWString().c_str(), "CrumbHTML");
  set(caps + "\\URLAssociations", L"http", "CrumbHTML");
  set(caps + "\\URLAssociations", L"https", "CrumbHTML");
  set("Software\\RegisteredApplications", L"Crumb", caps);
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void unregisterAsBrowser() {
  RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\CrumbHTML");
  RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CrumbHTML");
  RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Clients\\StartMenuInternet\\Crumb");
  RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Clients\\StartMenuInternet\\Crumb");
  HKEY key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
    RegDeleteValueW(key, L"Crumb");
    RegCloseKey(key);
  }
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void pause(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

}  // namespace

// ------------------------------------------------------------------ создание

void SetupBackend::configure(const Options& o) { g_options = o; }

SetupBackend* SetupBackend::instance() {
  static SetupBackend* self = new SetupBackend();
  return self;
}

SetupBackend* SetupBackend::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

SetupBackend::SetupBackend() : opt_(g_options) {
  if (!opt_.payloadPath.isEmpty()) {
    payload::Archive a;
    if (a.open(w(opt_.payloadPath))) payloadSize_ = qint64(a.index().totalSize);
  }
  const QString installed = installedDir();
  if (opt_.uninstall) {
    // Сначала --dir (перезапуск из временной папки), потом запись в реестре.
    dir_ = !opt_.dirOverride.isEmpty() ? QDir::fromNativeSeparators(opt_.dirOverride) : installed;
    already_ = !dir_.isEmpty() && QFile::exists(dir_ + "/Crumb.exe");
    if (dir_.isEmpty()) dir_ = defaultDir();
  } else {
    dir_ = installed.isEmpty() ? defaultDir() : installed;
    already_ = !installed.isEmpty() && QFile::exists(installed + "/Crumb.exe");
  }
}

SetupBackend::~SetupBackend() {
  if (worker_.joinable()) worker_.join();
}

QString SetupBackend::version() const { return QStringLiteral(CRUMB_VERSION); }

void SetupBackend::setInstallDir(const QString& d) {
  const QString clean = QDir::fromNativeSeparators(d.trimmed());
  if (clean == dir_) return;
  dir_ = clean;
  emit optionsChanged();
}

void SetupBackend::chooseFolder(const QUrl& folder) {
  QString path = QDir::cleanPath(folder.toLocalFile());
  if (path.isEmpty()) return;
  if (QFileInfo(path).fileName().compare("Crumb", Qt::CaseInsensitive) != 0) path += "/Crumb";
  setInstallDir(path);
}

QString SetupBackend::shortPath(const QString& path, int max) const {
  const QString p = QDir::toNativeSeparators(path);
  if (p.size() <= max) return p;
  return p.left(3) + QStringLiteral("…") + p.right(max - 4);
}

// ----------------------------------------------------------------- работа

void SetupBackend::setProgress(double p, const QString& status) {
  QMetaObject::invokeMethod(this, [this, p, status] {
    progress_ = p;
    status_ = status;
    emit progressChanged();
  }, Qt::QueuedConnection);
}

void SetupBackend::finishWork(const QString& error) {
  if (!error.isEmpty()) qWarning("Setup error: %s", qUtf8Printable(error));
  QMetaObject::invokeMethod(this, [this, error] {
    error_ = error;
    busy_ = false;
    finished_ = true;
    emit stateChanged();
  }, Qt::QueuedConnection);
}

void SetupBackend::start() {
  if (busy_) return;
  if (worker_.joinable()) worker_.join();
  progress_ = 0;
  status_.clear();
  error_.clear();
  busy_ = true;
  finished_ = false;
  emit progressChanged();
  emit stateChanged();
  worker_ = std::thread([this] {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (opt_.uninstall) runUninstall(); else runInstall();
    CoUninitialize();
  });
}

void SetupBackend::runInstall() {
  const QString dir = QDir::cleanPath(dir_);
  setProgress(0.02, tr("Подготовка…"));
  if (dir.isEmpty() || !QDir::isAbsolutePath(dir)) return finishWork(tr("Укажите полный путь к папке установки."));
  if (crumbRunningIn(dir)) return finishWork(kRunningError);

  payload::Archive archive;
  if (opt_.payloadPath.isEmpty() || !archive.open(w(opt_.payloadPath)))
    return finishWork(tr("Не найдены установочные файлы. Запустите Crumb-Setup.exe."));
  if (!QDir().mkpath(dir)) return finishWork(tr("Не удалось создать папку «%1».").arg(QDir::toNativeSeparators(dir)));
  pause(250);

  // 1. Файлы
  const auto oldFiles = readManifest(dir);
  const auto& idx = archive.index();
  const double total = double(std::max<uint64_t>(idx.totalSize, 1));
  uint64_t done = 0;
  std::set<QString> newFiles;
  QStringList manifest;
  for (const auto& e : idx.entries) {
    std::wstring err;
    const QString rel = QString::fromStdString(e.path);
    const bool ok = archive.extract(e, fs::path(w(dir + "/" + rel)), [&](uint64_t n) {
      done += n;
      setProgress(0.04 + 0.56 * (done / total), tr("Копирование файлов…"));
    }, &err);
    if (!ok)
      return finishWork(tr("%1\n\nЕсли Crumb сейчас открыт — закройте его и попробуйте снова.")
                            .arg(QString::fromStdWString(err)));
    newFiles.insert(rel.toLower());
    manifest << QStringLiteral("%1\t%2").arg((e.flags & payload::kFlagUi) ? 'u' : 'a').arg(rel);
  }
  // Копия интерфейса установщика — это и есть удалятор.
  QFile::remove(dir + "/Uninstall.exe");
  QFile::copy(dir + "/crumb_setup.exe", dir + "/Uninstall.exe");
  manifest << QStringLiteral("u\tUninstall.exe");
  newFiles.insert("uninstall.exe");
  // Файлы прошлой версии, которых больше нет, — убираем.
  for (const auto& old : oldFiles)
    if (!newFiles.count(old.path.toLower())) QFile::remove(dir + "/" + old.path);
  {
    QFile f(dir + "/" + kManifest);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      QTextStream out(&f);
      out.setEncoding(QStringConverter::Utf8);
      for (const QString& line : manifest) out << line << '\n';
    }
  }

  // 2. Ярлыки
  setProgress(0.64, tr("Создание ярлыков…"));
  const QString exe = dir + "/Crumb.exe";
  const QString desc = tr("Crumb — браузер в стиле Material You");
  removeShortcuts();
  if (startMenu_) {
    const QString programs = knownFolder(FOLDERID_Programs);
    createShortcut(programs + "/Crumb.lnk", exe, {}, dir, desc);
    createShortcut(programs + QStringLiteral("/Удалить Crumb.lnk"), dir + "/Uninstall.exe",
                   "--uninstall", dir, tr("Удалить Crumb"));
  }
  if (desktop_) createShortcut(knownFolder(FOLDERID_Desktop) + "/Crumb.lnk", exe, {}, dir, desc);
  pause(250);
  setProgress(0.8, tr("Создание ярлыков…"));

  // 3. Регистрация в «Установленных приложениях»
  setProgress(0.84, tr("Регистрация в системе…"));
  HKEY key;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kUninstallKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                      nullptr) != ERROR_SUCCESS)
    return finishWork(tr("Ошибка записи в реестр."));
  const QString nativeDir = QDir::toNativeSeparators(dir);
  bool ok = setRegString(key, L"DisplayName", "Crumb") &&
            setRegString(key, L"DisplayVersion", version()) &&
            setRegString(key, L"Publisher", "Crumb") &&
            setRegString(key, L"DisplayIcon", "\"" + QDir::toNativeSeparators(exe) + "\",0") &&
            setRegString(key, L"InstallLocation", nativeDir) &&
            setRegString(key, L"UninstallString", "\"" + nativeDir + "\\Uninstall.exe\" --uninstall") &&
            setRegDword(key, L"NoModify", 1) && setRegDword(key, L"NoRepair", 1) &&
            setRegDword(key, L"EstimatedSize", DWORD(idx.totalSize / 1024));
  RegCloseKey(key);
  if (!ok) return finishWork(tr("Ошибка записи в реестр."));
  if (registerBrowser_) registerAsBrowser(exe);
  pause(300);
  setProgress(1.0, tr("Готово!"));
  finishWork({});
}

void SetupBackend::runUninstall() {
  const QString dir = QDir::cleanPath(dir_);
  if (crumbRunningIn(dir)) return finishWork(kRunningError);

  setProgress(0.08, tr("Удаление ярлыков…"));
  pause(300);
  removeShortcuts();

  setProgress(0.35, tr("Очистка реестра…"));
  pause(250);
  RegDeleteTreeW(HKEY_CURRENT_USER, kUninstallKey);
  RegDeleteKeyW(HKEY_CURRENT_USER, kUninstallKey);
  unregisterAsBrowser();

  setProgress(0.6, tr("Удаление файлов…"));
  pause(200);
  auto files = readManifest(dir);
  if (files.isEmpty()) {
    // Нет списка файлов — удаляем только то, что точно наше.
    for (const char* f : {"Crumb.exe", "crumb_helper.exe", "Uninstall.exe", "crumb_setup.exe"})
      files.push_back({false, QString::fromLatin1(f)});
  }
  std::set<QString> dirs;
  int i = 0;
  for (const auto& f : files) {
    const QString full = dir + "/" + f.path;
    if (QFile::exists(full) && !QFile::remove(full))
      return finishWork(tr("Не удалось удалить %1.\nЕсли Crumb открыт — закройте его и попробуйте снова.")
                            .arg(QDir::toNativeSeparators(full)));
    for (QString d = QFileInfo(f.path).path(); d != "." && !d.isEmpty(); d = QFileInfo(d).path())
      dirs.insert(d);
    setProgress(0.6 + 0.35 * (++i) / files.size(), tr("Удаление файлов…"));
  }
  QFile::remove(dir + "/" + kManifest);
  // Пустые папки — от самых глубоких к корню; чужие файлы не трогаем.
  std::vector<QString> sorted(dirs.begin(), dirs.end());
  std::sort(sorted.begin(), sorted.end(), [](const QString& a, const QString& b) {
    return a.count('/') > b.count('/');
  });
  for (const QString& d : sorted) QDir(dir).rmdir(d);
  QDir().rmdir(dir);

  if (removeData_) {
    // Только папки Crumb: профиль браузера и настройки.
    QDir(QDir::fromNativeSeparators(knownFolder(FOLDERID_LocalAppData)) + "/Crumb").removeRecursively();
    QDir(QDir::fromNativeSeparators(knownFolder(FOLDERID_RoamingAppData)) + "/Crumb").removeRecursively();
  }
  setProgress(1.0, tr("Готово"));
  finishWork({});
}

void SetupBackend::quit() {
  qInfo("Setup.quit(): finished=%d error=%d launch=%d", finished_, !error_.isEmpty(), launch_);
  if (busy_) return;
  if (!opt_.uninstall && finished_ && error_.isEmpty() && launch_) {
    const QString d = QDir::toNativeSeparators(QDir::cleanPath(dir_));
    QProcess::startDetached(d + "\\Crumb.exe", {}, d);
  }
  if (opt_.fromTemp) {
    // Мы во временной копии удалятора: после выхода стираем эту временную папку.
    const QString tempDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    if (QFileInfo(tempDir).fileName().startsWith("CrumbUninstall-")) {
      QProcess p;
      p.setProgram("cmd.exe");
      p.setNativeArguments(QStringLiteral("/C \"ping 127.0.0.1 -n 3 > nul & rmdir /s /q \"%1\"\"").arg(tempDir));
      p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a) {
        a->flags |= CREATE_NO_WINDOW;
      });
      p.setWorkingDirectory(QDir::tempPath());
      p.startDetached();
    }
  }
  QCoreApplication::quit();
}

// ------------------------------------------- перезапуск удалятора из %TEMP%

bool SetupBackend::relaunchFromTempIfNeeded(const Options& o) {
  if (!o.uninstall || o.fromTemp || !o.payloadPath.isEmpty()) return false;
  const QString exeDir = QCoreApplication::applicationDirPath();
  const auto files = readManifest(exeDir);
  if (files.isEmpty()) return false;   // запущены не из папки установки

  const QString temp = QDir::tempPath() + QStringLiteral("/CrumbUninstall-%1").arg(QCoreApplication::applicationPid());
  QDir(temp).removeRecursively();
  QDir().mkpath(temp);
  for (const auto& f : files) {
    if (!f.ui) continue;
    const QString target = temp + "/" + f.path;
    QDir().mkpath(QFileInfo(target).path());
    QFile::copy(exeDir + "/" + f.path, target);
  }
  if (!QFile::exists(temp + "/crumb_setup.exe"))
    QFile::copy(exeDir + "/Uninstall.exe", temp + "/crumb_setup.exe");
  return QProcess::startDetached(temp + "/crumb_setup.exe",
                                 {"--uninstall", "--from-temp", "--dir", QDir::toNativeSeparators(exeDir)},
                                 temp);
}

}  // namespace crumb
