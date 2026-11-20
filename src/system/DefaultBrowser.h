#pragma once
// DefaultBrowser — Crumb как браузер Windows (QML-синглтон).
// Регистрирует Crumb для текущего пользователя (HKCU, без прав администратора):
// ProgID CrumbHTML, StartMenuInternet\Crumb с Capabilities (http/https, .html…),
// RegisteredApplications. Windows 10/11 не разрешает программам самим назначать
// себя браузером по умолчанию — поэтому openSystemSettings() открывает страницу
// «Приложения по умолчанию» сразу на Crumb, а выбор делает пользователь.

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace crumb {

class DefaultBrowser final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)

 public:
  static DefaultBrowser* instance();
  static DefaultBrowser* create(QQmlEngine*, QJSEngine*);

  bool isDefault() const { return isDefault_; }

  // Записать регистрацию браузера в реестр (безопасно повторять при каждом запуске).
  Q_INVOKABLE void registerBrowser();
  // Открыть «Параметры → Приложения по умолчанию → Crumb».
  Q_INVOKABLE void openSystemSettings();
  Q_INVOKABLE void recheck();

 signals:
  void changed();

 private:
  DefaultBrowser();
  bool isDefault_ = false;
};

}  // namespace crumb
