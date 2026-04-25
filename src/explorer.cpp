#include "explorer.h"
#include "app.h"
#include "memory/types.h"

#include <pcre2.h>

#include <sys/dir.h>
#include <sys/stat.h>
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

void ExplorerContext::walk() {
  static pcre2_code*                    pattern = g_app->pattern;
  thread_local static pcre2_match_data* match_data =
      pcre2_match_data_create_from_pattern(pattern, 0);

  if (!pattern) {
    spdlog::error("provided pattern is invalid");
    exit(1);
  }

  static const bool     isPipe        = g_app->pipe;
  static const bool     enqueueHidden = g_app->hidden;
  static const u32      maxDepth      = g_app->maxDepth;
  static const FileType fileType      = g_app->type;

  auto                  isDirectory = [](string::String& fullPath, struct dirent* entry) {
    if (entry->d_type == DT_DIR) {
      return true;
    } else if (entry->d_type == DT_UNKNOWN) {
      struct stat st;
      return (stat(fullPath.c_str(), &st) == 0) && S_ISDIR(st.st_mode);
    }
    return false;
  };

  auto buildPath = [&](string::String& rootPath, struct dirent* entry) {
    static thread_local string::String pathBuffer{KiB(4)};
    pathBuffer = rootPath;
    if (rootPath.back() != '/') { pathBuffer += "/"; }
    pathBuffer.append(entry->d_name);
    bool isDir = isDirectory(pathBuffer, entry);
    if (isDir) { pathBuffer += "/"; }
    return std::pair(pathBuffer, isDir);
  };

  thread_local string::String buffer{KiB(16)};
  auto                        flushBuffer = [&] {
    u64 written = 0;
    while (written < buffer.size()) {
      ssize_t n = write(STDOUT_FILENO, buffer.c_str() + written, buffer.size() - written);
      if (n <= 0) { break; }
      written += static_cast<u64>(n);
    }
    buffer.clear();
  };

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
    if (rc >= 0) {
      PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
      u64         lastIdx = 0;

      if (!isPipe) {
        // append until start of matched substring
        buffer.append(data + lastIdx, ovector[0] - lastIdx);
        buffer += "\033[1m\033[3m"; // bold, italic text
        lastIdx = ovector[0];

        for (i32 i = 0; i < rc; ++i) {
          if (rc > 1 && i == 0) { continue; }
          u64 startIdx = ovector[2 * i];
          u64 endIdx   = ovector[2 * i + 1];
          u64 len      = endIdx - startIdx;

          buffer.append(data + lastIdx, startIdx - lastIdx);
          const u8* start = data + startIdx;

          buffer += *it;
          if (++it == colors.cend()) { it = colors.cbegin(); }

          buffer.append(start, len);
          buffer += "\033[30m"; // black
          lastIdx = endIdx;
        }

        buffer += "\033[0m"; // reset color
      }

      // append rest of string
      buffer.append(data + lastIdx, str.size() - lastIdx);
      buffer += "\n";
    }
  };

  auto              ret = std::experimental::scope_exit([&] {
    flushBuffer();
    pcre2_match_data_free(match_data);
  });

  std::vector<Task> dirsBatch;
  dirsBatch.reserve(1024);

  std::array<Task, 16> tasks;

  while (true) {
    u32 count = dequeueBulk(tasks);

    for (u32 i = 0; i < count; ++i) {
      Task& task = tasks[i];
      [[unlikely]] if (task.fullPath == POISON) {
        enqueue(std::move(task));
        return;
      }

      string::String& dirPath = task.fullPath;
      [[likely]] if (!(dirPath.startsWith("/proc/"))) {
        DIR*           dirHandle;
        struct dirent* entry;

        [[likely]] if ((dirHandle = opendir(dirPath.c_str())) != NULL) {
          while ((entry = readdir(dirHandle)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { continue; }
            if (!enqueueHidden && entry->d_name[0] == '.') { continue; }

            auto [fullPath, isDir] = buildPath(dirPath, entry);
            if ((fileType == FileType::None) || ((fileType & FileType::Directory) > 0 && isDir) ||
                ((fileType & FileType::File) > 0 && !isDir)) {
              findAndHighlightPattern(fullPath);
            }

            if (fullPath.back() == '/') {
              if (maxDepth > 0 && task.directoryLevel + 1 == maxDepth) { continue; }
              dirsInFlight.fetch_add(1, std::memory_order_acq_rel);
              dirsBatch.emplace_back(std::move(fullPath), task.directoryLevel + 1);
            }
          }

          closedir(dirHandle);
        }
      }

      flushBuffer();
    }

    enqueueBulk(dirsBatch);
    dirsBatch.clear();

    [[unlikely]] if (dirsInFlight.fetch_sub(count, std::memory_order_acq_rel) == count) {
      poison();
    }
  }
};
