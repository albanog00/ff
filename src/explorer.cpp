#include "explorer.h"
#include "app.h"

#include <sys/dir.h>
#include <sys/stat.h>
#include <spdlog/spdlog.h>

ExplorerContext::ExplorerContext(WorkerContext& workerCtx, const std::string& startPath) :
    workerCtx(workerCtx) {
  if (!tryEnqueue(startPath)) {
    spdlog::error("unable to enqueue path `{}`", startPath);
    throw std::logic_error("could not start search");
  }

  threads.reserve(nExplorers);
  for (u32 i = 0; i < nExplorers; ++i) {
    threads.emplace_back(std::thread([this] { walk(); }));
  }
}

void ExplorerContext::walk() {
  Task rootRef;

  struct Path {
    std::string fullPath;
    bool        isDir;
  };

  auto isDirectory = [](struct dirent* entry, const char* fullPath) {
    if (entry->d_type == DT_DIR) {
      return true;
    } else if (entry->d_type == DT_UNKNOWN) {
      struct stat st;
      return (stat(fullPath, &st) == 0) && S_ISDIR(st.st_mode);
    }
    return false;
  };

  auto formatPath = [=](const std::string& rootPath, struct dirent* entry) {
    std::string fullPath;
    fullPath.reserve(rootPath.size() + 2 + strlen(entry->d_name));
    fullPath = rootPath;
    if (fullPath.back() != '/') { fullPath += "/"; }
    fullPath += entry->d_name;

    bool isDir = isDirectory(entry, fullPath.c_str());
    if (isDir) { fullPath += "/"; }
    return Path{std::move(fullPath), isDir};
  };

  bool enqueueHidden = g_app->hidden;

  while (true) {
    dequeue(rootRef);
    [[unlikely]] if (rootRef == POISON) { return; }

    std::string& rootPath = *rootRef;
    [[likely]] if (!(rootPath == "/proc" || rootPath.starts_with("/proc/"))) {
      DIR*           dirHandle;
      struct dirent* entry;

      [[likely]] if ((dirHandle = opendir(rootPath.c_str())) != NULL) {
        while ((entry = readdir(dirHandle)) != NULL) {
          if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { continue; }
          if (!enqueueHidden && strncmp(entry->d_name, ".", 1) == 0) { continue; }

          Path path = formatPath(rootPath, entry);
          Task strRef{std::move(path.fullPath)};

          if (path.isDir && enqueue(strRef)) {
            dirsInFlight.fetch_add(1, std::memory_order_acq_rel);
          }
          workerCtx.enqueue(strRef);
        }

        closedir(dirHandle);
      }
    }

    if (dirsInFlight.fetch_sub(1, std::memory_order_acq_rel) == 1) { poison(); }
  }
};
