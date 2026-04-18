#pragma once

#include "memory/arena.h"
#include "memory/types.h"
#include "queue/blockingconcurrentqueue.h"

#include <string>
#include <thread>

using namespace memory;

static inline const u32 nCores   = std::thread::hardware_concurrency();
static inline const u32 nWorkers = 0;
// std::max(1u, static_cast<u32>(std::floor(static_cast<f32>(nCores) * 0.25f)));
static inline const u32 nExplorers = nCores - nWorkers;

using Task = std::string;

inline Arena arena{GiB(1)};

struct QueueTraits {
  typedef std::size_t        size_t;
  typedef std::size_t        index_t;
  static const size_t        BLOCK_SIZE                                        = 32;
  static const size_t        EXPLICIT_BLOCK_EMPTY_COUNTER_THRESHOLD            = 32;
  static const size_t        EXPLICIT_INITIAL_INDEX_SIZE                       = 32;
  static const size_t        IMPLICIT_INITIAL_INDEX_SIZE                       = 32;
  static const size_t        INITIAL_IMPLICIT_PRODUCER_HASH_SIZE               = 32;
  static const std::uint32_t EXPLICIT_CONSUMER_CONSUMPTION_QUOTA_BEFORE_ROTATE = 256;
  static const size_t MAX_SUBQUEUE_SIZE = moodycamel::details::const_numeric_max<size_t>::value;
  static const int    MAX_SEMA_SPINS    = 10000;
  static const bool   RECYCLE_ALLOCATED_BLOCKS = false;
  static inline void* malloc(size_t size) { return arena.alloc(size); }
  static inline void  free(void*) { return; }
};

using BlockingQueue = moodycamel::BlockingConcurrentQueue<Task>;
