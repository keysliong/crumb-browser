#include "Theme.h"

#include <QEasingCurve>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImageReader>
#include <QQmlEngine>
#include <QSettings>
#include <QStyleHints>
#include <QTime>
#include <QTimer>
#include <QVariantAnimation>
#include <QWindow>

#include "MaterialColor.h"

#if defined(_WIN32)
#include <windows.h>
#include <dwmapi.h>
#else
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QUrl>
#endif

namespace crumb {

namespace {

QString wallpaperFile() {
#if defined(_WIN32)
  wchar_t buf[MAX_PATH] = {};
  if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, buf, 0)) return QString::fromWCharArray(buf);
  return {};
#else
  auto toPath = [](QString v) {
    v = v.trimmed();
    if (v.startsWith('\'') && v.endsWith('\'')) v = v.mid(1, v.size() - 2);
    if (v.startsWith("file://")) v = QUrl(v).toLocalFile();
    return v;
  };
  // KDE Plasma (CachyOS, Kubuntu, Fedora KDE…)
  QFile kde(QDir::homePath() + "/.config/plasma-org.kde.plasma.desktop-appletsrc");
  if (kde.open(QIODevice::ReadOnly | QIODevice::Text)) {
    while (!kde.atEnd()) {
      const QString line = QString::fromUtf8(kde.readLine()).trimmed();
      if (line.startsWith("Image=")) {
        QString p = toPath(line.mid(6));
        // Папка темы обоев Plasma: берём картинку из contents/images
        if (QFileInfo(p).isDir()) {
          const QStringList imgs = QDir(p + "/contents/images").entryList({"*.png", "*.jpg", "*.jpeg", "*.webp"},
                                                                          QDir::Files, QDir::Name);
          if (!imgs.isEmpty()) p = p + "/contents/images/" + imgs.last();
        }
        if (QFileInfo(p).isFile()) return p;
      }
    }
  }
  // GNOME и производные
  for (const char* key : {"picture-uri-dark", "picture-uri"}) {
    QProcess g;
    g.start("gsettings", {"get", "org.gnome.desktop.background", key});
    if (g.waitForFinished(1500)) {
      const QString p = toPath(QString::fromUtf8(g.readAllStandardOutput()));
      if (QFileInfo(p).isFile()) return p;
    }
  }
  return {};
#endif
}

QColor solidBackgroundColor() {
#if defined(_WIN32)
  const DWORD c = GetSysColor(COLOR_BACKGROUND);
  return QColor(GetRValue(c), GetGValue(c), GetBValue(c));
#else
  return QColor();
#endif
}

QColor lerpColor(const QColor& a, const QColor& b, double t) {
  return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t),
                          float(a.greenF() + (b.greenF() - a.greenF()) * t),
                          float(a.blueF() + (b.blueF() - a.blueF()) * t),
                          float(a.alphaF() + (b.alphaF() - a.alphaF()) * t));
}

}  // namespace

Theme* Theme::instance() {
  static Theme* self = new Theme();
  return self;
}

Theme* Theme::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

Theme::Theme() {
  // Шрифт интерфейса — Roboto Flex (OFL), вшит в ресурсы.
  const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/crumb/fonts/RobotoFlex.ttf"));
  const QStringList fams = QFontDatabase::applicationFontFamilies(id);
  fontFamily_ = fams.isEmpty() ? QGuiApplication::font().family() : fams.first();
  QFont f(fontFamily_);
  f.setPointSizeF(10.0);
  f.setHintingPreference(QFont::PreferNoHinting);
  QGuiApplication::setFont(f);

  const QStringList all = QFontDatabase::families();
  if (all.contains(QStringLiteral("Segoe Fluent Icons"))) {
    iconFont_ = QStringLiteral("Segoe Fluent Icons");
  } else if (all.contains(QStringLiteral("Segoe MDL2 Assets"))) {
    iconFont_ = QStringLiteral("Segoe MDL2 Assets");
  } else {
    // Linux и др.: свой шрифт с теми же кодами (scripts/make-iconfont.py, из Material Symbols).
    const int icons = QFontDatabase::addApplicationFont(QStringLiteral(":/crumb/fonts/CrumbIcons.ttf"));
    const QStringList f2 = QFontDatabase::applicationFontFamilies(icons);
    iconFont_ = f2.isEmpty() ? QStringLiteral("Segoe MDL2 Assets") : f2.first();
  }

  anim_ = new QVariantAnimation(this);
  anim_->setStartValue(0.0);
  anim_->setEndValue(1.0);
  anim_->setEasingCurve(QEasingCurve::OutCubic);
  connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
    const double t = v.toDouble();
    for (int i = 0; i < current_.size(); ++i) current_[i] = lerpColor(from_[i], to_[i], t);
    emit colorsChanged();
  });

  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
          [this] { if (themeMode_ == QLatin1String("system")) rebuild(); });

  // Обои могут смениться в любой момент — проверяем раз в 20 секунд (дёшево).
  wallpaperPoll_ = new QTimer(this);
  connect(wallpaperPoll_, &QTimer::timeout, this, &Theme::refreshWallpaper);
  wallpaperPoll_->start(20000);

  // Расписание тёмной темы проверяем раз в 30 секунд.
  scheduleTimer_ = new QTimer(this);
  connect(scheduleTimer_, &QTimer::timeout, this, [this] {
    if (themeMode_ == QLatin1String("schedule")) rebuild();
  });
  scheduleTimer_->start(30000);

  refreshWallpaper();
  rebuild(false);
}

