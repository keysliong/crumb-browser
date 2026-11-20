#include "MaterialColor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace crumb::mcu {

namespace {

constexpr double kPi = 3.14159265358979323846;

int red(Argb c) { return (c >> 16) & 0xFF; }
int green(Argb c) { return (c >> 8) & 0xFF; }
int blue(Argb c) { return c & 0xFF; }
Argb argbFromRgb(int r, int g, int b) {
  return 0xFF000000u | (uint32_t(r & 255) << 16) | (uint32_t(g & 255) << 8) | uint32_t(b & 255);
}

double sanitizeDegrees(double d) {
  d = std::fmod(d, 360.0);
  return d < 0 ? d + 360.0 : d;
}
int signum(double x) { return x < 0 ? -1 : (x == 0 ? 0 : 1); }
double lerp(double a, double b, double t) { return (1.0 - t) * a + t * b; }

// sRGB канал 0..255 → линейный 0..100
double linearized(int c) {
  const double n = c / 255.0;
  return (n <= 0.040449936 ? n / 12.92 : std::pow((n + 0.055) / 1.055, 2.4)) * 100.0;
}
// линейный 0..100 → sRGB 0..255
int delinearized(double l) {
  const double n = l / 100.0;
  const double d = n <= 0.0031308 ? n * 12.92 : 1.055 * std::pow(n, 1.0 / 2.4) - 0.055;
  return std::clamp(int(std::lround(d * 255.0)), 0, 255);
}

double labF(double t) {
  constexpr double e = 216.0 / 24389.0, kappa = 24389.0 / 27.0;
  return t > e ? std::cbrt(t) : (kappa * t + 16.0) / 116.0;
}
double labInvf(double ft) {
  constexpr double e = 216.0 / 24389.0, kappa = 24389.0 / 27.0;
  const double ft3 = ft * ft * ft;
  return ft3 > e ? ft3 : (116.0 * ft - 16.0) / kappa;
}
double yFromLstar(double l) { return 100.0 * labInvf((l + 16.0) / 116.0); }
double lstarFromY(double y) { return labF(y / 100.0) * 116.0 - 16.0; }

Argb argbFromLstar(double l) {
  const int c = delinearized(yFromLstar(l));
  return argbFromRgb(c, c, c);
}

Argb argbFromXyz(double x, double y, double z) {
  const double r = 3.2413774792388685 * x - 1.5376652402851851 * y - 0.49885366846268053 * z;
  const double g = -0.9691452513005321 * x + 1.8758853451067872 * y + 0.04156585616912061 * z;
  const double b = 0.05562093689691305 * x - 0.20395524564742123 * y + 1.0571799111220335 * z;
  return argbFromRgb(delinearized(r), delinearized(g), delinearized(b));
}

// Условия наблюдения по умолчанию (sRGB, D65, серый фон L*=50).
struct ViewingConditions {
  double n, aw, nbb, ncb, c, nc, fl, flRoot, z;
  std::array<double, 3> rgbD;

  static const ViewingConditions& standard() {
    static const ViewingConditions vc = make();
    return vc;
  }

