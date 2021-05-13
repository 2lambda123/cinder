// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "Jit/perf_jitdump.h"

#include "Jit/log.h"
#include "Jit/util.h"

#include <fmt/format.h>

#include <elf.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>

#ifdef __x86_64__
// Use the cheaper rdtsc by default. If you disable this for some reason, or
// run on a non-x86_64 architecture, you need to add '-k1' to your 'perf
// record' command.
#define PERF_USE_RDTSC
#endif

#ifdef PERF_USE_RDTSC
#include <x86intrin.h>
#endif

namespace jit {
namespace perf {

extern const std::string kDefaultSymbolPrefix{"__CINDER_INFRA_JIT"};
extern const std::string kFuncSymbolPrefix{"__CINDER_JIT"};
extern const std::string kNoFrameSymbolPrefix{"__CINDER_NO_FRAME_JIT"};

namespace {

struct FileInfo {
  std::string filename;
  std::string filename_format;
  std::FILE* file{nullptr};
};

FileInfo g_pid_map;

FileInfo g_jitdump_file;
void* g_jitdump_mmap_addr = nullptr;
const size_t kJitdumpMmapSize = 1;

// C++-friendly wrapper around strerror_r().
std::string string_error(int errnum) {
  char buf[1024];
  return strerror_r(errnum, buf, sizeof(buf));
}

class FileLock {
 public:
  FileLock(std::FILE* file, bool exclusive) : fd_{fileno(file)} {
    auto operation = exclusive ? LOCK_EX : LOCK_SH;
    while (true) {
      auto ret = ::flock(fd_, operation);
      if (ret == 0) {
        return;
      }
      if (ret == -1 && errno == EINTR) {
        continue;
      }
      JIT_CHECK(
          false,
          "flock(%d, %d) failed: %s",
          fd_,
          operation,
          string_error(errno));
    }
  }

  ~FileLock() {
    auto ret = ::flock(fd_, LOCK_UN);
    JIT_CHECK(
        ret == 0, "flock(%d, LOCK_UN) failed: %s", fd_, string_error(errno));
  }

  DISALLOW_COPY_AND_ASSIGN(FileLock);

