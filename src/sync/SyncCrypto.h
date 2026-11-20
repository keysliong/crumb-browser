#pragma once
// Криптография для синхронизации на Windows CNG (bcrypt) и DPAPI — без сторонних библиотек.

#include <QByteArray>

namespace crumb::crypto {

QByteArray randomBytes(int n);
// PBKDF2-HMAC-SHA256 → 32 байта ключа.
QByteArray deriveKey(const QByteArray& passphrase, const QByteArray& salt, int iterations);
// AES-256-GCM. Результат: iv(12) + шифртекст + tag(16).
QByteArray seal(const QByteArray& key, const QByteArray& plain);
// Возвращает пустой массив и ok=false, если ключ неверный или данные повреждены.
QByteArray open(const QByteArray& key, const QByteArray& sealed, bool* ok);
// DPAPI: защитить/раскрыть для текущего пользователя Windows.
QByteArray protect(const QByteArray& data);
QByteArray unprotect(const QByteArray& data);

}  // namespace crumb::crypto
