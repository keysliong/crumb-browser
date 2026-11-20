#include "Prefs.h"

#include <QQmlEngine>
#include <QtEndian>
#include <algorithm>
#include <cmath>

#include "Theme.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace crumb {

Prefs* Prefs::instance() {
  static Prefs* self = new Prefs();
  return self;
}

Prefs* Prefs::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

Prefs::Prefs() = default;

int Prefs::px(double size) const { return int(std::lround(size * textScale)); }

int Prefs::dur(int ms) const {
  if (!animations) return 0;
  return int(std::lround(ms / std::max(0.1, animSpeed)));
}

QString Prefs::font() const {
  return fontFamily.isEmpty() ? Theme::instance()->fontFamily() : fontFamily;
}

// Мягкий «чпок»: синус, скользящий вниз по частоте, с быстрым затуханием.
// Звук синтезируется на лету — никаких файлов в дистрибутиве.
void Prefs::buildSound() {
  const int rate = 44100, samples = rate * 70 / 1000;
  QByteArray pcm(samples * 2, Qt::Uninitialized);
  double phase = 0;
  for (int i = 0; i < samples; ++i) {
    const double t = double(i) / rate;
    const double freq = 380 + 620 * std::exp(-t * 60);
    phase += 2 * 3.14159265358979 * freq / rate;
    const double env = std::min(1.0, t * 900) * std::exp(-t * 55);
    const auto v = qint16(std::lround(std::sin(phase) * env * 26000 * clickVolume));
    qToLittleEndian(v, pcm.data() + i * 2);
  }
  QByteArray w;
  auto u32 = [&w](quint32 v) { char b[4]; qToLittleEndian(v, b); w.append(b, 4); };
  auto u16 = [&w](quint16 v) { char b[2]; qToLittleEndian(v, b); w.append(b, 2); };
  w.append("RIFF");
  u32(36 + pcm.size());
  w.append("WAVEfmt ");
  u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
  w.append("data");
  u32(quint32(pcm.size()));
  w.append(pcm);
  wav_ = w;
  wavVolume_ = clickVolume;
}

void Prefs::click() {
#if defined(_WIN32)
  if (!clickSounds || clickVolume <= 0.01) return;
  if (wavVolume_ != clickVolume) {
    PlaySoundW(nullptr, nullptr, 0);  // остановить, прежде чем менять буфер
    buildSound();
  }
  PlaySoundW(reinterpret_cast<LPCWSTR>(wav_.constData()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#endif
}

}  // namespace crumb
