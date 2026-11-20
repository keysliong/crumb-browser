#pragma once
// Формат архива, приклеенного к концу Crumb-Setup.exe, и общий код чтения/записи.
// Без Qt и без сторонних библиотек: сжатие LZMS через Compression API Windows
// (cabinet.dll, есть начиная с Windows 8).
//
//   [stub.exe][блоки данных…][индекс][трейлер 24 байта]
//   трейлер: u64 смещение индекса, u64 размер индекса, "CRUMBPK1"
//   индекс:  u32 версия, u32 число файлов, u64 общий размер,
//            для файла: u16 длина пути, путь UTF-8 (через «/»), u8 флаги,
//                       u64 размер, u32 число блоков,
//                       для блока: u64 смещение, u32 сжатый размер, u32 исходный, u8 метод

#include <windows.h>
#include <compressapi.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace crumb::payload {

constexpr char kMagic[8] = {'C', 'R', 'U', 'M', 'B', 'P', 'K', '1'};
constexpr uint32_t kVersion = 1;
constexpr uint32_t kChunkSize = 16u << 20;   // 16 МБ — хороший баланс сжатия и прогресса

enum Flags : uint8_t {
  kFlagUi = 1,   // нужен самому установщику (Qt и т.п.) — распаковывается во временную папку
};
enum Method : uint8_t { kStored = 0, kLzms = 1 };

struct Chunk {
  uint64_t offset = 0;
  uint32_t compSize = 0, rawSize = 0;
  uint8_t method = kStored;
};

struct Entry {
  std::string path;   // относительный, через «/»
  uint8_t flags = 0;
  uint64_t size = 0;
  std::vector<Chunk> chunks;
};

struct Index {
  uint64_t totalSize = 0;
  std::vector<Entry> entries;
};

inline std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
  return w;
}

