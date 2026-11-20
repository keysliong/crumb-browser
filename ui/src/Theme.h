#pragma once
// Theme — динамическая палитра Material You для всего Crumb (QML-синглтон).
// Источник цвета: обои рабочего стола или цвет, выбранный пользователем.
// Смена темы/цвета анимируется: все роли плавно перетекают за ~450 мс.

#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QVariantList>
#include <QVector>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class QTimer;
class QVariantAnimation;

namespace crumb {

class Theme final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  // --- выбор пользователя
  Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY settingsChanged)    // system|light|dark|schedule
  // Расписание тёмной темы (для themeMode = "schedule"), «ЧЧ:ММ».
  Q_PROPERTY(QString darkFrom READ darkFrom WRITE setDarkFrom NOTIFY settingsChanged)
  Q_PROPERTY(QString darkTo READ darkTo WRITE setDarkTo NOTIFY settingsChanged)
  Q_PROPERTY(QString wallpaperPath READ wallpaperPath NOTIFY wallpaperColorsChanged)
  Q_PROPERTY(QString sourceType READ sourceType WRITE setSourceType NOTIFY settingsChanged) // wallpaper|custom
  Q_PROPERTY(QColor customColor READ customColor WRITE setCustomColor NOTIFY settingsChanged)
  Q_PROPERTY(bool amoled READ amoled WRITE setAmoled NOTIFY settingsChanged)
  Q_PROPERTY(double contrast READ contrast WRITE setContrast NOTIFY settingsChanged)
  Q_PROPERTY(int transitionMs READ transitionMs WRITE setTransitionMs NOTIFY settingsChanged)

  // --- вычисленное
  Q_PROPERTY(bool dark READ dark NOTIFY schemeChanged)
  Q_PROPERTY(QColor sourceColor READ sourceColor NOTIFY schemeChanged)
  Q_PROPERTY(QVariantList wallpaperColors READ wallpaperColors NOTIFY wallpaperColorsChanged)
  Q_PROPERTY(QVariantList presetColors READ presetColors CONSTANT)
  Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
  Q_PROPERTY(QString iconFont READ iconFont CONSTANT)

  // --- роли Material 3
  Q_PROPERTY(QColor primary READ primary NOTIFY colorsChanged)
  Q_PROPERTY(QColor onPrimary READ onPrimary NOTIFY colorsChanged)
  Q_PROPERTY(QColor primaryContainer READ primaryContainer NOTIFY colorsChanged)
  Q_PROPERTY(QColor onPrimaryContainer READ onPrimaryContainer NOTIFY colorsChanged)
  Q_PROPERTY(QColor secondary READ secondary NOTIFY colorsChanged)
  Q_PROPERTY(QColor onSecondary READ onSecondary NOTIFY colorsChanged)
  Q_PROPERTY(QColor secondaryContainer READ secondaryContainer NOTIFY colorsChanged)
  Q_PROPERTY(QColor onSecondaryContainer READ onSecondaryContainer NOTIFY colorsChanged)
  Q_PROPERTY(QColor tertiary READ tertiary NOTIFY colorsChanged)
  Q_PROPERTY(QColor onTertiary READ onTertiary NOTIFY colorsChanged)
  Q_PROPERTY(QColor tertiaryContainer READ tertiaryContainer NOTIFY colorsChanged)
  Q_PROPERTY(QColor onTertiaryContainer READ onTertiaryContainer NOTIFY colorsChanged)
  Q_PROPERTY(QColor error READ error NOTIFY colorsChanged)
  Q_PROPERTY(QColor onError READ onError NOTIFY colorsChanged)
  Q_PROPERTY(QColor errorContainer READ errorContainer NOTIFY colorsChanged)
  Q_PROPERTY(QColor onErrorContainer READ onErrorContainer NOTIFY colorsChanged)
  Q_PROPERTY(QColor surface READ surface NOTIFY colorsChanged)
  Q_PROPERTY(QColor onSurface READ onSurface NOTIFY colorsChanged)
  Q_PROPERTY(QColor surfaceVariant READ surfaceVariant NOTIFY colorsChanged)
  Q_PROPERTY(QColor onSurfaceVariant READ onSurfaceVariant NOTIFY colorsChanged)
  Q_PROPERTY(QColor surfaceDim READ surfaceDim NOTIFY colorsChanged)
  Q_PROPERTY(QColor surfaceBright READ surfaceBright NOTIFY colorsChanged)
  Q_PROPERTY(QColor surfaceContainerLowest READ surfaceContainerLowest NOTIFY colorsChanged)
  Q_PROPERTY(QColor surfaceContainerLow READ surfaceContainerLow NOTIFY colorsChanged)
  Q_PROPERTY(QColor surfaceContainer READ surfaceContainer NOTIFY colorsChanged)
  Q_PROPERTY(QColor surfaceContainerHigh READ surfaceContainerHigh NOTIFY colorsChanged)
  Q_PROPERTY(QColor surfaceContainerHighest READ surfaceContainerHighest NOTIFY colorsChanged)
  Q_PROPERTY(QColor outline READ outline NOTIFY colorsChanged)
  Q_PROPERTY(QColor outlineVariant READ outlineVariant NOTIFY colorsChanged)
  Q_PROPERTY(QColor inverseSurface READ inverseSurface NOTIFY colorsChanged)
  Q_PROPERTY(QColor inverseOnSurface READ inverseOnSurface NOTIFY colorsChanged)
  Q_PROPERTY(QColor inversePrimary READ inversePrimary NOTIFY colorsChanged)
  Q_PROPERTY(QColor shadow READ shadow NOTIFY colorsChanged)
  Q_PROPERTY(QColor scrim READ scrim NOTIFY colorsChanged)

 public:
  static Theme* instance();
  static Theme* create(QQmlEngine*, QJSEngine*);

  // Для установщика: без сохранения настроек и с фиксированным цветом.
  void configureFixed(QColor seed);

  QString themeMode() const { return themeMode_; }
  void setThemeMode(const QString& m);
  QString darkFrom() const { return darkFrom_; }
  void setDarkFrom(const QString& t);
  QString darkTo() const { return darkTo_; }
  void setDarkTo(const QString& t);
  QString wallpaperPath() const { return wallpaperPath_; }
  QString sourceType() const { return sourceType_; }
  void setSourceType(const QString& t);
  QColor customColor() const { return customColor_; }
  void setCustomColor(const QColor& c);
  bool amoled() const { return amoled_; }
  void setAmoled(bool on);
  double contrast() const { return contrast_; }
  void setContrast(double c);
  int transitionMs() const { return transitionMs_; }
  void setTransitionMs(int ms);

  bool dark() const { return dark_; }
  QColor sourceColor() const { return QColor::fromRgba(source_); }
  QVariantList wallpaperColors() const;
  QVariantList presetColors() const;
  QString fontFamily() const { return fontFamily_; }
  QString iconFont() const { return iconFont_; }

  // Цвет из тональной палитры произвольного оттенка (для групп вкладок и т.п.).
  Q_INVOKABLE QColor toneOf(double hue, double chroma, double tone) const;
  // Оттенок (0…360) и насыщенность цвета в модели HCT — для ползунков своего цвета.
  Q_INVOKABLE double hueOf(const QColor& c) const;
  Q_INVOKABLE double chromaOf(const QColor& c) const;
  // Цвет группы вкладок №index, подогнанный под светлую/тёмную тему.
  Q_INVOKABLE QColor groupColor(int index) const;
  Q_INVOKABLE int groupColorCount() const { return 8; }
  Q_INVOKABLE void refreshWallpaper();

