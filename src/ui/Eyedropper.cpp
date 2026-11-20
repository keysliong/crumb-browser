#include "ui/Eyedropper.h"

#include <QCursor>
#include <QGuiApplication>
#include <QPixmap>
#include <QQmlEngine>
#include <QScreen>

namespace crumb {

Eyedropper* Eyedropper::instance() {
  static Eyedropper* self = new Eyedropper();
  return self;
}

Eyedropper* Eyedropper::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

bool Eyedropper::grab() {
  QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
  if (!screen) screen = QGuiApplication::primaryScreen();
  if (!screen) return false;
  const QPixmap shot = screen->grabWindow(0);
  if (shot.isNull()) return false;
  image_ = shot.toImage().convertToFormat(QImage::Format_RGB32);
  geometry_ = screen->geometry();
  dpr_ = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : screen->devicePixelRatio();
  ++serial_;
  emit grabbed();
  return true;
}

QColor Eyedropper::colorAt(double x, double y) const {
  if (image_.isNull()) return {};
  // Снимок в физических пикселях, окно — в логических.
  const double sx = double(image_.width()) / geometry_.width();
  const double sy = double(image_.height()) / geometry_.height();
  const int px = qBound(0, int(x * sx), image_.width() - 1);
  const int py = qBound(0, int(y * sy), image_.height() - 1);
  return QColor(image_.pixel(px, py));
}

}  // namespace crumb