  static ViewingConditions make() {
    const double wp[3] = {95.047, 100.0, 108.883};
    const double adaptingLuminance = (200.0 / kPi) * yFromLstar(50.0) / 100.0;
    const double backgroundLstar = 50.0, surround = 2.0;
    const double rW = wp[0] * 0.401288 + wp[1] * 0.650173 + wp[2] * -0.051461;
    const double gW = wp[0] * -0.250268 + wp[1] * 1.204414 + wp[2] * 0.045854;
    const double bW = wp[0] * -0.002079 + wp[1] * 0.048952 + wp[2] * 0.953127;
    ViewingConditions v{};
    const double f = 0.8 + surround / 10.0;
    v.c = f >= 0.9 ? lerp(0.59, 0.69, (f - 0.9) * 10.0) : lerp(0.525, 0.59, (f - 0.8) * 10.0);
    double d = f * (1.0 - (1.0 / 3.6) * std::exp((-adaptingLuminance - 42.0) / 92.0));
    d = std::clamp(d, 0.0, 1.0);
    v.nc = f;
    v.rgbD = {d * (100.0 / rW) + 1.0 - d, d * (100.0 / gW) + 1.0 - d, d * (100.0 / bW) + 1.0 - d};
    const double k = 1.0 / (5.0 * adaptingLuminance + 1.0);
    const double k4 = k * k * k * k, k4F = 1.0 - k4;
    v.fl = k4 * adaptingLuminance + 0.1 * k4F * k4F * std::cbrt(5.0 * adaptingLuminance);
    v.n = yFromLstar(backgroundLstar) / wp[1];
    v.z = 1.48 + std::sqrt(v.n);
    v.nbb = 0.725 / std::pow(v.n, 0.2);
    v.ncb = v.nbb;
    const double rgbW[3] = {rW, gW, bW};
    double rgbA[3];
    for (int i = 0; i < 3; ++i) {
      const double fct = std::pow(v.fl * v.rgbD[i] * rgbW[i] / 100.0, 0.42);
      rgbA[i] = 400.0 * fct / (fct + 27.13);
    }
    v.aw = (2.0 * rgbA[0] + rgbA[1] + 0.05 * rgbA[2]) * v.nbb;
    v.flRoot = std::pow(v.fl, 0.25);
    return v;
  }
};

void fillUcs(Cam16& cam) {
  const double hr = cam.hue * kPi / 180.0;
  cam.jstar = (1.0 + 100.0 * 0.007) * cam.j / (1.0 + 0.007 * cam.j);
  const double mstar = 1.0 / 0.0228 * std::log1p(0.0228 * cam.m);
  cam.astar = mstar * std::cos(hr);
  cam.bstar = mstar * std::sin(hr);
}

}  // namespace

// ------------------------------------------------------------------ CAM16

Cam16 Cam16::fromArgb(Argb argb) {
  const auto& vc = ViewingConditions::standard();
  const double r = linearized(red(argb)), g = linearized(green(argb)), b = linearized(blue(argb));
  const double x = 0.41233895 * r + 0.35762064 * g + 0.18051042 * b;
  const double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  const double z = 0.01932141 * r + 0.11916382 * g + 0.95034478 * b;

  const double rC = 0.401288 * x + 0.650173 * y - 0.051461 * z;
  const double gC = -0.250268 * x + 1.204414 * y + 0.045854 * z;
  const double bC = -0.002079 * x + 0.048952 * y + 0.953127 * z;
  const double rD = vc.rgbD[0] * rC, gD = vc.rgbD[1] * gC, bD = vc.rgbD[2] * bC;
  const double rAF = std::pow(vc.fl * std::abs(rD) / 100.0, 0.42);
  const double gAF = std::pow(vc.fl * std::abs(gD) / 100.0, 0.42);
  const double bAF = std::pow(vc.fl * std::abs(bD) / 100.0, 0.42);
  const double rA = signum(rD) * 400.0 * rAF / (rAF + 27.13);
  const double gA = signum(gD) * 400.0 * gAF / (gAF + 27.13);
  const double bA = signum(bD) * 400.0 * bAF / (bAF + 27.13);

  const double a = (11.0 * rA + -12.0 * gA + bA) / 11.0;
  const double bb = (rA + gA - 2.0 * bA) / 9.0;
  const double u = (20.0 * rA + 20.0 * gA + 21.0 * bA) / 20.0;
  const double p2 = (40.0 * rA + 20.0 * gA + bA) / 20.0;
  const double hue = sanitizeDegrees(std::atan2(bb, a) * 180.0 / kPi);
  const double ac = p2 * vc.nbb;

  Cam16 cam;
  cam.hue = hue;
  cam.j = 100.0 * std::pow(ac / vc.aw, vc.c * vc.z);
  cam.q = 4.0 / vc.c * std::sqrt(cam.j / 100.0) * (vc.aw + 4.0) * vc.flRoot;
  const double huePrime = hue < 20.14 ? hue + 360.0 : hue;
  const double eHue = 0.25 * (std::cos(huePrime * kPi / 180.0 + 2.0) + 3.8);
  const double p1 = 50000.0 / 13.0 * eHue * vc.nc * vc.ncb;
  const double t = p1 * std::hypot(a, bb) / (u + 0.305);
  const double alpha = std::pow(1.64 - std::pow(0.29, vc.n), 0.73) * std::pow(t, 0.9);
  cam.chroma = alpha * std::sqrt(cam.j / 100.0);
  cam.m = cam.chroma * vc.flRoot;
  cam.s = 50.0 * std::sqrt((alpha * vc.c) / (vc.aw + 4.0));
  fillUcs(cam);
  return cam;
}

