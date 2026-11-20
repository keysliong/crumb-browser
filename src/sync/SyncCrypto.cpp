#include "sync/SyncCrypto.h"

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
// TODO(Linux, этап 9): OpenSSL/libsecret.
QByteArray randomBytes(int) { return {}; }
QByteArray deriveKey(const QByteArray&, const QByteArray&, int) { return {}; }
QByteArray seal(const QByteArray&, const QByteArray&) { return {}; }
QByteArray open(const QByteArray&, const QByteArray&, bool* ok) { if (ok) *ok = false; return {}; }
QByteArray protect(const QByteArray& d) { return d; }
QByteArray unprotect(const QByteArray& d) { return d; }
#endif

}  // namespace crumb::crypto
