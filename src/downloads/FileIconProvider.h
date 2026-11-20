#pragma once
// image://fileicon/<путь> — значок файла из Windows (как в Проводнике и Edge).
// Для .exe берётся значок самой программы, для остальных — значок типа файла.

#include <QQuickImageProvider>

namespace crumb {

class FileIconProvider final : public QQuickImageProvider {
 public:
  FileIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
  QImage requestImage(const QString& id, QSize* size, const QSize& requested) override;
};

}  // namespace crumb