Cam16 Cam16::fromJch(double j, double c, double h) {
  const auto& vc = ViewingConditions::standard();
  Cam16 cam;
  cam.j = j;
  cam.chroma = c;
  cam.hue = h;
  cam.q = 4.0 / vc.c * std::sqrt(j / 100.0) * (vc.aw + 4.0) * vc.flRoot;
  cam.m = c * vc.flRoot;
  const double alpha = j == 0 ? 0 : c / std::sqrt(j / 100.0);
  cam.s = 50.0 * std::sqrt((alpha * vc.c) / (vc.aw + 4.0));
  fillUcs(cam);
  return cam;
}

Argb Cam16::toArgb() const {
  const auto& vc = ViewingConditions::standard();
  const double alpha = (chroma == 0.0 || j == 0.0) ? 0.0 : chroma / std::sqrt(j / 100.0);
  const double t = std::pow(alpha / std::pow(1.64 - std::pow(0.29, vc.n), 0.73), 1.0 / 0.9);
  const double hRad = hue * kPi / 180.0;
  const double eHue = 0.25 * (std::cos(hRad + 2.0) + 3.8);
  const double ac = vc.aw * std::pow(j / 100.0, 1.0 / vc.c / vc.z);
  const double p1 = eHue * (50000.0 / 13.0) * vc.nc * vc.ncb;
  const double p2 = ac / vc.nbb;
  const double hSin = std::sin(hRad), hCos = std::cos(hRad);
  const double gamma = 23.0 * (p2 + 0.305) * t / (23.0 * p1 + 11.0 * t * hCos + 108.0 * t * hSin);
  const double a = gamma * hCos, b = gamma * hSin;
  const double rA = (460.0 * p2 + 451.0 * a + 288.0 * b) / 1403.0;
  const double gA = (460.0 * p2 - 891.0 * a - 261.0 * b) / 1403.0;
  const double bA = (460.0 * p2 - 220.0 * a - 6300.0 * b) / 1403.0;
  auto inv = [&](double cA) {
    const double base = std::max(0.0, (27.13 * std::abs(cA)) / (400.0 - std::abs(cA)));
    return signum(cA) * (100.0 / vc.fl) * std::pow(base, 1.0 / 0.42);
  };
  const double rF = inv(rA) / vc.rgbD[0], gF = inv(gA) / vc.rgbD[1], bF = inv(bA) / vc.rgbD[2];
  const double x = 1.86206786 * rF - 1.01125463 * gF + 0.14918677 * bF;
  const double y = 0.38752654 * rF + 0.62144744 * gF - 0.00897398 * bF;
  const double z = -0.01584150 * rF - 0.03412294 * gF + 1.04996444 * bF;
  return argbFromXyz(x, y, z);
}

double Cam16::distance(const Cam16& o) const {
  const double dJ = jstar - o.jstar, dA = astar - o.astar, dB = bstar - o.bstar;
  return 1.41 * std::pow(std::sqrt(dJ * dJ + dA * dA + dB * dB), 0.63);
}

double lstarFromArgb(Argb argb) {
  const double y = 0.2126 * linearized(red(argb)) + 0.7152 * linearized(green(argb)) +
                   0.0722 * linearized(blue(argb));
  return lstarFromY(y);
}

// ------------------------------------------------------------------- HCT

namespace {

// Двоичный поиск по J (светлоте CAM16), пока L* результата не совпадёт с тоном.
bool findCamByJ(double hue, double chroma, double tone, Cam16& out) {
  double low = 0.0, high = 100.0, bestdL = 1000.0, bestdE = 1000.0;
  bool found = false;
  while (std::abs(low - high) > 0.01) {
    const double mid = low + (high - low) / 2.0;
    const Argb clipped = Cam16::fromJch(mid, chroma, hue).toArgb();
    const double clippedLstar = lstarFromArgb(clipped);
    const double dL = std::abs(tone - clippedLstar);
    if (dL < 0.2) {
      const Cam16 camClipped = Cam16::fromArgb(clipped);
      const double dE = camClipped.distance(Cam16::fromJch(camClipped.j, camClipped.chroma, hue));
      if (dE <= 1.0 && dE <= bestdE) {
        bestdL = dL;
        bestdE = dE;
        out = camClipped;
        found = true;
      }
    }
    if (bestdL == 0.0 && bestdE == 0.0) break;
    if (clippedLstar < tone) low = mid; else high = mid;
  }
  return found;
}

}  // namespace