#define CRUMB_ROLE(name, idx) \
  QColor name() const { return current_[idx]; }
  CRUMB_ROLE(primary, 0) CRUMB_ROLE(onPrimary, 1) CRUMB_ROLE(primaryContainer, 2)
  CRUMB_ROLE(onPrimaryContainer, 3) CRUMB_ROLE(secondary, 4) CRUMB_ROLE(onSecondary, 5)
  CRUMB_ROLE(secondaryContainer, 6) CRUMB_ROLE(onSecondaryContainer, 7) CRUMB_ROLE(tertiary, 8)
  CRUMB_ROLE(onTertiary, 9) CRUMB_ROLE(tertiaryContainer, 10) CRUMB_ROLE(onTertiaryContainer, 11)
  CRUMB_ROLE(error, 12) CRUMB_ROLE(onError, 13) CRUMB_ROLE(errorContainer, 14)
  CRUMB_ROLE(onErrorContainer, 15) CRUMB_ROLE(surface, 16) CRUMB_ROLE(onSurface, 17)
  CRUMB_ROLE(surfaceVariant, 18) CRUMB_ROLE(onSurfaceVariant, 19) CRUMB_ROLE(surfaceDim, 20)
  CRUMB_ROLE(surfaceBright, 21) CRUMB_ROLE(surfaceContainerLowest, 22)
  CRUMB_ROLE(surfaceContainerLow, 23) CRUMB_ROLE(surfaceContainer, 24)
  CRUMB_ROLE(surfaceContainerHigh, 25) CRUMB_ROLE(surfaceContainerHighest, 26)
  CRUMB_ROLE(outline, 27) CRUMB_ROLE(outlineVariant, 28) CRUMB_ROLE(inverseSurface, 29)
  CRUMB_ROLE(inverseOnSurface, 30) CRUMB_ROLE(inversePrimary, 31) CRUMB_ROLE(shadow, 32)
  CRUMB_ROLE(scrim, 33)
#undef CRUMB_ROLE

 signals:
  void settingsChanged();
  void schemeChanged();
  void colorsChanged();
  void wallpaperColorsChanged();

 private:
  Theme();
  void load();   // старое хранение в ini (больше не используется браузером)
  void save();
  void rebuild(bool animate = true);
  bool systemDark() const;

  QString themeMode_ = QStringLiteral("system");
  QString darkFrom_ = QStringLiteral("21:00");
  QString darkTo_ = QStringLiteral("07:00");
  QTimer* scheduleTimer_ = nullptr;
  QString sourceType_ = QStringLiteral("wallpaper");
  QColor customColor_ = QColor(0x67, 0x50, 0xA4);
  bool amoled_ = false;
  double contrast_ = 0.0;
  int transitionMs_ = 450;
  bool persistent_ = false;   // сохраняет хранилище браузера (SQLite), а не сама тема

  bool dark_ = false;
  uint32_t source_ = 0xFF4285F4;
  QVector<uint32_t> wallpaper_;
  QString wallpaperPath_;
  QDateTime wallpaperStamp_;

  QVector<QColor> from_, to_, current_;
  QVariantAnimation* anim_ = nullptr;
  QTimer* wallpaperPoll_ = nullptr;
  QString fontFamily_, iconFont_;
};

// Скруглённые углы и тень Windows 11 для окон без системной рамки.
class WindowEffects : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
 public:
  Q_INVOKABLE void roundCorners(QWindow* window, bool round = true);
};

}  // namespace crumb
