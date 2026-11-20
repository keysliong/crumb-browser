// crumb_icongen — SVG → PNG и .ico для Crumb.
//   crumb_icongen --big crumb-a.svg --small crumb-a-small.svg --out <папка> [--sheet sheet.png]
// Для 32 px и меньше берётся упрощённая версия (--small), чтобы иконка читалась.
// В .ico все размеры хранятся как PNG (поддерживается с Windows Vista).

#include <QBuffer>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QtEndian>

namespace {

QImage render(const QString& svgPath, int size) {
  QSvgRenderer r(svgPath);
  QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  r.render(&p, QRectF(0, 0, size, size));
  p.end();
  return img.convertToFormat(QImage::Format_ARGB32);
}

QByteArray png(const QImage& img) {
  QByteArray bytes;
  QBuffer buf(&bytes);
  buf.open(QIODevice::WriteOnly);
  img.save(&buf, "PNG");
  return bytes;
}

void put16(QByteArray& b, quint16 v) { v = qToLittleEndian(v); b.append(reinterpret_cast<const char*>(&v), 2); }
void put32(QByteArray& b, quint32 v) { v = qToLittleEndian(v); b.append(reinterpret_cast<const char*>(&v), 4); }

}  // namespace

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  QCommandLineParser cli;
  cli.addOptions({{"big", "SVG для больших размеров", "file"},
                  {"small", "SVG для 32 px и меньше", "file"},
                  {"out", "папка результата", "dir"},
                  {"sheet", "PNG-лист предпросмотра (светлый и тёмный фон)", "file"}});
  cli.process(app);
  const QString big = cli.value("big"), small = cli.value("small").isEmpty() ? big : cli.value("small");
  const QString out = cli.value("out");
  if (big.isEmpty() || out.isEmpty()) {
    qWarning("usage: crumb_icongen --big A.svg [--small A-small.svg] --out DIR [--sheet S.png]");
    return 2;
  }
  QDir().mkpath(out);

  const QList<int> sizes = {16, 20, 24, 32, 40, 48, 64, 128, 256};
  QList<QByteArray> images;
  for (int s : sizes) images << png(render(s <= 32 ? small : big, s));

  // .ico: заголовок, каталог, затем PNG-данные.
  QByteArray ico;
  put16(ico, 0);
  put16(ico, 1);
  put16(ico, quint16(sizes.size()));
  quint32 offset = 6 + 16 * quint32(sizes.size());
  for (int i = 0; i < sizes.size(); ++i) {
    const int s = sizes[i];
    ico.append(char(s >= 256 ? 0 : s));
    ico.append(char(s >= 256 ? 0 : s));
    ico.append(char(0));
    ico.append(char(0));
    put16(ico, 1);
    put16(ico, 32);
    put32(ico, quint32(images[i].size()));
    put32(ico, offset);
    offset += quint32(images[i].size());
  }
  for (const auto& img : images) ico.append(img);
  QFile f(out + "/crumb.ico");
  if (!f.open(QIODevice::WriteOnly)) return 1;
  f.write(ico);
  f.close();

  render(big, 512).save(out + "/crumb-512.png");
  render(big, 256).save(out + "/crumb-256.png");
  render(small, 32).save(out + "/crumb-32.png");

  if (cli.isSet("sheet")) {
    const QList<int> shown = {256, 128, 64, 48, 32, 24, 16};
    const int w = 40 + 256 + 128 + 64 + 48 + 32 + 24 + 16 + 24 * 7, h = 2 * (256 + 40);
    QImage sheet(w, h, QImage::Format_ARGB32);
    QPainter p(&sheet);
    p.fillRect(0, 0, w, h / 2, QColor("#FFFFFF"));
    p.fillRect(0, h / 2, w, h / 2, QColor("#1C1B1F"));
    for (int row = 0; row < 2; ++row) {
      int x = 20;
      for (int s : shown) {
        p.drawImage(x, row * h / 2 + 20 + (256 - s), render(s <= 32 ? small : big, s));
        x += s + 24;
      }
    }
    p.end();
    sheet.save(cli.value("sheet"));
  }
  return 0;
}
