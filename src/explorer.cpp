#include "explorer.h"
#include "app.h"

#include <sys/dir.h>
#include <sys/stat.h>
#include <spdlog/spdlog.h>

ExplorerContext::ExplorerContext(std::string startPath) {
  if (!tryEnqueue(std::move(startPath))) {
    spdlog::error("unable to enqueue path `{}`", startPath);
    throw std::logic_error("could not start search");
  }

  threads.reserve(nExplorers);
  for (u32 i = 0; i < nExplorers; ++i) {
    threads.emplace_back(std::thread([this] { walk(); }));
  }
}

void ExplorerContext::walk() {

  const std::string& pattern       = g_app->pattern;
  const u64          size          = pattern.size();
  const bool         is_pipe       = g_app->pipe;
  const bool         enqueueHidden = g_app->hidden;

  std::string        buffer;
  buffer.reserve(KiB(32));

  auto isDirectory = [](struct dirent* entry, const char* fullPath) {
    if (entry->d_type == DT_DIR) {
      return true;
    } else if (entry->d_type == DT_UNKNOWN) {
      struct stat st;
      return (stat(fullPath, &st) == 0) && S_ISDIR(st.st_mode);
    }
    return false;
  };

  auto buildPath = [=](std::string_view rootPath, struct dirent* entry) {
    std::string pathBuffer;
    pathBuffer.reserve(KiB(4));
    pathBuffer.assign(rootPath);
    if (rootPath.back() != '/') { pathBuffer += "/"; }
    pathBuffer.append(entry->d_name);
    if (isDirectory(entry, pathBuffer.data())) { pathBuffer += "/"; }
    return pathBuffer;
  };

  auto flushBuffer = [&] {
    u64 written = 0;
    while (written < buffer.size()) {
      ssize_t n = write(STDOUT_FILENO, buffer.data() + written, buffer.size() - written);
      if (n <= 0) { break; }
      written += static_cast<u64>(n);
    }
    buffer.clear();
  };

  // view will move
  auto highlightPattern = [&](std::string_view& view) {
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
    return found;
  };

  auto validateAndPushInBuffer = [&](std::string_view view) {
    if (highlightPattern(view)) {
      buffer.append(view); // append rest of string
      buffer += "\n";
    }
  };

  std::vector<Task> dirsBatch{256};
  dirsBatch.clear();

  std::array<Task, 16> filePathRefs;
  bool                 loop = true;

  while (loop) {
    u32 count = dequeueBulk(filePathRefs);
    for (u32 i = 0; i < count; ++i) {
      [[unlikely]] if (filePathRefs[i] == POISON) {
        enqueue(std::move(filePathRefs[i]));
        loop = false;
        break;
      }

      std::string& filePathRoot = filePathRefs[i];
      [[likely]] if (!(filePathRoot == "/proc" || filePathRoot.starts_with("/proc/"))) {
        DIR*           dirHandle;
        struct dirent* entry;

        [[likely]] if ((dirHandle = opendir(filePathRoot.data())) != NULL) {
          while ((entry = readdir(dirHandle)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { continue; }
            if (!enqueueHidden && strncmp(entry->d_name, ".", 1) == 0) { continue; }

            std::string path = buildPath(filePathRoot, entry);
            validateAndPushInBuffer(path);
            if (path.back() == '/') {
              dirsInFlight.fetch_add(1, std::memory_order_acq_rel);
              dirsBatch.emplace_back(path);
            }
          }
          closedir(dirHandle);
        }
      }
    }

    enqueueBulk({dirsBatch.begin(), dirsBatch.size()});
    dirsBatch.clear();
    [[unlikely]] if (!loop) { break; }
    [[unlikely]] if (dirsInFlight.fetch_sub(count, std::memory_order_acq_rel) == count) {
      poison();
    }
    if (buffer.size() >= KiB(30)) { flushBuffer(); }
  }
  flushBuffer();
};
