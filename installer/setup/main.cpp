// crumb_setup.exe — интерфейс установщика и удалятора Crumb.
// Запускается из Crumb-Setup.exe (--payload <путь к архиву>) или как
// Uninstall.exe из папки установки (--uninstall).

#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

#include "SetupBackend.h"
#include "Theme.h"

Q_IMPORT_QML_PLUGIN(Crumb_UiPlugin)

namespace {
// Диагностика: CRUMB_SETUP_LOG=<файл> — писать сообщения Qt/QML в файл
// (у окна установщика нет консоли).
QString g_logPath;
void fileLogger(QtMsgType, const QMessageLogContext&, const QString& msg) {
  QFile f(g_logPath);
  if (f.open(QIODevice::Append | QIODevice::Text))
    f.write((QDateTime::currentDateTime().toString("hh:mm:ss.zzz ") + msg + "\n").toUtf8());
}
}  // namespace

int main(int argc, char* argv[]) {
  g_logPath = qEnvironmentVariable("CRUMB_SETUP_LOG");
  if (!g_logPath.isEmpty()) qInstallMessageHandler(fileLogger);
  QQuickStyle::setStyle(QStringLiteral("Basic"));
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("Crumb Setup"));

  crumb::SetupBackend::Options opt;
  const QStringList args = app.arguments();
  for (int i = 1; i < args.size(); ++i) {
    const QString& a = args[i];
    if (a == "--uninstall") opt.uninstall = true;
    else if (a == "--from-temp") opt.fromTemp = true;
    else if (a == "--payload" && i + 1 < args.size()) opt.payloadPath = args[++i];
    else if (a == "--dir" && i + 1 < args.size()) opt.dirOverride = args[++i];
  }
  if (crumb::SetupBackend::relaunchFromTempIfNeeded(opt)) return 0;
  crumb::SetupBackend::configure(opt);

  // Как в образце: фиксированный лавандовый цвет, светлая/тёмная — как в системе.
  crumb::Theme::instance()->configureFixed(QColor(0x8E, 0x6F, 0xD8));

  QQmlApplicationEngine engine;
  QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                   [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("CrumbSetup", "SetupWindow");
  return app.exec();
}
