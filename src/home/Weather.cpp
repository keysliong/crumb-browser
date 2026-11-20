#include "home/Weather.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>
#include <algorithm>
#include <climits>
#include <cmath>
#include <memory>

#include "storage/SettingsStore.h"

namespace crumb {

namespace {

// Коды погоды WMO → описание и значок.
void describe(int code, bool day, QString& desc, QString& emoji) {
  struct W { int from, to; const char* d; const char* e; };
  static const W table[] = {
      {0, 0, "Ясно", "☀️"},
      {1, 1, "Преимущественно ясно", "🌤️"},
      {2, 2, "Переменная облачность", "⛅"},
      {3, 3, "Пасмурно", "☁️"},
      {45, 48, "Туман", "🌫️"},
      {51, 57, "Морось", "🌦️"},
      {61, 67, "Дождь", "🌧️"},
      {71, 77, "Снег", "🌨️"},
      {80, 82, "Ливень", "🌧️"},
      {85, 86, "Снегопад", "❄️"},
      {95, 99, "Гроза", "⛈️"},
  };
  desc = QStringLiteral("—");
  emoji = QStringLiteral("🌡️");
  for (const auto& w : table) {
    if (code >= w.from && code <= w.to) {
      desc = QString::fromUtf8(w.d);
      emoji = QString::fromUtf8(w.e);
      break;
    }
  }
  if (!day && code <= 1) emoji = QStringLiteral("🌙");
}

// Для сравнения названий: нижний регистр, без диакритики и знаков препинания
// («Środa» == «sroda»).
QString normalize(const QString& s) {
  const QString d = s.toLower().normalized(QString::NormalizationForm_D);
  QString out;
  out.reserve(d.size());
  for (const QChar c : d) {
    if (c.category() == QChar::Mark_NonSpacing) continue;
    if (c.isLetterOrNumber()) out += c;
    else if (!out.endsWith(' ')) out += ' ';
  }
  return out.trimmed();
}

int levenshtein(const QString& a, const QString& b) {
  std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
  for (int j = 0; j <= b.size(); ++j) prev[j] = j;
  for (int i = 1; i <= a.size(); ++i) {
    cur[0] = i;
    for (int j = 1; j <= b.size(); ++j)
      cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
    std::swap(prev, cur);
  }
  return prev[b.size()];
}

QString joinLabel(const QStringList& parts) {
  QStringList out;
  for (const QString& p : parts)
    if (!p.isEmpty() && !out.contains(p)) out << p;
  return out.join(", ");
}

}  // namespace

Weather* Weather::instance() {
  static Weather* self = new Weather();
  return self;
}

Weather* Weather::create(QQmlEngine*, QJSEngine*) {
  QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
  return instance();
}

Weather::Weather() {
  net_ = new QNetworkAccessManager(this);
  timer_ = new QTimer(this);
  connect(timer_, &QTimer::timeout, this, &Weather::refresh);
  timer_->start(30 * 60 * 1000);
  connect(SettingsStore::instance(), &SettingsStore::settingChanged, this, [this](const QString& key) {
    if (key == QLatin1String("homeCity")) refresh();
  });
  QTimer::singleShot(0, this, &Weather::refresh);
}

QNetworkReply* Weather::get(const QUrl& url) {
  QNetworkRequest req(url);
  // OpenStreetMap просит указывать приложение в User-Agent.
  req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Crumb/%1 (desktop browser)").arg(CRUMB_VERSION));
  req.setRawHeader("Accept-Language", "ru,en;q=0.7");
  return net_->get(req);
}

void Weather::fail(const QString& message) {
  loading_ = false;
  ready_ = false;
  error_ = message;
  emit changed();
}

// ---------------------------------------------------------------- поиск мест

void Weather::searchOpenMeteo(const QString& q, int count, std::function<void(QList<Place>)> done,
                              const QString& lang) {
  QUrl url("https://geocoding-api.open-meteo.com/v1/search");
  QUrlQuery query;
  query.addQueryItem("name", q);
  query.addQueryItem("count", QString::number(count));
  query.addQueryItem("language", lang);
  url.setQuery(query);
  QNetworkReply* reply = get(url);
  connect(reply, &QNetworkReply::finished, this, [reply, done] {
    reply->deleteLater();
    QList<Place> out;
    if (reply->error() == QNetworkReply::NoError) {
      for (const auto& v : QJsonDocument::fromJson(reply->readAll()).object().value("results").toArray()) {
        const QJsonObject r = v.toObject();
        Place p;
        p.name = r.value("name").toString();
        p.label = joinLabel({p.name, r.value("admin1").toString(), r.value("country").toString()});
        p.lat = r.value("latitude").toDouble();
        p.lon = r.value("longitude").toDouble();
        out << p;
      }
    }
    done(out);
  });
}

void Weather::searchNominatim(const QString& q, int count, std::function<void(QList<Place>)> done,
                              const QString& lang) {
  QUrl url("https://nominatim.openstreetmap.org/search");
  QUrlQuery query;
  query.addQueryItem("q", q);
  query.addQueryItem("format", "jsonv2");
  query.addQueryItem("addressdetails", "1");
  query.addQueryItem("limit", QString::number(count));
  query.addQueryItem("accept-language", lang);
  url.setQuery(query);
  QNetworkReply* reply = get(url);
  connect(reply, &QNetworkReply::finished, this, [reply, done] {
    reply->deleteLater();
    QList<Place> out;
    if (reply->error() == QNetworkReply::NoError) {
      for (const auto& v : QJsonDocument::fromJson(reply->readAll()).array()) {
        const QJsonObject r = v.toObject();
        const QJsonObject a = r.value("address").toObject();
        Place p;
        p.name = r.value("name").toString();
        for (const char* k : {"city", "town", "village", "hamlet", "municipality"})
          if (p.name.isEmpty()) p.name = a.value(k).toString();
        if (p.name.isEmpty()) p.name = r.value("display_name").toString().section(',', 0, 0);
        p.label = joinLabel({p.name, a.value("state").toString(), a.value("country").toString()});
        p.lat = r.value("lat").toString().toDouble();
        p.lon = r.value("lon").toString().toDouble();
        out << p;
      }
    }
    done(out);
  });
}

void Weather::suggest(const QString& text) {
  const QString q = text.trimmed();
  const int serial = ++suggestSerial_;
  if (q.size() < 2) {
    clearSuggestions();
    return;
  }
  // Два источника параллельно; Open-Meteo — первым в списке.
  auto om = std::make_shared<QList<Place>>();
  auto osm = std::make_shared<QList<Place>>();
  auto left = std::make_shared<int>(2);
  suggestPending_ = 2;
  emit suggestionsChanged();
  auto finish = [this, serial, om, osm, left] {
    if (--*left > 0 || serial != suggestSerial_) return;
    QList<Place> merged;
    auto add = [&merged](const Place& p) {
      for (const Place& m : merged)
        if (std::abs(m.lat - p.lat) < 0.05 && std::abs(m.lon - p.lon) < 0.05) return;   // то же место
      merged << p;
    };
    for (const Place& p : *om) add(p);
    for (const Place& p : *osm) add(p);
    while (merged.size() > 8) merged.removeLast();
    suggestionPlaces_ = merged;
    suggestions_.clear();
    for (const Place& p : merged) suggestions_ << QVariantMap{{"name", p.name}, {"label", p.label}};
    suggestPending_ = 0;
    emit suggestionsChanged();
  };
  const QString lang = q.contains(QRegularExpression("[\\x{0400}-\\x{04FF}]")) ? QStringLiteral("ru") : QStringLiteral("en");
  searchOpenMeteo(q, 6, [om, finish](QList<Place> r) { *om = r; finish(); }, lang);
  searchNominatim(q, 6, [osm, finish](QList<Place> r) { *osm = r; finish(); }, lang);
}

void Weather::clearSuggestions() {
  ++suggestSerial_;
  suggestions_.clear();
  suggestionPlaces_.clear();
  suggestPending_ = 0;
  emit suggestionsChanged();
}

void Weather::choose(int index) {
  if (index < 0 || index >= suggestionPlaces_.size()) return;
  const Place p = suggestionPlaces_.at(index);
  clearSuggestions();
  auto* s = SettingsStore::instance();
  s->set("homeLat", p.lat);
  s->set("homeLon", p.lon);
  s->set("homeCityResolved", p.label);
  s->set("homeCity", p.label);   // вызовет refresh() — координаты уже известны
}

// ------------------------------------------------------------------ погода

void Weather::refresh() {
  auto* s = SettingsStore::instance();
  query_ = s->get("homeCity").toString().trimmed();
  if (query_.isEmpty()) {
    ready_ = false;
    error_.clear();
    emit changed();
    return;
  }
  loading_ = true;
  emit changed();
  // Место уже найдено раньше — сразу за прогнозом.
  const double lat = s->get("homeLat").toDouble(), lon = s->get("homeLon").toDouble();
  if (s->get("homeCityResolved").toString() == query_ && std::abs(lat) <= 90 && std::abs(lon) <= 180) {
    fetchForecast(lat, lon, query_.section(',', 0, 0));
    return;
  }
  geocode(query_);
}

// Свободный текст → место: точно → OpenStreetMap → по первому слову с поправкой опечаток.
void Weather::geocode(const QString& text) {
  const QString asked = text;
  auto stillAsked = [this, asked] { return asked == query_; };

  auto fuzzyPick = [asked](const QList<Place>& list, Place* best) {
    const QString want = normalize(asked);
    int bestDist = INT_MAX;
    for (const Place& p : list) {
      const QString candidates[] = {normalize(p.name), normalize(p.label)};
      for (const QString& c : candidates) {
        // сравниваем с началом подписи той же длины, что и запрос
        const int d = levenshtein(want, c.left(want.size() + 2));
        if (d < bestDist) { bestDist = d; *best = p; }
      }
    }
    return bestDist <= std::max(3, int(want.size() * 0.3));
  };

  const QString first = asked.section(QRegularExpression("[\\s,]+"), 0, 0, QString::SectionSkipEmpty);
  // Сравниваем в том же алфавите, что и ввод: латиница — местные/английские
  // названия («Środa Wielkopolska»), кириллица — русские.
  const bool cyrillic = asked.contains(QRegularExpression("[\\x{0400}-\\x{04FF}]"));
  const QString lang = cyrillic ? QStringLiteral("ru") : QStringLiteral("en");

  searchOpenMeteo(asked, 1, [=](QList<Place> r) {
    if (!stillAsked()) return;
    if (!r.isEmpty()) return usePlace(r.first(), true);
    searchNominatim(asked, 1, [=](QList<Place> r2) {
      if (!stillAsked()) return;
      if (!r2.isEmpty()) return usePlace(r2.first(), true);
      if (first.isEmpty() || first == asked) return fail(tr("Место «%1» не найдено").arg(asked));
      searchOpenMeteo(first, 20, [=](QList<Place> r3) {
        if (!stillAsked()) return;
        Place p;
        if (fuzzyPick(r3, &p)) return usePlace(p, true);
        searchNominatim(first, 10, [=](QList<Place> r4) {
          if (!stillAsked()) return;
          Place p2;
          if (fuzzyPick(r4, &p2)) return usePlace(p2, true);
          fail(tr("Место «%1» не найдено — начните вводить и выберите из подсказок").arg(asked));
        }, lang);
      }, lang);
    });
  });
}

void Weather::usePlace(const Place& p, bool remember) {
  if (remember) {
    // Запоминаем координаты для этого текста — в следующий раз без поиска.
    auto* s = SettingsStore::instance();
    s->set("homeLat", p.lat);
    s->set("homeLon", p.lon);
    s->set("homeCityResolved", query_);
  }
  fetchForecast(p.lat, p.lon, p.name);
}

void Weather::fetchForecast(double lat, double lon, const QString& name) {
  QUrl url("https://api.open-meteo.com/v1/forecast");
  QUrlQuery q;
  q.addQueryItem("latitude", QString::number(lat, 'f', 4));
  q.addQueryItem("longitude", QString::number(lon, 'f', 4));
  q.addQueryItem("current", "temperature_2m,weather_code,is_day");
  q.addQueryItem("daily", "temperature_2m_max,temperature_2m_min");
  q.addQueryItem("forecast_days", "1");
  q.addQueryItem("timezone", "auto");
  url.setQuery(q);
  QNetworkReply* reply = get(url);
  connect(reply, &QNetworkReply::finished, this, [this, reply, name] {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) return fail(tr("Нет связи с сервисом погоды"));
    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonObject cur = root.value("current").toObject();
    const QJsonObject daily = root.value("daily").toObject();
    temp_ = int(std::lround(cur.value("temperature_2m").toDouble()));
    high_ = int(std::lround(daily.value("temperature_2m_max").toArray().at(0).toDouble()));
    low_ = int(std::lround(daily.value("temperature_2m_min").toArray().at(0).toDouble()));
    describe(cur.value("weather_code").toInt(), cur.value("is_day").toInt() == 1, desc_, emoji_);
    city_ = name;
    error_.clear();
    loading_ = false;
    ready_ = true;
    emit changed();
  });
}

}  // namespace crumb
