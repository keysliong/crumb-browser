// Crumb — точка входа.

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <memory>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QtQml/qqmlextensionplugin.h>

#include "Prefs.h"
#include "Theme.h"
#include "cef/CefRuntime.h"
#include "cef/CrumbCefApp.h"
#include "storage/SettingsStore.h"
#include "rewind/RewindManager.h"
#include "history/HistoryManager.h"
#include "passwords/PasswordManager.h"
#include "downloads/FileIconProvider.h"
#include "ui/Eyedropper.h"
#include "ui/NativeWindow.h"

// Статический QML-модуль Crumb.Ui (тема, печеньки).
Q_IMPORT_QML_PLUGIN(Crumb_UiPlugin)

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

// Применить к Chromium настройки, которые живут на его стороне.
void applyToChromium(const QString& key) {
  auto* s = crumb::SettingsStore::instance();
  auto* rt = crumb::CefRuntime::instance();
  const bool all = key.isEmpty();
  if (all || key == "pagesFontSize")
    rt->setPreference("webkit.webprefs.default_font_size", s->get("pagesFontSize").toInt());
  if (all || key == "privacyDnt")
    rt->setPreference("enable_do_not_track", s->get("privacyDnt").toBool());
  if (all || key == "privacyBlock3p")   // 1 — блокировать сторонние cookie, 0 — разрешить
    rt->setPreference("profile.cookie_controls_mode", s->get("privacyBlock3p").toBool() ? 1 : 0);
  if (all) {
    // Пароли сохраняет и подставляет сам Crumb — встроенный менеджер Chromium выключаем.
    rt->setPreference("credentials_enable_service", false);
    rt->setPreference("credentials_enable_autosignin", false);
    rt->setPreference("profile.password_manager_enabled", false);
    // Подсказки Chromium из истории полей вылезали бы поверх списка паролей Crumb.
    rt->setPreference("autofill.profile_enabled", false);
  }
  if (all || key == "debugConsoleLog")
    rt->setConsoleLog(s->get("debugConsoleLog").toBool());
  if (all || key.startsWith("downloads"))
    rt->setDownloads(s->get("downloadsDir").toString(), s->get("downloadsAsk").toBool());
  if (all || key.startsWith("shortcut")) {
    // shortcutNewTab = "Ctrl+T"  →  "Ctrl+T" → "newTab"
    QHash<QString, QString> map;
    for (const QString& k : s->keys()) {
      if (!k.startsWith("shortcut")) continue;
      const QString seq = s->get(k).toString();
      if (seq.isEmpty()) continue;
      QString action = k.mid(8);
      action[0] = action[0].toLower();
      map.insert(seq, action);
    }
    rt->setShortcuts(map);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
#if defined(_WIN32)
  CefMainArgs mainArgs(GetModuleHandleW(nullptr));
#else
  CefMainArgs mainArgs(argc, argv);
#endif
  CefRefPtr<crumb::CrumbCefApp> cefApp = new crumb::CrumbCefApp();

  // Подпроцессы запускаются через crumb_helper.exe, но если Chromium всё же
  // перезапустит основной exe с --type=…, отрабатываем это здесь и выходим.
  if (const int code = CefExecuteProcess(mainArgs, cefApp, nullptr); code >= 0) return code;

#if defined(__linux__)
  // Окна Chromium встраиваются через X11: Qt тоже должен работать через X11
  // (на Wayland — XWayland). Переменная окружения пользователя важнее.
  if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "xcb");
  crumb::native::installXErrorHandlers();
  QGuiApplication::setDesktopFileName(QStringLiteral("crumb"));
#endif
  QGuiApplication::setApplicationName(QStringLiteral("Crumb"));
  QGuiApplication::setApplicationDisplayName(QStringLiteral("Crumb"));
  QGuiApplication::setApplicationVersion(QStringLiteral(CRUMB_VERSION));
  QQuickStyle::setStyle(QStringLiteral("Basic"));
  QGuiApplication app(argc, argv);
  // Окно закрываем сами — после того как CEF закроет все браузеры.
  QGuiApplication::setQuitOnLastWindowClosed(false);

  // CRUMB_DEMO=1 — отдельный чистый профиль (для скриншотов и проверок): все данные Crumb
  // лежат в тестовой папке Qt, а не в %LOCALAPPDATA%\Crumb, и не мешают основному Crumb.
  if (qEnvironmentVariableIsSet("CRUMB_DEMO")) QStandardPaths::setTestModeEnabled(true);

  auto* runtime = crumb::CefRuntime::instance();
  if (!runtime->initialize(mainArgs, cefApp)) {
    // Уже запущен другой Crumb: CEF передал ему наши аргументы.
    return 0;
  }

  // Палитра, шрифт и все настройки готовы до создания первого окна.
  auto* theme = crumb::Theme::instance();
  crumb::Prefs::instance();
  auto* settings = crumb::SettingsStore::instance();
  settings->load();
  crumb::HistoryManager::instance()->open();
  crumb::PasswordManager::instance()->open();

  // Страницы следуют теме Crumb, а не теме Windows.
  auto syncPages = [theme, runtime] {
    runtime->setPageColorScheme(theme->dark(), theme->sourceColor().rgba());
  };
  QObject::connect(theme, &crumb::Theme::schemeChanged, runtime, syncPages);
  QObject::connect(runtime, &crumb::CefRuntime::readyChanged, theme, [syncPages] {
    syncPages();
    applyToChromium({});
  });
  QObject::connect(settings, &crumb::SettingsStore::settingChanged, runtime, &applyToChromium);
  applyToChromium({});   // загрузки и клавиши нужны сразу; prefs — когда CEF готов

  int rc = 0;
  {
    QQmlApplicationEngine engine;
    engine.addImageProvider("eyedropper", new crumb::EyedropperImageProvider);
    engine.addImageProvider("rewind", new crumb::RewindImageProvider);
    engine.addImageProvider("fileicon", new crumb::FileIconProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    // Самотест сохранения настроек: CRUMB_SELFTEST=1 — запись из QML и выход.
    if (qEnvironmentVariableIsSet("CRUMB_SELFTEST")) {
      QQmlComponent c(&engine);
      c.setData("import QtQuick\nimport Crumb\nQtObject { Component.onCompleted: {"
                " Settings.homeNotes = 'selftest-' + Date.now();"
                " Settings.homeSites = [{ title: 'Тест', url: 'https://example.com' }];"
                " console.log('SELFTEST read back:', Settings.homeNotes, JSON.stringify(Settings.homeSites)) } }",
                QUrl("selftest.qml"));
      std::unique_ptr<QObject> obj(c.create());
      if (!obj) qWarning("SELFTEST: %s", qUtf8Printable(c.errorString()));
      QCoreApplication::processEvents();
      runtime->shutdown();
      return 0;
    }
    engine.loadFromModule("Crumb", "Main");
    rc = app.exec();
  }
  runtime->shutdown();
  return rc;
}