inline std::string narrow(const std::wstring& w) {
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

// ------------------------------------------------------------- сериализация

class Writer {
 public:
  void u8(uint8_t v) { buf.push_back(v); }
  void u16(uint16_t v) { raw(&v, 2); }
  void u32(uint32_t v) { raw(&v, 4); }
  void u64(uint64_t v) { raw(&v, 8); }
  void raw(const void* p, size_t n) {
    const auto* b = static_cast<const uint8_t*>(p);
    buf.insert(buf.end(), b, b + n);
  }
  std::vector<uint8_t> buf;
};

class Reader {
 public:
  Reader(const uint8_t* p, size_t n) : p_(p), end_(p + n) {}
  bool ok() const { return ok_; }
  uint8_t u8() { uint8_t v = 0; raw(&v, 1); return v; }
  uint16_t u16() { uint16_t v = 0; raw(&v, 2); return v; }
  uint32_t u32() { uint32_t v = 0; raw(&v, 4); return v; }
  uint64_t u64() { uint64_t v = 0; raw(&v, 8); return v; }
  void raw(void* out, size_t n) {
    if (size_t(end_ - p_) < n) { ok_ = false; std::memset(out, 0, n); return; }
    std::memcpy(out, p_, n);
    p_ += n;
  }

 private:
  const uint8_t* p_;
  const uint8_t* end_;
  bool ok_ = true;
};

inline std::vector<uint8_t> serializeIndex(const Index& idx) {
  Writer w;
  w.u32(kVersion);
  w.u32(uint32_t(idx.entries.size()));
  w.u64(idx.totalSize);
  for (const auto& e : idx.entries) {
    w.u16(uint16_t(e.path.size()));
    w.raw(e.path.data(), e.path.size());
    w.u8(e.flags);
    w.u64(e.size);
    w.u32(uint32_t(e.chunks.size()));
    for (const auto& c : e.chunks) {
      w.u64(c.offset);
      w.u32(c.compSize);
      w.u32(c.rawSize);
      w.u8(c.method);
    }
  }
  return w.buf;
}

// ------------------------------------------------------------------ чтение

class Archive {
 public:
  ~Archive() { close(); }

  // Открыть exe с приклеенным архивом. false — архива нет или он повреждён.
  bool open(const std::wstring& path) {
    close();
    file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file_, &size) || size.QuadPart < 24) return false;
    uint8_t trailer[24];
    if (!readAt(uint64_t(size.QuadPart) - 24, trailer, 24)) return false;
    if (std::memcmp(trailer + 16, kMagic, 8) != 0) return false;
    uint64_t indexOffset, indexSize;
    std::memcpy(&indexOffset, trailer, 8);
    std::memcpy(&indexSize, trailer + 8, 8);
    if (indexSize > (64u << 20) || indexOffset + indexSize > uint64_t(size.QuadPart)) return false;
    std::vector<uint8_t> buf(static_cast<size_t>(indexSize));
    if (!readAt(indexOffset, buf.data(), buf.size())) return false;

    Reader r(buf.data(), buf.size());
    if (r.u32() != kVersion) return false;
    const uint32_t count = r.u32();
    index_.totalSize = r.u64();
    index_.entries.resize(count);
    for (auto& e : index_.entries) {
      e.path.resize(r.u16());
      r.raw(e.path.data(), e.path.size());
      e.flags = r.u8();
      e.size = r.u64();
      e.chunks.resize(r.u32());
      for (auto& c : e.chunks) {
        c.offset = r.u64();
        c.compSize = r.u32();
        c.rawSize = r.u32();
        c.method = r.u8();
      }
    }
    if (!r.ok()) return false;
    return CreateDecompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &dec_) != FALSE;
  }

  void close() {
    if (dec_) { CloseDecompressor(dec_); dec_ = nullptr; }
    if (file_ != INVALID_HANDLE_VALUE) { CloseHandle(file_); file_ = INVALID_HANDLE_VALUE; }
  }

  const Index& index() const { return index_; }

  // Распаковать файл в target. onBytes(n) вызывается после каждого блока.
  template <class OnBytes>
  bool extract(const Entry& e, const std::filesystem::path& target, OnBytes&& onBytes,
               std::wstring* error = nullptr) {
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    // Файл может быть ненадолго занят (антивирус проверяет только что записанный
    // файл прошлой установки) — несколько попыток с паузой.
    HANDLE out = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 20; ++attempt) {
      out = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
      const DWORD err = out == INVALID_HANDLE_VALUE ? GetLastError() : 0;
      if (out != INVALID_HANDLE_VALUE || (err != ERROR_SHARING_VIOLATION && err != ERROR_ACCESS_DENIED &&
                                          err != ERROR_LOCK_VIOLATION))
        break;
      Sleep(250);
    }
    if (out == INVALID_HANDLE_VALUE) {
      if (error) *error = L"Не удалось записать файл " + target.wstring();
      return false;
    }
    bool ok = true;
    std::vector<uint8_t> comp, raw;
    for (const auto& c : e.chunks) {
      comp.resize(c.compSize);
      raw.resize(c.rawSize);
      if (!readAt(c.offset, comp.data(), comp.size())) { ok = false; break; }
      const uint8_t* data = comp.data();
      if (c.method == kLzms) {
        SIZE_T got = 0;
        if (!Decompress(dec_, comp.data(), comp.size(), raw.data(), raw.size(), &got) ||
            got != c.rawSize) { ok = false; break; }
        data = raw.data();
      }
      DWORD written = 0;
      if (!WriteFile(out, data, c.rawSize, &written, nullptr) || written != c.rawSize) {
        ok = false;
        break;
      }
      onBytes(uint64_t(c.rawSize));
    }
    CloseHandle(out);
    if (!ok && error && error->empty()) *error = L"Повреждён установочный архив или нет места на диске";
    return ok;
  }

 private:
  bool readAt(uint64_t offset, void* out, size_t n) {
    OVERLAPPED ov{};
    ov.Offset = DWORD(offset & 0xFFFFFFFFu);
    ov.OffsetHigh = DWORD(offset >> 32);
    DWORD got = 0;
    return ReadFile(file_, out, DWORD(n), &got, &ov) && got == n;
  }

  HANDLE file_ = INVALID_HANDLE_VALUE;
  DECOMPRESSOR_HANDLE dec_ = nullptr;
  Index index_;
};

}  // namespace crumb::payload
