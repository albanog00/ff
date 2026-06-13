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
      fullPath(std::move(o.fullPath)), directoryLevel(std::exchange(o.directoryLevel, 0)) {}

  Task& operator=(Task&& o) noexcept {
    if (this == &o) { return *this; }
    fullPath       = std::move(o.fullPath);
    directoryLevel = std::exchange(o.directoryLevel, 0);
    return *this;
  }

  Task(const string::String& path, u32 dirLevel) noexcept :
      fullPath(path), directoryLevel(dirLevel) {}
  Task(string::String&& path, u32 dirLevel) noexcept :
      fullPath(std::move(path)), directoryLevel(dirLevel) {}

  string::String fullPath;
  u32            directoryLevel;
};
