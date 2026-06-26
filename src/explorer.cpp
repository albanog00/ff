#include "explorer.h"
#include "app.h"
#include "utils/defer.h"
#include "utils/fs.h"
#include "ignore.h"

#include "memory/types.h"
#include "string/string.h"

#include <cstring>
#include <sys/dir.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <pcre2.h>
#include <spdlog/spdlog.h>
#include <experimental/scope>

ExplorerContext::ExplorerContext(std::span<string::String> startPaths) {
  u32 failed = 0;
  for (u32 i = 0; i < startPaths.size(); ++i) {
    if (!tryEnqueue(Task(startPaths[i], 0))) {
      failed += 1;
      spdlog::error("unable to enqueue path `{}`", startPaths[i].c_str());
    } else {
      dirsInFlight.fetch_add(1, std::memory_order_relaxed);
    }
  }

  if (failed == startPaths.size()) { throw std::logic_error("could not start search"); }

  threads.reserve(nExplorers);
  for (u32 i = 0; i < nExplorers; ++i) {
    threads.emplace_back(std::thread([this] { walk(); }));
  }
}

string::String buildPath(string::String& rootPath, struct dirent* entry, bool isDir) {
  static thread_local string::String pathBuffer{KiB(4)};
  pathBuffer = rootPath;
  if (rootPath.back() != '/') { pathBuffer += "/"; }
  pathBuffer.append(entry->d_name);
  if (isDir) { pathBuffer += "/"; }
  return pathBuffer;
};

bool ignorePath(FileType fileType, bool isDir) {
  return !((fileType == FileType::None) || ((fileType & FileType::Directory) > 0 && isDir) ||
      ((fileType & FileType::File) > 0 && !isDir));
}

void ExplorerContext::walk() {
  static pcre2_code* pattern = g_app->pattern;

  if (!pattern) {
    spdlog::error("provided pattern is invalid");
    exit(1);
  }

  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(pattern, 0);
  defer(pcre2_match_data_free(match_data));

  static const bool     isPipe                = g_app->pipe;
  static const bool     enqueueHidden         = g_app->hidden;
  static const u32      maxDepth              = g_app->maxDepth;
  static const FileType fileType              = g_app->type;
  static const u32      OutBufferCapacity     = KiB(32) - 1;
  static const u32      MaxBufferRetainedSize = KiB(24);

  string::String        buffer{OutBufferCapacity};
  auto                  flushBuffer = [&] {
    u64 written = 0;
    u64 size    = buffer.size();
    while (written < size) {
      ssize_t n = write(STDOUT_FILENO, buffer.c_str() + written, size - written);
      if (n <= 0) { break; }
      written += static_cast<u64>(n);
    }
    buffer.clear();
  };

  defer(flushBuffer());
#if DEBUG
  defer(spdlog::debug("thread_id={} buffer_capacity={}", pthread_self(), buffer.capacity()));
  defer(string::String::dumpPoolStats());
#endif

  auto findAndHighlightPattern = [&](const string::String& str) {
    static const std::array<const char*, 7> colors = {
        "\033[31;1m",
        "\033[32;1m",
        "\033[33;1m",
        "\033[34;1m",
        "\033[35;1m",
        "\033[36;1m",
        "\033[37;1m",
    };

    auto      it   = colors.cbegin();
    const u8* data = str.data();

    i32       rc = pcre2_match(pattern, data, str.size(), 0, 0, match_data, NULL);
    if (rc > 0) {
      PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
      u32         lastIdx = 0;

      if (!isPipe) {
        // append until start of matched substring
        buffer.append(data + lastIdx, ovector[0] - lastIdx);
        buffer += "\033[1m\033[3m"; // bold, italic text
        lastIdx = ovector[0];

        i32 i = rc > 1; // 1 or 0
        while (i < rc) {
          u32 startIdx = ovector[2 * i];
          u32 endIdx   = ovector[2 * i + 1];
          u64 len      = endIdx - startIdx;

          buffer.append(data + lastIdx, startIdx - lastIdx);
          const u8* start = data + startIdx;

          // set color for current group match
          buffer += *it;
          if (++it == colors.cend()) { it = colors.cbegin(); }

          buffer.append(start, len);
          buffer += "\033[30m"; // black
          lastIdx = endIdx;
          i += 1;
        }

        buffer += "\033[0m"; // reset color
      }

      // append rest of string
      buffer.append(data + lastIdx, str.size() - lastIdx);
      buffer += "\n";
    }
  };

  std::vector<Task> dirsBatch;
  dirsBatch.reserve(1024);

  std::array<Task, 16> tasks;
  bool                 loop = true;

  while (loop) {
    u32 count = dequeueBulk(tasks);

    for (u32 i = 0; i < count; ++i) {
      Task& task = tasks[i];
      if (loop && task.fullPath == POISON) [[unlikely]] {
        enqueue(std::move(task));
        loop = false;
        continue;
      }

      string::String& dirPath = task.fullPath;
      DIR*            dirHandle;
      struct dirent*  entry;

      if ((dirHandle = opendir(dirPath.c_str())) != NULL) [[likely]] {
        defer(closedir(dirHandle));
        i32 dirFd = dirfd(dirHandle);

        readGitignore(dirPath, dirFd);

        // scan dir entries
        while ((entry = readdir(dirHandle)) != NULL) {
          if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) [[unlikely]] {
            continue;
          }

          // skip hidden when hidden flag is not enabled
          if (entry->d_name[0] == '.' && !enqueueHidden) { continue; }

          bool           isDir    = isDirectory(entry, dirFd);
          string::String fullPath = buildPath(dirPath, entry, isDir);

          // TODO: apply .gitignore rules

          if (ignorePath(fileType, isDir)) { continue; }

          findAndHighlightPattern(fullPath);
          if (buffer.size() >= MaxBufferRetainedSize) { flushBuffer(); }

          if (isDir) {
            if (maxDepth > 0 && task.directoryLevel + 1 == maxDepth) { continue; }
            dirsInFlight.fetch_add(1, std::memory_order_acq_rel);
            dirsBatch.emplace_back(std::move(fullPath), task.directoryLevel + 1);
          }
        }
      }

      enqueueBulk(dirsBatch);
      dirsBatch.clear();
    }

    if (dirsInFlight.fetch_sub(count, std::memory_order_acq_rel) == count) [[unlikely]] {
      poison();
    }
  }
};