 private:
  int fd_;
};

class SharedFileLock : public FileLock {
 public:
  SharedFileLock(std::FILE* file) : FileLock{file, false} {}
};

class ExclusiveFileLock : public FileLock {
 public:
  ExclusiveFileLock(std::FILE* file) : FileLock{file, true} {}
};

// This file writes out perf jitdump files, to be used by 'perf inject' and
// 'perf report'. The format is documented here:
// https://raw.githubusercontent.com/torvalds/linux/master/tools/perf/Documentation/jitdump-specification.txt.

enum Flags {
  JITDUMP_FLAGS_ARCH_TIMESTAMP = 1,
};

struct FileHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t total_size;
  uint32_t elf_mach;
  uint32_t pad1;
  uint32_t pid;
  uint64_t timestamp;
  uint64_t flags;
};

enum RecordType {
  JIT_CODE_LOAD = 0,
  JIT_CODE_MOVE = 1,
  JIT_CODE_DEBUG_INFO = 2,
  JIT_CODE_CLOSE = 3,
  JIT_CODE_UNWINDING_INFO = 4,
};

struct RecordHeader {
  uint32_t type;
  uint32_t total_size;
  uint64_t timestamp;
};

struct CodeLoadRecord : RecordHeader {
  uint32_t pid;
  uint32_t tid;
  uint64_t vma;
  uint64_t code_addr;
  uint64_t code_size;
  uint64_t code_index;
};

// The gettid() syscall doesn't have a C wrapper.
pid_t gettid() {
  return syscall(SYS_gettid);
}

// Get a timestamp for the current event.
uint64_t getTimestamp() {
#ifdef PERF_USE_RDTSC
  return __rdtsc();
#else
  static const uint64_t kNanosPerSecond = 1000000000;
  struct timespec tm;
  int ret = clock_gettime(CLOCK_MONOTONIC, &tm);
  if (ret < 0) {
    return -1;
  }
  return tm.tv_sec * kNanosPerSecond + tm.tv_nsec;
#endif
}

FileInfo openFileInfo(std::string filename_format) {
  auto filename = fmt::format(filename_format, getpid());
  auto file = std::fopen(filename.c_str(), "w+");
  if (file == nullptr) {
    JIT_LOG("Couldn't open %s for writing (%s)", filename, string_error(errno));
    return {};
  }
  return {filename, filename_format, file};
}

FileInfo openPidMap() {
  auto env = Py_GETENV("JIT_PERFMAP");
  if (env == nullptr || env[0] == '\0') {
    return {};
  }

  FileInfo perf_map = openFileInfo("/tmp/perf-{}.map");
  JIT_DLOG("Opened JIT perf-map file: %s", perf_map.filename);
  return perf_map;
}

// If enabled, open the jitdump file, and write out its header.
FileInfo openJitdumpFile() {
  auto dumpdir = Py_GETENV("JIT_DUMPDIR");
  if (dumpdir == nullptr || dumpdir[0] == '\0') {
    return {};
  }

  JIT_CHECK(dumpdir[0] == '/', "jitdump directory path isn't absolute");
  auto info = openFileInfo(fmt::format("{}/jit-{{}}.dump", dumpdir));
  if (info.file == nullptr) {
    return {};
  }
  auto fd = fileno(info.file);

  // mmap() the jitdump file so perf inject can find it.
  auto g_jitdump_mmap_addr =
      mmap(nullptr, kJitdumpMmapSize, PROT_EXEC, MAP_PRIVATE, fd, 0);
  JIT_CHECK(
      g_jitdump_mmap_addr != MAP_FAILED,
      "marker mmap of jitdump file failed: %s",
      string_error(errno));

  // Write out the file header.
  FileHeader header;
  header.magic = 0x4a695444;
  header.version = 1;
  header.total_size = sizeof(header);
#ifdef __x86_64__
  header.elf_mach = EM_X86_64;
#else
#error Please provide the ELF e_machine value for your architecture.
#endif
  header.pad1 = 0;
  header.pid = getpid();
  header.timestamp = getTimestamp();
#ifdef PERF_USE_RDTSC
  header.flags = JITDUMP_FLAGS_ARCH_TIMESTAMP;
#else
  header.flags = 0;
#endif

  std::fwrite(&header, sizeof(header), 1, info.file);
  std::fflush(info.file);
  return info;
}

void initFiles() {
  static bool inited = false;
  if (inited) {
    return;
  }
  g_pid_map = openPidMap();
  g_jitdump_file = openJitdumpFile();
  inited = true;
}

// Copy the contents of from_name to to_name. Returns a std::FILE* at the end
// of to_name on success, or nullptr on failure.
std::FILE* copyFile(const std::string& from_name, const std::string& to_name) {
  auto from = std::fopen(from_name.c_str(), "r");
  if (from == nullptr) {
    JIT_LOG(
        "Couldn't open %s for reading (%s)", from_name, string_error(errno));
    return nullptr;
  }
  auto to = std::fopen(to_name.c_str(), "w+");
  if (to == nullptr) {
    std::fclose(from);
    JIT_LOG("Couldn't open %s for writing (%s)", to_name, string_error(errno));
    return nullptr;
  }

  char buf[4096];
  while (true) {
    auto bytes_read = std::fread(&buf, 1, sizeof(buf), from);
    auto bytes_written = std::fwrite(&buf, 1, bytes_read, to);
    if (bytes_read < sizeof(buf) && std::feof(from)) {
      // We finished successfully.
      std::fflush(to);
      std::fclose(from);
      return to;
    }
    if (bytes_read == 0 || bytes_written < bytes_read) {
      JIT_LOG("Error copying %s to %s", from_name, to_name);
      std::fclose(from);
      std::fclose(to);
      return nullptr;
    }
  }
}

// Copy the perf pid map from the parent process into a new file for this child
// process.
void copyFileInfo(FileInfo& info) {
  if (info.file == nullptr) {
    return;
  }

  std::fclose(info.file);
  auto parent_filename = info.filename;
  auto child_filename = fmt::format(info.filename_format, getpid());
  info = {};

  unlink(child_filename.c_str());
  if (auto new_pid_map = copyFile(parent_filename, child_filename)) {
    info.filename = child_filename;
    info.file = new_pid_map;
  }
}

void copyParentPidMap() {
  copyFileInfo(g_pid_map);
}

void copyJitdumpFile() {
  auto ret = munmap(g_jitdump_mmap_addr, kJitdumpMmapSize);
  JIT_CHECK(
      ret == 0, "marker unmap of jitdump file failed: %s", string_error(errno));

  copyFileInfo(g_jitdump_file);
  if (g_jitdump_file.file == nullptr) {
    return;
  }

  g_jitdump_mmap_addr = mmap(
      nullptr,
      kJitdumpMmapSize,
      PROT_EXEC,
      MAP_PRIVATE,
      fileno(g_jitdump_file.file),
      0);
}

} // namespace

void registerFunction(
    void* code,
    std::size_t size,
    const std::string& name,
    const std::string& prefix) {
  initFiles();

  if (auto file = g_pid_map.file) {
    fmt::print(
        file,
        "{:x} {:x} {}:{}\n",
        reinterpret_cast<uintptr_t>(code),
        size,
        prefix,
        name);
    std::fflush(file);
  }

  if (auto file = g_jitdump_file.file) {
    // Make sure no parent or child process writes concurrently.
    ExclusiveFileLock write_lock(file);

    static uint64_t code_index = 0;
    auto const prefixed_name = prefix + ":" + name;

    CodeLoadRecord record;
    record.type = JIT_CODE_LOAD;
    record.total_size = sizeof(record) + prefixed_name.size() + 1 + size;
    record.timestamp = getTimestamp();
    record.pid = getpid();
    record.tid = gettid();
    record.vma = record.code_addr = reinterpret_cast<uint64_t>(code);
    record.code_size = size;
    record.code_index = code_index++;

    std::fwrite(&record, sizeof(record), 1, file);
    std::fwrite(prefixed_name.data(), 1, prefixed_name.size() + 1, file);
    std::fwrite(code, 1, size, file);
    std::fflush(file);
  }
}

void afterForkChild() {
  copyParentPidMap();
  copyJitdumpFile();
}

} // namespace perf
} // namespace jit
