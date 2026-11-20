#include "ui/UrlUtil.h"

#include <QHostAddress>
#include <QRegularExpression>
#include <QUrl>

#include "storage/SettingsStore.h"

namespace crumb::UrlUtil {

QString searchUrl(const QString& query) {
  return SettingsStore::instance()->searchUrl(query);   // выбранный в настройках поисковик
}

QString fixup(const QString& raw) {
  const QString input = raw.trimmed();
  if (input.isEmpty()) return {};

  // Явная схема: https://, file://, chrome://, about:, data:, view-source: …
  static const QRegularExpression scheme(
      QStringLiteral("^(https?|file|ftp|chrome|chrome-extension|about|data|view-source|blob|crumb):"),
      QRegularExpression::CaseInsensitiveOption);
  if (scheme.match(input).hasMatch()) return input;

  // Пробел внутри — точно поисковый запрос.
  if (input.contains(QChar(' '))) return searchUrl(input);

  // Путь к локальному файлу: C:\… или \\server\…
  static const QRegularExpression winPath(QStringLiteral(R"(^([a-zA-Z]:[\\/]|\\\\))"));
  if (winPath.match(input).hasMatch()) return QUrl::fromLocalFile(input).toString();

  // Хост без схемы: localhost, IP-адрес или домен с точкой и буквенной зоной.
  const QString hostPart = input.section(QRegularExpression(QStringLiteral("[/?#]")), 0, 0);
  const QString host = hostPart.section(QChar(':'), 0, 0);
  const bool local = host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0;
  const bool ip = !QHostAddress(host).isNull();
  static const QRegularExpression domain(
      QStringLiteral(R"(^([\p{L}\p{N}-]+\.)+[\p{L}]{2,}$)"),
      QRegularExpression::UseUnicodePropertiesOption);
  if (local || ip) return QStringLiteral("http://") + input;
  if (domain.match(host).hasMatch()) return QStringLiteral("https://") + input;

  return searchUrl(input);
}

}  // namespace crumb::UrlUtil