void Theme::setDarkFrom(const QString& t) {
  if (t == darkFrom_) return;
  darkFrom_ = t;
  emit settingsChanged();
  rebuild();
}

void Theme::setDarkTo(const QString& t) {
  if (t == darkTo_) return;
  darkTo_ = t;
  emit settingsChanged();
  rebuild();
}

void Theme::configureFixed(QColor seed) {
  persistent_ = false;
  wallpaperPoll_->stop();
  themeMode_ = QStringLiteral("system");
  sourceType_ = QStringLiteral("custom");
  customColor_ = seed;
  rebuild(false);
}

// ------------------------------------------------------------- настройки

void Theme::load() {
  QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Crumb"), QStringLiteral("theme"));
  themeMode_ = s.value("mode", themeMode_).toString();
  sourceType_ = s.value("source", sourceType_).toString();
  customColor_ = QColor(s.value("customColor", customColor_.name()).toString());
  amoled_ = s.value("amoled", amoled_).toBool();
  contrast_ = s.value("contrast", contrast_).toDouble();
  transitionMs_ = s.value("transitionMs", transitionMs_).toInt();
}

void Theme::save() {
  if (!persistent_) return;
  QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Crumb"), QStringLiteral("theme"));
  s.setValue("mode", themeMode_);
  s.setValue("source", sourceType_);
  s.setValue("customColor", customColor_.name());
  s.setValue("amoled", amoled_);
  s.setValue("contrast", contrast_);
  s.setValue("transitionMs", transitionMs_);
}

void Theme::setThemeMode(const QString& m) {
  if (m == themeMode_) return;
  themeMode_ = m;
  save();
  emit settingsChanged();
  rebuild();
}

void Theme::setSourceType(const QString& t) {
  if (t == sourceType_) return;
  sourceType_ = t;
  save();
  emit settingsChanged();
  rebuild();
}

void Theme::setCustomColor(const QColor& c) {
  if (c == customColor_ && sourceType_ == QLatin1String("custom")) return;
  customColor_ = c;
  sourceType_ = QStringLiteral("custom");
  save();
  emit settingsChanged();
  rebuild();
}

void Theme::setAmoled(bool on) {
  if (on == amoled_) return;
  amoled_ = on;
  save();
  emit settingsChanged();
  rebuild();
}

void Theme::setContrast(double c) {
  if (qFuzzyCompare(c, contrast_)) return;
  contrast_ = c;
  save();
  emit settingsChanged();
  rebuild();
}

void Theme::setTransitionMs(int ms) {
  if (ms == transitionMs_) return;
  transitionMs_ = ms;
  save();
  emit settingsChanged();
}

// ------------------------------------------------------------------- обои

