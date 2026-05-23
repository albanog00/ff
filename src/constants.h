#pragma once

#include "memory/types.h"
#include "memory/arena.h"
#include "queue/blockingconcurrentqueue.h"
#include "queue/concurrentqueue.h"
#include "task.h"

#include <thread>

static inline const u32 nCores     = std::thread::hardware_concurrency();
static inline const u32 nExplorers = nCores;

struct QueueTraits : moodycamel::ConcurrentQueueDefaultTraits {
  static const bool   RECYCLE_ALLOCATED_BLOCKS = true;
  static inline void* malloc(size_t size) { return memory::getSharedMemory().alloc(size); }
  static inline void  free(void*) { return; }
};

using BlockingQueue = moodycamel::BlockingConcurrentQueue<Task, QueueTraits>;

template <typename F>
struct privDefer {
  F f;
  privDefer(F f) : f(f) {}
  ~privDefer() { f(); }
};

template <typename F>
privDefer<F> defer_func(F f) {
  return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&]() { code; })
