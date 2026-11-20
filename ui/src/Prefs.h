#pragma once
// Prefs — параметры внешнего вида, общие для всех компонентов Crumb.Ui
// (печеньки, анимации, шрифт, звуки). Компоненты читают их напрямую, поэтому
// любое изменение в настройках видно сразу. Сохраняет их хранилище браузера;
// установщик работает со значениями по умолчанию.

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace crumb {

class Prefs : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  // Печеньки
  Q_PROPERTY(int cookieWaves MEMBER cookieWaves NOTIFY changed)
  Q_PROPERTY(double cookieDepth MEMBER cookieDepth NOTIFY changed)
  Q_PROPERTY(double cookieScale MEMBER cookieScale NOTIFY changed)
  Q_PROPERTY(double cookieSpin MEMBER cookieSpin NOTIFY changed)
  Q_PROPERTY(bool cookieBreathe MEMBER cookieBreathe NOTIFY changed)
  // Курсор и звуки
  Q_PROPERTY(bool handCursor MEMBER handCursor NOTIFY changed)
  Q_PROPERTY(bool clickSounds MEMBER clickSounds NOTIFY changed)
  Q_PROPERTY(double clickVolume MEMBER clickVolume NOTIFY changed)
  // Анимации
  Q_PROPERTY(bool animations MEMBER animations NOTIFY changed)
  Q_PROPERTY(double animSpeed MEMBER animSpeed NOTIFY changed)
  Q_PROPERTY(double springStrength MEMBER springStrength NOTIFY changed)
  // Шрифты
  Q_PROPERTY(QString fontFamily MEMBER fontFamily NOTIFY changed)   // пусто — Roboto Flex
  Q_PROPERTY(double textScale MEMBER textScale NOTIFY changed)
  Q_PROPERTY(QString font READ font NOTIFY changed)

 public:
  static Prefs* instance();
  static Prefs* create(QQmlEngine*, QJSEngine*);

  // Размер текста с учётом масштаба интерфейса.
  Q_INVOKABLE int px(double size) const;
  // Длительность анимации с учётом скорости; 0 — анимации выключены.
  Q_INVOKABLE int dur(int ms) const;
  // Жёсткость пружины с учётом «силы пружины».
  Q_INVOKABLE double spring(double base) const { return base * springStrength; }
  // Звук нажатия на печеньку (если включён).
  Q_INVOKABLE void click();

  QString font() const;

  int cookieWaves = 10;
  double cookieDepth = 0.10;
  double cookieScale = 1.0;
  double cookieSpin = 40.0;
  bool cookieBreathe = true;
  bool handCursor = true;
  bool clickSounds = false;
  double clickVolume = 0.5;
  bool animations = true;
  double animSpeed = 1.0;
  double springStrength = 1.0;
  QString fontFamily;
  double textScale = 1.0;

 signals:
  void changed();

 private:
  Prefs();
  void buildSound();

  QByteArray wav_;
  double wavVolume_ = -1;
};

}  // namespace crumb
