#include "sync/SyncCrypto.h"

#if !defined(_WIN32)
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#include <dpapi.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#endif

namespace crumb::crypto {

#if defined(_WIN32)
namespace {
constexpr ULONG kIv = 12, kTag = 16;

struct Alg {
  BCRYPT_ALG_HANDLE h = nullptr;
  Alg(const wchar_t* id, ULONG flags = 0) { BCryptOpenAlgorithmProvider(&h, id, nullptr, flags); }
  ~Alg() { if (h) BCryptCloseAlgorithmProvider(h, 0); }
};

BCRYPT_KEY_HANDLE gcmKey(Alg& aes, const QByteArray& key) {
  BCryptSetProperty(aes.h, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
  BCRYPT_KEY_HANDLE k = nullptr;
  BCryptGenerateSymmetricKey(aes.h, &k, nullptr, 0, (PUCHAR)key.constData(), ULONG(key.size()), 0);
  return k;
}
}  // namespace

QByteArray randomBytes(int n) {
  QByteArray out(n, Qt::Uninitialized);
  BCryptGenRandom(nullptr, (PUCHAR)out.data(), ULONG(n), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  return out;
}

QByteArray deriveKey(const QByteArray& pass, const QByteArray& salt, int iterations) {
  Alg sha(BCRYPT_SHA256_ALGORITHM, BCRYPT_ALG_HANDLE_HMAC_FLAG);
  QByteArray key(32, 0);
  if (!sha.h) return {};
  if (BCryptDeriveKeyPBKDF2(sha.h, (PUCHAR)pass.constData(), ULONG(pass.size()), (PUCHAR)salt.constData(),
                            ULONG(salt.size()), ULONGLONG(iterations), (PUCHAR)key.data(), 32, 0) != 0)
    return {};
  return key;
}

QByteArray seal(const QByteArray& key, const QByteArray& plain) {
  Alg aes(BCRYPT_AES_ALGORITHM);
  BCRYPT_KEY_HANDLE k = gcmKey(aes, key);
  if (!k) return {};
  QByteArray iv = randomBytes(kIv), tag(kTag, 0), ct(plain.size(), Qt::Uninitialized);
  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
  BCRYPT_INIT_AUTH_MODE_INFO(info);
  info.pbNonce = (PUCHAR)iv.data();
  info.cbNonce = kIv;
  info.pbTag = (PUCHAR)tag.data();
  info.cbTag = kTag;
  ULONG done = 0;
  const NTSTATUS st = BCryptEncrypt(k, (PUCHAR)plain.constData(), ULONG(plain.size()), &info, nullptr, 0,
                                    (PUCHAR)ct.data(), ULONG(ct.size()), &done, 0);
  BCryptDestroyKey(k);
  if (st != 0) return {};
  return iv + ct + tag;
}

QByteArray open(const QByteArray& key, const QByteArray& sealed, bool* ok) {
  if (ok) *ok = false;
  if (sealed.size() < int(kIv + kTag)) return {};
  Alg aes(BCRYPT_AES_ALGORITHM);
  BCRYPT_KEY_HANDLE k = gcmKey(aes, key);
  if (!k) return {};
  QByteArray iv = sealed.left(kIv), tag = sealed.right(kTag);
  QByteArray ct = sealed.mid(kIv, sealed.size() - kIv - kTag), plain(ct.size(), Qt::Uninitialized);
  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
  BCRYPT_INIT_AUTH_MODE_INFO(info);
  info.pbNonce = (PUCHAR)iv.data();
  info.cbNonce = kIv;
  info.pbTag = (PUCHAR)tag.data();
  info.cbTag = kTag;
  ULONG done = 0;
  const NTSTATUS st = BCryptDecrypt(k, (PUCHAR)ct.data(), ULONG(ct.size()), &info, nullptr, 0,
                                    (PUCHAR)plain.data(), ULONG(plain.size()), &done, 0);
  BCryptDestroyKey(k);
  if (st != 0) return {};
  if (ok) *ok = true;
  return plain;
}

QByteArray protect(const QByteArray& data) {
  DATA_BLOB in{DWORD(data.size()), (BYTE*)data.constData()}, out{};
  if (!CryptProtectData(&in, L"Crumb sync", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
  QByteArray r((const char*)out.pbData, int(out.cbData));
  LocalFree(out.pbData);
  return r;
}

QByteArray unprotect(const QByteArray& data) {
  DATA_BLOB in{DWORD(data.size()), (BYTE*)data.constData()}, out{};
  if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
  QByteArray r((const char*)out.pbData, int(out.cbData));
  LocalFree(out.pbData);
  return r;
}
#else
// Linux: OpenSSL. Формат тот же, что на Windows (PBKDF2-SHA256, AES-256-GCM iv|ct|tag),
// поэтому синхронизация между Windows и Linux совместима.
namespace {
constexpr int kIv = 12, kTag = 16;

// Замена DPAPI: случайный ключ пользователя в файле, доступном только ему (0600).
QByteArray masterKey() {
  static QByteArray key;
  if (!key.isEmpty()) return key;
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Crumb";
  QDir().mkpath(dir);
  QFile f(dir + "/.crumb-key");
  if (f.open(QIODevice::ReadOnly)) key = f.readAll();
  if (key.size() != 32) {
    key = randomBytes(32);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
      f.write(key);
    }
  }
  return key;
}
}  // namespace

QByteArray randomBytes(int n) {
  QByteArray out(n, Qt::Uninitialized);
  if (RAND_bytes(reinterpret_cast<unsigned char*>(out.data()), n) != 1) return {};
  return out;
}

QByteArray deriveKey(const QByteArray& pass, const QByteArray& salt, int iterations) {
  QByteArray key(32, 0);
  if (PKCS5_PBKDF2_HMAC(pass.constData(), int(pass.size()), reinterpret_cast<const unsigned char*>(salt.constData()),
                        int(salt.size()), iterations, EVP_sha256(), 32, reinterpret_cast<unsigned char*>(key.data())) != 1)
    return {};
  return key;
}

QByteArray seal(const QByteArray& key, const QByteArray& plain) {
  if (key.size() != 32) return {};
  QByteArray iv = randomBytes(kIv), tag(kTag, 0), ct(plain.size() + 16, Qt::Uninitialized);
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  int len = 0, total = 0;
  bool ok = ctx && EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIv, nullptr) == 1 &&
            EVP_EncryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.constData()),
                               reinterpret_cast<const unsigned char*>(iv.constData())) == 1 &&
            EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ct.data()), &len,
                              reinterpret_cast<const unsigned char*>(plain.constData()), int(plain.size())) == 1;
  total = len;
  ok = ok && EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ct.data()) + total, &len) == 1;
  total += len;
  ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTag, tag.data()) == 1;
  EVP_CIPHER_CTX_free(ctx);
  if (!ok) return {};
  ct.resize(total);
  return iv + ct + tag;
}

