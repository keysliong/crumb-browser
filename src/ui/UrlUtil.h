#pragma once

#include <QString>

namespace crumb::UrlUtil {

// Превращает ввод из адресной строки в URL: «example.com» → https://example.com/,
// «localhost:3000» → http://localhost:3000, «котики» → поисковый запрос.
QString fixup(const QString& input);

// Адрес поиска выбранным в настройках поисковиком.
QString searchUrl(const QString& query);

}  // namespace crumb::UrlUtil
