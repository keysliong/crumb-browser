#pragma once
// Eyedropper — пипетка: снимок экрана, на котором курсор, и цвет пикселя по клику.
// QML показывает снимок во весь экран с лупой (EyedropperWindow.qml).

#include <QImage>
#include <QObject>
#include <QQuickImageProvider>
#include <QRect>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace crumb {

class Eyedropper final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QRect screenGeometry READ screenGeometry NOTIFY grabbed)
  Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY grabbed)

 public:
  static Eyedropper* instance();
  static Eyedropper* create(QQmlEngine*, QJSEngine*);

  QRect screenGeometry() const { return geometry_; }
  QString imageUrl() const { return QStringLiteral("image://eyedropper/%1").arg(serial_); }
  QImage image() const { return image_; }

  Q_INVOKABLE bool grab();
  // x, y — в логических координатах окна-снимка.
  Q_INVOKABLE QColor colorAt(double x, double y) const;
  Q_INVOKABLE void release() { image_ = QImage(); }

 signals:
  void grabbed();

 private:
  Eyedropper() = default;
  QImage image_;
  QRect geometry_;
  qreal dpr_ = 1;
  int serial_ = 0;
};

class EyedropperImageProvider final : public QQuickImageProvider {
 public:
  EyedropperImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
  QImage requestImage(const QString&, QSize* size, const QSize&) override {
    const QImage img = Eyedropper::instance()->image();
    if (size) *size = img.size();
    return img;
  }
};

}  // namespace crumb
