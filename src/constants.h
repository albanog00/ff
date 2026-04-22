#pragma once

#include "memory/types.h"
#include "queue/blockingconcurrentqueue.h"
#include "queue/concurrentqueue.h"
#include "string/string.h"

#include <thread>

static inline const u32 nCores     = std::thread::hardware_concurrency();
static inline const u32 nExplorers = nCores;

struct QueueTraits : moodycamel::ConcurrentQueueDefaultTraits {
  static const bool   RECYCLE_ALLOCATED_BLOCKS = true;
  static inline void* malloc(size_t size) { return memory::getArena().alloc(size); }
  static inline void  free(void*) { return; }
};

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

using BlockingQueue = moodycamel::BlockingConcurrentQueue<Task, QueueTraits>;
