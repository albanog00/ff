#include "explorer.h"
#include "app.h"
#include "memory/types.h"

#include <string_view>
#include <sys/dir.h>
#include <sys/stat.h>
#include <spdlog/spdlog.h>
#include <experimental/scope>

ExplorerContext::ExplorerContext(string::String startPath) {
  if (!tryEnqueue(Task(startPath, 0))) {
    spdlog::error("unable to enqueue path `{}`", startPath.c_str());
    throw std::logic_error("could not start search");
  }

  threads.reserve(nExplorers);
  for (u32 i = 0; i < nExplorers; ++i) {
    threads.emplace_back(std::thread([this] { walk(); }));
  }
}

void ExplorerContext::walk() {
  static const std::string pattern       = g_app->pattern.string();
  static const u64         size          = pattern.size();
  static const bool        is_pipe       = g_app->pipe;
  static const bool        enqueueHidden = g_app->hidden;
  static const u32         maxDepth      = g_app->maxDepth;

  auto                     isDirectory = [](string::String& fullPath, struct dirent* entry) {
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
    if (isDirectory(pathBuffer, entry)) { pathBuffer += "/"; }
    return pathBuffer;
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

  auto findAndHighlightPattern = [&](std::string_view view) {
    bool found = false;
    u64  it    = 0;

    while ((it = view.find(pattern)) != std::string::npos) {
      found = true;
      if (is_pipe) { break; }               // no highlight in pipe mode
      buffer.append(view.substr(0, it));    // append till start of pattern
      buffer += "\033[31m";                 // color
      buffer.append(view.substr(it, size)); // append pattern
      buffer += "\033[0m";                  // reset color
      view.remove_prefix(it + size);
    }

    if (found) {
      buffer.append(view); // append rest of string
      buffer += "\n";
    }
  };

  auto              ret = std::experimental::scope_exit(flushBuffer);

  std::vector<Task> dirsBatch;
  dirsBatch.reserve(1024);

  std::array<Task, 16> tasks;

  // Add max-depth
  // Simple way: struct with directory level

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

            string::String fullPath = buildPath(dirPath, entry);
            findAndHighlightPattern(fullPath.view());

            if (maxDepth > 0 && task.directoryLevel == maxDepth) { continue; }

            if (fullPath.back() == '/') {
              dirsInFlight.fetch_add(1, std::memory_order_acq_rel);
              dirsBatch.emplace_back(std::move(fullPath), task.directoryLevel + 1);
            }
          }
          closedir(dirHandle);
        }
      }
    }

    enqueueBulk(dirsBatch);
    dirsBatch.clear();

    if (buffer.size() >= KiB(8)) { flushBuffer(); }
    [[unlikely]] if (dirsInFlight.fetch_sub(count, std::memory_order_acq_rel) == count) {
      poison();
    }
  }
};
