#pragma once
// Weather — погода для виджета домашней страницы (QML-синглтон).
//
// Город задаёт пользователь (местоположение не определяется автоматически).
// Найти можно любой город или село:
//   1) подсказки при вводе — Open-Meteo + OpenStreetMap (Nominatim);
//   2) свободный текст: точный поиск Open-Meteo → Nominatim → поиск по первому
//      слову с выбором ближайшего по написанию (опечатки вроде «Wilekopolska»).
// Выбранное место хранится с координатами — повторно не ищется.
// Прогноз — Open-Meteo, обновление раз в 30 минут.

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;
class QJSEngine;
class QTimer;

namespace crumb {

class Weather final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool ready READ ready NOTIFY changed)
  Q_PROPERTY(bool loading READ loading NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString city READ city NOTIFY changed)          // как место назвал сервис
  Q_PROPERTY(int temperature READ temperature NOTIFY changed)
  Q_PROPERTY(int high READ high NOTIFY changed)
  Q_PROPERTY(int low READ low NOTIFY changed)
  Q_PROPERTY(QString description READ description NOTIFY changed)
  Q_PROPERTY(QString emoji READ emoji NOTIFY changed)
  Q_PROPERTY(QVariantList suggestions READ suggestions NOTIFY suggestionsChanged)
  Q_PROPERTY(bool suggesting READ suggesting NOTIFY suggestionsChanged)

 public:
  static Weather* instance();
  static Weather* create(QQmlEngine*, QJSEngine*);

  bool ready() const { return ready_; }
  bool loading() const { return loading_; }
  QString error() const { return error_; }
  QString city() const { return city_; }
  int temperature() const { return temp_; }
  int high() const { return high_; }
  int low() const { return low_; }
  QString description() const { return desc_; }
  QString emoji() const { return emoji_; }
  QVariantList suggestions() const { return suggestions_; }
  bool suggesting() const { return suggestPending_ > 0; }

  Q_INVOKABLE void refresh();
  // Подсказки мест для введённого текста (вызывать с небольшой задержкой при вводе).
  Q_INVOKABLE void suggest(const QString& text);
  // Выбрать место из подсказок: сохраняется название и координаты.
  Q_INVOKABLE void choose(int index);
  Q_INVOKABLE void clearSuggestions();

  struct Place { QString name, label; double lat = 0, lon = 0; };

 signals:
  void changed();
  void suggestionsChanged();

 private:
  Weather();
  QNetworkReply* get(const QUrl& url);
  void searchOpenMeteo(const QString& q, int count, std::function<void(QList<Place>)> done,
                       const QString& lang = QStringLiteral("ru"));
  void searchNominatim(const QString& q, int count, std::function<void(QList<Place>)> done,
                       const QString& lang = QStringLiteral("ru"));
  void geocode(const QString& text);
  void usePlace(const Place& p, bool remember);
  void fetchForecast(double lat, double lon, const QString& name);
  void fail(const QString& message);

  QNetworkAccessManager* net_ = nullptr;
  QTimer* timer_ = nullptr;
  bool ready_ = false, loading_ = false;
  QString error_, city_, desc_, emoji_, query_;
  int temp_ = 0, high_ = 0, low_ = 0;
  QVariantList suggestions_;
  QList<Place> suggestionPlaces_;
  int suggestPending_ = 0;
  int suggestSerial_ = 0;
  bool selfUpdate_ = false;
};

}  // namespace crumb