QByteArray open(const QByteArray& key, const QByteArray& sealed, bool* ok) {
  if (ok) *ok = false;
  if (key.size() != 32 || sealed.size() < kIv + kTag) return {};
  const QByteArray iv = sealed.left(kIv), tag = sealed.right(kTag);
  const QByteArray ct = sealed.mid(kIv, sealed.size() - kIv - kTag);
  QByteArray plain(ct.size() + 16, Qt::Uninitialized);
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  int len = 0, total = 0;
  bool good = ctx && EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
              EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIv, nullptr) == 1 &&
              EVP_DecryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.constData()),
                                 reinterpret_cast<const unsigned char*>(iv.constData())) == 1 &&
              EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plain.data()), &len,
                                reinterpret_cast<const unsigned char*>(ct.constData()), int(ct.size())) == 1;
  total = len;
  good = good && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTag, const_cast<char*>(tag.constData())) == 1 &&
         EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plain.data()) + total, &len) == 1;
  total += len;
  EVP_CIPHER_CTX_free(ctx);
  if (!good) return {};
  plain.resize(total);
  if (ok) *ok = true;
  return plain;
}

QByteArray protect(const QByteArray& data) {
  const QByteArray s = seal(masterKey(), data);
  return s.isEmpty() ? QByteArray() : QByteArrayLiteral("CRB1") + s;
}

QByteArray unprotect(const QByteArray& data) {
  if (!data.startsWith("CRB1")) return {};
  bool ok = false;
  const QByteArray plain = open(masterKey(), data.mid(4), &ok);
  return ok ? plain : QByteArray();
}
#endif

}  // namespace crumb::crypto
