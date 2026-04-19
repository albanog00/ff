#pragma once

#include "memory/types.h"
#include "queue/blockingconcurrentqueue.h"
#include "queue/concurrentqueue.h"
#include "string/string.h"

#include <thread>

static inline const u32 nCores     = std::thread::hardware_concurrency();
static inline const u32 nExplorers = nCores;

struct QueueTraits : moodycamel::ConcurrentQueueDefaultTraits {
  static inline memory::Arena arena{GiB(1)};

  static const bool           RECYCLE_ALLOCATED_BLOCKS = true;
  static inline void*         malloc(size_t size) { return arena.alloc(size); }
  static inline void          free(void*) { return; }
};

using BlockingQueue = moodycamel::BlockingConcurrentQueue<string::String, QueueTraits>;
