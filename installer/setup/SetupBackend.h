#pragma once
// Логика установщика/удалятора Crumb (по образцу установщика Notepad You):
// ставит для текущего пользователя в %LOCALAPPDATA%\Programs\Crumb без прав
// администратора, создаёт ярлыки и запись в «Установленных приложениях».

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
#include <atomic>
#include <thread>

class QQmlEngine;
class QJSEngine;

namespace crumb {

class SetupBackend final : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Setup)
  QML_SINGLETON

  Q_PROPERTY(bool uninstallMode READ uninstallMode CONSTANT)
  Q_PROPERTY(bool hasPayload READ hasPayload CONSTANT)
  Q_PROPERTY(bool alreadyInstalled READ alreadyInstalled CONSTANT)
  Q_PROPERTY(QString version READ version CONSTANT)
  Q_PROPERTY(double requiredMB READ requiredMB CONSTANT)
  Q_PROPERTY(QString installDir READ installDir WRITE setInstallDir NOTIFY optionsChanged)
  Q_PROPERTY(bool desktopShortcut READ desktopShortcut WRITE setDesktopShortcut NOTIFY optionsChanged)
  Q_PROPERTY(bool startMenuShortcut READ startMenuShortcut WRITE setStartMenuShortcut NOTIFY optionsChanged)
  Q_PROPERTY(bool removeUserData READ removeUserData WRITE setRemoveUserData NOTIFY optionsChanged)
  Q_PROPERTY(bool registerBrowser READ registerBrowser WRITE setRegisterBrowser NOTIFY optionsChanged)
  Q_PROPERTY(bool launchAfter READ launchAfter WRITE setLaunchAfter NOTIFY optionsChanged)
  Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
  Q_PROPERTY(QString status READ status NOTIFY progressChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
  Q_PROPERTY(bool finished READ finished NOTIFY stateChanged)
  Q_PROPERTY(QString error READ error NOTIFY stateChanged)

 public:
  struct Options {
    QString payloadPath;
    bool uninstall = false;
    bool fromTemp = false;      // удалятор перезапущен из временной папки
    QString dirOverride;        // --dir
  };

  static void configure(const Options& o);
  static SetupBackend* instance();
  static SetupBackend* create(QQmlEngine*, QJSEngine*);
  ~SetupBackend() override;

  // Удалятор, запущенный из папки установки, копирует себя во временную папку
  // и перезапускается оттуда — иначе не сможет удалить собственные DLL.
  // true — перезапуск выполнен, текущий процесс должен выйти.
  static bool relaunchFromTempIfNeeded(const Options& o);

  bool uninstallMode() const { return opt_.uninstall; }
  bool hasPayload() const { return payloadSize_ > 0; }
  bool alreadyInstalled() const { return already_; }
  QString version() const;
  double requiredMB() const { return payloadSize_ / 1048576.0; }

  QString installDir() const { return dir_; }
  void setInstallDir(const QString& d);
  bool desktopShortcut() const { return desktop_; }
  void setDesktopShortcut(bool v) { desktop_ = v; emit optionsChanged(); }
  bool startMenuShortcut() const { return startMenu_; }
  void setStartMenuShortcut(bool v) { startMenu_ = v; emit optionsChanged(); }
  bool removeUserData() const { return removeData_; }
  void setRemoveUserData(bool v) { removeData_ = v; emit optionsChanged(); }
  bool registerBrowser() const { return registerBrowser_; }
  void setRegisterBrowser(bool v) { registerBrowser_ = v; emit optionsChanged(); }
  bool launchAfter() const { return launch_; }
  void setLaunchAfter(bool v) { launch_ = v; emit optionsChanged(); }

  double progress() const { return progress_; }
  QString status() const { return status_; }
  bool busy() const { return busy_; }
  bool finished() const { return finished_; }
  QString error() const { return error_; }

  // Папка из диалога «Обзор…»: к ней добавляется «Crumb», если её нет.
  Q_INVOKABLE void chooseFolder(const QUrl& folder);
  Q_INVOKABLE void start();
  // Кнопка «Готово/Закрыть»: запустить Crumb (если отмечено), прибраться и выйти.
  Q_INVOKABLE void quit();
  Q_INVOKABLE QString shortPath(const QString& path, int max) const;

 signals:
  void optionsChanged();
  void progressChanged();
  void stateChanged();

 private:
  SetupBackend();
  void setProgress(double p, const QString& status);
  void runInstall();
  void runUninstall();
  void finishWork(const QString& error);

  Options opt_;
  qint64 payloadSize_ = 0;
  bool already_ = false;
  QString dir_;
  bool desktop_ = true, startMenu_ = true, removeData_ = false, launch_ = true, registerBrowser_ = true;

  double progress_ = 0;
  QString status_;
  bool busy_ = false, finished_ = false;
  QString error_;
  std::thread worker_;
};

}  // namespace crumb
