// crumb_pack — собирает Crumb-Setup.exe: stub + сжатая папка сборки.
//   crumb_pack --input <папка> --stub <stub.exe> --out <Crumb-Setup.exe>
// Блоки по 16 МБ сжимаются LZMS параллельно на всех ядрах.

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>

#include "../common/Payload.h"

namespace fs = std::filesystem;
using namespace crumb::payload;

namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

// Файлы браузера, которые не нужны интерфейсу установщика. Всё остальное
// (Qt, QML, runtime Visual C++, сам crumb_setup.exe) помечается kFlagUi.
bool isUiFile(const std::string& rel) {
  static const std::set<std::string> heavy = {
      "crumb.exe", "crumb_helper.exe", "libcef.dll", "chrome_elf.dll", "dxcompiler.dll",
      "dxil.dll", "vk_swiftshader.dll", "vk_swiftshader_icd.json", "vulkan-1.dll",
      "v8_context_snapshot.bin", "icudtl.dat", "chrome_100_percent.pak",
      "chrome_200_percent.pak", "resources.pak"};
  const std::string l = lower(rel);
  if (l.rfind("locales/", 0) == 0) return false;
  return heavy.count(l) == 0;
}

bool skipFile(const fs::path& p) {
  const std::string ext = lower(p.extension().string());
  return ext == ".pdb" || ext == ".lib" || ext == ".exp" || ext == ".ilk" || ext == ".log";
}

struct Job {
  size_t entry;
  size_t chunk;
  uint64_t fileOffset;
  uint32_t rawSize;
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
  SetConsoleOutputCP(CP_UTF8);
  fs::path input, stub, out;
  for (int i = 1; i + 1 < argc; ++i) {
    const std::wstring a = argv[i];
    if (a == L"--input") input = argv[++i];
    else if (a == L"--stub") stub = argv[++i];
    else if (a == L"--out") out = argv[++i];
  }
  if (input.empty() || stub.empty() || out.empty()) {
    std::cerr << "usage: crumb_pack --input DIR --stub STUB.exe --out SETUP.exe\n";
    return 2;
  }

  // 1. Список файлов.
  Index idx;
  std::vector<fs::path> sources;
  for (const auto& it : fs::recursive_directory_iterator(input)) {
    if (!it.is_regular_file() || skipFile(it.path())) continue;
    std::string rel = narrow(fs::relative(it.path(), input).wstring());
    std::replace(rel.begin(), rel.end(), '\\', '/');
    Entry e;
    e.path = rel;
    e.size = it.file_size();
    e.flags = isUiFile(rel) ? kFlagUi : 0;
    const uint64_t n = (e.size + kChunkSize - 1) / kChunkSize;
    e.chunks.resize(size_t(std::max<uint64_t>(n, 0)));
    idx.totalSize += e.size;
    idx.entries.push_back(std::move(e));
    sources.push_back(it.path());
  }
  if (idx.entries.empty()) {
    std::cerr << "input folder is empty\n";
    return 1;
  }

  std::vector<Job> jobs;
  for (size_t i = 0; i < idx.entries.size(); ++i)
    for (size_t c = 0; c < idx.entries[i].chunks.size(); ++c) {
      const uint64_t off = uint64_t(c) * kChunkSize;
      jobs.push_back({i, c, off, uint32_t(std::min<uint64_t>(kChunkSize, idx.entries[i].size - off))});
    }
  // Большие блоки — первыми, чтобы потоки закончили примерно одновременно.
  std::stable_sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) { return a.rawSize > b.rawSize; });

  // 2. Параллельное сжатие.
  std::vector<std::vector<uint8_t>> results(jobs.size());
  std::atomic<size_t> next{0}, done{0};
  std::atomic<bool> failed{false};
  std::mutex logMutex;
  const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::thread> pool;
  for (unsigned t = 0; t < threads; ++t) {
    pool.emplace_back([&] {
      COMPRESSOR_HANDLE comp = nullptr;
      if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &comp)) { failed = true; return; }
      std::vector<uint8_t> raw;
      for (size_t j; (j = next++) < jobs.size() && !failed;) {
        const Job& job = jobs[j];
        raw.resize(job.rawSize);
        std::ifstream in(sources[job.entry], std::ios::binary);
        in.seekg(std::streamoff(job.fileOffset));
        in.read(reinterpret_cast<char*>(raw.data()), job.rawSize);
        if (!in) { failed = true; break; }

        Chunk& ch = idx.entries[job.entry].chunks[job.chunk];
        ch.rawSize = job.rawSize;
        SIZE_T need = 0;
        Compress(comp, raw.data(), raw.size(), nullptr, 0, &need);
        std::vector<uint8_t> packed(need ? need : raw.size() + 1024);
        SIZE_T got = 0;
        if (Compress(comp, raw.data(), raw.size(), packed.data(), packed.size(), &got) && got < raw.size()) {
          packed.resize(got);
          ch.method = kLzms;
          results[j] = std::move(packed);
        } else {
          ch.method = kStored;   // не сжимается (уже сжатые данные) — храним как есть
          results[j] = raw;
        }
        ch.compSize = uint32_t(results[j].size());
        const size_t d = ++done;
        std::lock_guard l(logMutex);
        std::printf("\r  сжато блоков: %zu / %zu", d, jobs.size());
        std::fflush(stdout);
      }
      CloseCompressor(comp);
    });
  }
  for (auto& th : pool) th.join();
  std::printf("\n");
  if (failed) {
    std::cerr << "compression failed\n";
    return 1;
  }

  // 3. Запись: stub, блоки в порядке файлов, индекс, трейлер.
  std::error_code ec;
  fs::create_directories(out.parent_path(), ec);
  std::ofstream f(out, std::ios::binary | std::ios::trunc);
  {
    std::ifstream s(stub, std::ios::binary);
    f << s.rdbuf();
  }
  uint64_t offset = uint64_t(f.tellp());
  // Индекс задания по (файл, блок), чтобы писать данные по порядку.
  std::vector<std::vector<size_t>> jobOf(idx.entries.size());
  for (size_t i = 0; i < idx.entries.size(); ++i) jobOf[i].resize(idx.entries[i].chunks.size());
  for (size_t j = 0; j < jobs.size(); ++j) jobOf[jobs[j].entry][jobs[j].chunk] = j;
  for (size_t i = 0; i < idx.entries.size(); ++i)
    for (size_t c = 0; c < idx.entries[i].chunks.size(); ++c) {
      const auto& data = results[jobOf[i][c]];
      idx.entries[i].chunks[c].offset = offset;
      f.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
      offset += data.size();
    }
  const std::vector<uint8_t> index = serializeIndex(idx);
  const uint64_t indexOffset = offset, indexSize = index.size();
  f.write(reinterpret_cast<const char*>(index.data()), std::streamsize(index.size()));
  f.write(reinterpret_cast<const char*>(&indexOffset), 8);
  f.write(reinterpret_cast<const char*>(&indexSize), 8);
  f.write(kMagic, 8);
  f.close();
  if (!f) {
    std::cerr << "write failed\n";
    return 1;
  }

  uint64_t ui = 0;
  for (const auto& e : idx.entries) if (e.flags & kFlagUi) ui += e.size;
  std::printf("  файлов: %zu, исходный размер: %.1f МБ (интерфейс установщика: %.1f МБ)\n",
              idx.entries.size(), idx.totalSize / 1048576.0, ui / 1048576.0);
  std::printf("  установщик: %.1f МБ -> %ls\n", double(fs::file_size(out)) / 1048576.0, out.c_str());
  return 0;
}
