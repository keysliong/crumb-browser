#pragma once
// Порт ключевых частей Material Color Utilities (Google, Apache 2.0):
// цветовая модель CAM16 и HCT, тональные палитры, схема Material You
// (вариант Tonal Spot) и выбор основного цвета из изображения (quantize + score).

#include <QColor>
#include <QImage>
#include <QVector>
#include <cstdint>

namespace crumb::mcu {

using Argb = uint32_t;

struct Cam16 {
  double hue = 0, chroma = 0, j = 0, q = 0, m = 0, s = 0;
  double jstar = 0, astar = 0, bstar = 0;

  static Cam16 fromArgb(Argb argb);
  static Cam16 fromJch(double j, double c, double h);
  Argb toArgb() const;
  double distance(const Cam16& other) const;
};

// HCT → sRGB. Ищет цвет с нужными оттенком и тоном, уменьшая насыщенность,
// пока цвет не попадёт в sRGB.
Argb hctToArgb(double hue, double chroma, double tone);
double lstarFromArgb(Argb argb);

class TonalPalette {
 public:
  TonalPalette() = default;
  TonalPalette(double hue, double chroma) : hue_(hue), chroma_(chroma) {}
  Argb tone(int t) const { return hctToArgb(hue_, chroma_, t); }
  double hue() const { return hue_; }

 private:
  double hue_ = 0, chroma_ = 0;
};

// Роли цветов Material 3. Порядок важен: Theme использует его как индекс.
enum Role : int {
  Primary, OnPrimary, PrimaryContainer, OnPrimaryContainer,
  Secondary, OnSecondary, SecondaryContainer, OnSecondaryContainer,
  Tertiary, OnTertiary, TertiaryContainer, OnTertiaryContainer,
  Error, OnError, ErrorContainer, OnErrorContainer,
  Surface, OnSurface, SurfaceVariant, OnSurfaceVariant,
  SurfaceDim, SurfaceBright,
  SurfaceContainerLowest, SurfaceContainerLow, SurfaceContainer,
  SurfaceContainerHigh, SurfaceContainerHighest,
  Outline, OutlineVariant, InverseSurface, InverseOnSurface, InversePrimary,
  Shadow, Scrim,
  RoleCount
};

struct SchemeOptions {
  bool dark = false;
  bool amoled = false;      // чисто-чёрные поверхности в тёмной теме
  double contrast = 0.0;    // -1 … 1, сдвигает тоны «on»-цветов и контуров
};

QVector<QColor> tonalSpotScheme(Argb source, const SchemeOptions& opt);

// До `count` подходящих основных цветов изображения, лучший — первым.
QVector<Argb> sourceColorsFromImage(const QImage& image, int count = 4);

constexpr Argb kFallbackSource = 0xFF4285F4;  // как в MCU по умолчанию

}  // namespace crumb::mcu