Argb hctToArgb(double hue, double chroma, double tone) {
  tone = std::clamp(tone, 0.0, 100.0);
  if (chroma < 1.0 || std::round(tone) <= 0.0 || std::round(tone) >= 100.0)
    return argbFromLstar(tone);
  hue = sanitizeDegrees(hue);

  double high = chroma, mid = chroma, low = 0.0;
  bool first = true, haveAnswer = false;
  Cam16 answer, possible;
  while (std::abs(low - high) >= 0.4) {
    const bool ok = findCamByJ(hue, mid, tone, possible);
    if (first) {
      if (ok) return possible.toArgb();
      first = false;
    } else if (!ok) {
      high = mid;
    } else {
      answer = possible;
      haveAnswer = true;
      low = mid;
    }
    mid = low + (high - low) / 2.0;
  }
  return haveAnswer ? answer.toArgb() : argbFromLstar(tone);
}

// ---------------------------------------------------------------- схема

QVector<QColor> tonalSpotScheme(Argb source, const SchemeOptions& opt) {
  const Cam16 cam = Cam16::fromArgb(source);
  const TonalPalette p(cam.hue, 36.0);
  const TonalPalette s(cam.hue, 16.0);
  const TonalPalette t(sanitizeDegrees(cam.hue + 60.0), 24.0);
  const TonalPalette n(cam.hue, 6.0);
  const TonalPalette nv(cam.hue, 8.0);
  const TonalPalette e(25.0, 84.0);

  const bool d = opt.dark;
  // Контраст: «акцентные» и «on»-тона уходят дальше от фона.
  const double shift = std::clamp(opt.contrast, -1.0, 1.0) * 10.0;
  auto acc = [&](int light, int dark) { return int(std::lround(d ? dark + shift : light - shift)); };
  auto onc = [&](int light, int dark) { return int(std::lround(d ? dark + shift : light - shift)); };
  auto c = [](Argb a) { return QColor::fromRgba(a); };

  QVector<QColor> r(RoleCount);
  r[Primary] = c(p.tone(acc(40, 80)));
  r[OnPrimary] = c(p.tone(d ? 20 : 100));
  r[PrimaryContainer] = c(p.tone(d ? 30 : 90));
  r[OnPrimaryContainer] = c(p.tone(onc(10, 90)));
  r[Secondary] = c(s.tone(acc(40, 80)));
  r[OnSecondary] = c(s.tone(d ? 20 : 100));
  r[SecondaryContainer] = c(s.tone(d ? 30 : 90));
  r[OnSecondaryContainer] = c(s.tone(onc(10, 90)));
  r[Tertiary] = c(t.tone(acc(40, 80)));
  r[OnTertiary] = c(t.tone(d ? 20 : 100));
  r[TertiaryContainer] = c(t.tone(d ? 30 : 90));
  r[OnTertiaryContainer] = c(t.tone(onc(10, 90)));
  r[Error] = c(e.tone(acc(40, 80)));
  r[OnError] = c(e.tone(d ? 20 : 100));
  r[ErrorContainer] = c(e.tone(d ? 30 : 90));
  r[OnErrorContainer] = c(e.tone(onc(10, 90)));

  if (d && opt.amoled) {
    // AMOLED: фон чисто чёрный, контейнеры чуть светлее для различимости.
    r[Surface] = r[SurfaceDim] = r[SurfaceContainerLowest] = QColor(0, 0, 0);
    r[SurfaceBright] = c(n.tone(18));
    r[SurfaceContainerLow] = c(n.tone(4));
    r[SurfaceContainer] = c(n.tone(6));
    r[SurfaceContainerHigh] = c(n.tone(10));
    r[SurfaceContainerHighest] = c(n.tone(15));
  } else {
    r[Surface] = c(n.tone(d ? 6 : 98));
    r[SurfaceDim] = c(n.tone(d ? 6 : 87));
    r[SurfaceBright] = c(n.tone(d ? 24 : 98));
    r[SurfaceContainerLowest] = c(n.tone(d ? 4 : 100));
    r[SurfaceContainerLow] = c(n.tone(d ? 10 : 96));
    r[SurfaceContainer] = c(n.tone(d ? 12 : 94));
    r[SurfaceContainerHigh] = c(n.tone(d ? 17 : 92));
    r[SurfaceContainerHighest] = c(n.tone(d ? 22 : 90));
  }
  r[OnSurface] = c(n.tone(onc(10, 90)));
  r[SurfaceVariant] = c(nv.tone(d ? 30 : 90));
  r[OnSurfaceVariant] = c(nv.tone(onc(30, 80)));
  r[Outline] = c(nv.tone(onc(50, 60)));
  r[OutlineVariant] = c(nv.tone(d ? 30 : 80));
  r[InverseSurface] = c(n.tone(d ? 90 : 20));
  r[InverseOnSurface] = c(n.tone(d ? 20 : 95));
  r[InversePrimary] = c(p.tone(d ? 40 : 80));
  r[Shadow] = QColor(0, 0, 0);
  r[Scrim] = QColor(0, 0, 0);
  return r;
}