void Theme::refreshWallpaper() {
  const QString path = wallpaperFile();
  const QDateTime stamp = QFileInfo(path).lastModified();
  if (!wallpaper_.isEmpty() && path == wallpaperPath_ && stamp == wallpaperStamp_) return;
  wallpaperPath_ = path;
  wallpaperStamp_ = stamp;

  QVector<uint32_t> colors;
  if (!path.isEmpty()) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    // Уменьшаем прямо при декодировании — 4K-обои не грузим целиком.
    const QSize sz = reader.size();
    if (sz.isValid()) reader.setScaledSize(sz.scaled(256, 256, Qt::KeepAspectRatio));
    const QImage img = reader.read();
    if (!img.isNull()) colors = mcu::sourceColorsFromImage(img);
  }
  if (colors.isEmpty()) {
    const QColor solid = solidBackgroundColor();
    colors.push_back(solid.isValid() ? solid.rgba() : mcu::kFallbackSource);
  }
  if (colors != wallpaper_) {
    wallpaper_ = colors;
    emit wallpaperColorsChanged();
    if (sourceType_ == QLatin1String("wallpaper")) rebuild();
  }
}

QVariantList Theme::wallpaperColors() const {
  QVariantList l;
  for (uint32_t c : wallpaper_) l << QColor::fromRgba(c);
  return l;
}

QVariantList Theme::presetColors() const {
  static const uint32_t presets[] = {0xFF4285F4, 0xFF8E6FD8, 0xFFE8638C, 0xFFD84B3B, 0xFFE8833A,
                                     0xFFD9A520, 0xFF3E9B5F, 0xFF2B9A9A, 0xFF8B6B4E};
  QVariantList l;
  for (uint32_t c : presets) l << QColor::fromRgba(c);
  return l;
}

// ------------------------------------------------------------------ схема

bool Theme::systemDark() const {
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

void Theme::rebuild(bool animate) {
  bool dark = themeMode_ == QLatin1String("dark") ||
              (themeMode_ == QLatin1String("system") && systemDark());
  if (themeMode_ == QLatin1String("schedule")) {
    const QTime now = QTime::currentTime();
    const QTime from = QTime::fromString(darkFrom_, "HH:mm"), to = QTime::fromString(darkTo_, "HH:mm");
    if (from.isValid() && to.isValid())
      dark = from <= to ? (now >= from && now < to) : (now >= from || now < to);  // через полночь
  }
  const uint32_t source = sourceType_ == QLatin1String("custom") || wallpaper_.isEmpty()
                              ? customColor_.rgba()
                              : wallpaper_.first();

  mcu::SchemeOptions opt;
  opt.dark = dark;
  opt.amoled = amoled_;
  opt.contrast = contrast_;
  const QVector<QColor> next = mcu::tonalSpotScheme(source, opt);

  const bool schemeDiff = dark != dark_ || source != source_;
  dark_ = dark;
  source_ = source;

  if (!animate || current_.isEmpty() || transitionMs_ <= 0) {
    anim_->stop();
    current_ = from_ = to_ = next;
    emit colorsChanged();
  } else {
    from_ = current_;
    to_ = next;
    anim_->stop();
    anim_->setDuration(transitionMs_);
    anim_->start();
  }
  if (schemeDiff) emit schemeChanged();
}

QColor Theme::toneOf(double hue, double chroma, double tone) const {
  return QColor::fromRgba(mcu::hctToArgb(hue, chroma, tone));
}

double Theme::hueOf(const QColor& c) const { return mcu::Cam16::fromArgb(c.rgba()).hue; }
double Theme::chromaOf(const QColor& c) const { return mcu::Cam16::fromArgb(c.rgba()).chroma; }

QColor Theme::groupColor(int index) const {
  static const double hues[] = {25, 55, 95, 145, 195, 255, 295, 340};
  const double h = hues[((index % 8) + 8) % 8];
  return toneOf(h, 48, dark_ ? 80 : 45);
}

// ----------------------------------------------------------- окна Windows 11

void WindowEffects::roundCorners(QWindow* window, bool round) {
#if defined(_WIN32)
  if (!window) return;
  const HWND hwnd = reinterpret_cast<HWND>(window->winId());
  // DWMWA_WINDOW_CORNER_PREFERENCE = 33; DWMWCP_ROUND = 2, DWMWCP_DONOTROUND = 1
  const DWORD pref = round ? 2 : 1;
  DwmSetWindowAttribute(hwnd, 33, &pref, sizeof(pref));
  // Тень вокруг окна без рамки: «расширяем» рамку DWM на 1 пиксель.
  const MARGINS m = {0, 0, round ? 1 : 0, 0};
  DwmExtendFrameIntoClientArea(hwnd, &m);
#else
  Q_UNUSED(window);
  Q_UNUSED(round);
#endif
}

}  // namespace crumb
