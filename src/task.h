#include "ignore.h"
#include "memory/arc.h"
#include "string/string.h"

#include <utility>

struct Task {
  Task()  = default;
  ~Task() = default;

  // non copyable
  Task(const Task&)            = delete;
  Task& operator=(const Task&) = delete;

  // movable
  Task(Task&& o) noexcept :
      fullPath(std::move(o.fullPath)), directoryLevel(std::exchange(o.directoryLevel, 0)),
      ignoreContext(std::move(o.ignoreContext)) {}

  Task& operator=(Task&& o) noexcept {
    if (this == &o)
      return *this;
    fullPath       = std::move(o.fullPath);
    directoryLevel = std::exchange(o.directoryLevel, 0);
    ignoreContext  = std::move(o.ignoreContext);
    return *this;
  }

  Task(const string::String& path, u32 dirLevel) noexcept :
      fullPath(path), directoryLevel(dirLevel), ignoreContext() {}

  Task(string::String&& path, u32 dirLevel) noexcept :
      fullPath(std::move(path)), directoryLevel(dirLevel), ignoreContext() {}

  Task(const string::String& path, u32 dirLevel,
      const memory::Arc<IgnoreContext>& ignoreContext) noexcept :
      fullPath(path), directoryLevel(dirLevel), ignoreContext(ignoreContext) {}

  Task(string::String&& path, u32 dirLevel, const memory::Arc<IgnoreContext>& ignoreContext) noexcept :
      fullPath(std::move(path)), directoryLevel(dirLevel), ignoreContext(ignoreContext) {}

  string::String             fullPath;
  u32                        directoryLevel;
  memory::Arc<IgnoreContext> ignoreContext;
};
