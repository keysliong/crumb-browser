#pragma once
#include <string>

namespace crumb::winverify {
// Блокирующий вызов — запускать в отдельном потоке. true — человек подтвердил,
// что он владелец учётной записи Windows (Windows Hello или пароль Windows).
bool verify(void* hwnd, const std::wstring& message);
}  // namespace crumb::winverify
