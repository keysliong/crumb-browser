#include "downloads/FileIconProvider.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commoncontrols.h>
#endif

namespace crumb {

QImage FileIconProvider::requestImage(const QString& id, QSize* size, const QSize& requested) {
  const QString path = QDir::toNativeSeparators(QUrl::fromPercentEncoding(id.section('?', 0, 0).toUtf8()));
  QImage img;
#if defined(_WIN32)
  const bool exists = QFileInfo::exists(path);
  SHFILEINFOW sfi{};
  // Существующий файл — его собственный значок (у .exe он свой), иначе — по расширению.
  const UINT flags = SHGFI_SYSICONINDEX | (exists ? 0 : SHGFI_USEFILEATTRIBUTES);
  if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags)) {
    IImageList* list = nullptr;
    // 48×48 — чётко и на экранах с масштабом 150%.
    if (SUCCEEDED(SHGetImageList(SHIL_EXTRALARGE, IID_IImageList, reinterpret_cast<void**>(&list))) && list) {
      HICON icon = nullptr;
      if (SUCCEEDED(list->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &icon)) && icon) {
        img = QImage::fromHICON(icon);
        DestroyIcon(icon);
      }
      list->Release();
    }
  }
#endif
  if (img.isNull()) {
    img = QImage(48, 48, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
  }
  if (requested.isValid() && !requested.isEmpty())
    img = img.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  if (size) *size = img.size();
  return img;
}

}  // namespace crumb