// ------------------------------------------- основной цвет из изображения

QVector<Argb> sourceColorsFromImage(const QImage& src, int count) {
  if (src.isNull()) return {kFallbackSource};
  const QImage img = src.scaled(112, 112, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                         .convertToFormat(QImage::Format_ARGB32);

  // 1. Грубое квантование: 5 бит на канал.
  struct Bucket { double r = 0, g = 0, b = 0; int n = 0; };
  std::map<int, Bucket> buckets;
  int total = 0;
  for (int y = 0; y < img.height(); ++y) {
    const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
    for (int x = 0; x < img.width(); ++x) {
      const QRgb px = line[x];
      if (qAlpha(px) < 255) continue;
      const int key = ((qRed(px) >> 3) << 10) | ((qGreen(px) >> 3) << 5) | (qBlue(px) >> 3);
      Bucket& bk = buckets[key];
      bk.r += qRed(px); bk.g += qGreen(px); bk.b += qBlue(px); ++bk.n;
      ++total;
    }
  }
  if (total == 0) return {kFallbackSource};

  // 2. Доля каждого оттенка (по кругу 360°).
  struct Candidate { Argb argb; Cam16 cam; double proportion; };
  std::vector<Candidate> cands;
  std::array<double, 360> hueProp{};
  for (const auto& [key, bk] : buckets) {
    const Argb a = argbFromRgb(int(bk.r / bk.n), int(bk.g / bk.n), int(bk.b / bk.n));
    const Cam16 cam = Cam16::fromArgb(a);
    const double prop = double(bk.n) / total;
    hueProp[int(std::floor(cam.hue)) % 360] += prop;
    cands.push_back({a, cam, prop});
  }
  std::array<double, 360> excited{};
  for (int i = 0; i < 360; ++i)
    for (int k = i - 14; k < i + 16; ++k) excited[i] += hueProp[(k + 360) % 360];

  // 3. Оценка как в MCU Score: доля оттенка + бонус за насыщенность.
  struct Scored { Argb argb; double hue; double score; };
  std::vector<Scored> scored;
  for (const auto& c : cands) {
    const double prop = excited[int(std::floor(c.cam.hue)) % 360];
    if (c.cam.chroma < 5.0 || prop <= 0.01) continue;
    const double chromaScore = (c.cam.chroma - 48.0) * (c.cam.chroma < 48.0 ? 0.1 : 0.3);
    scored.push_back({c.argb, c.cam.hue, prop * 100.0 * 0.7 + chromaScore});
  }
  std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.score > b.score; });

  // 4. Берём лучшие, заметно различающиеся по оттенку.
  QVector<Argb> out;
  std::vector<double> hues;
  for (const auto& s : scored) {
    bool similar = false;
    for (double h : hues) {
      const double diff = 180.0 - std::abs(std::abs(s.hue - h) - 180.0);
      if (diff < 15.0) { similar = true; break; }
    }
    if (similar) continue;
    out.push_back(s.argb);
    hues.push_back(s.hue);
    if (out.size() >= count) break;
  }
  if (out.isEmpty()) out.push_back(kFallbackSource);
  return out;
}

}  // namespace crumb::mcu
