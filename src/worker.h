#pragma once

#include "constants.h"
#include "queue/blockingconcurrentqueue.h"
#include "queue/concurrentqueue.h"

#include <vector>
#include <thread>

struct WorkerContext {
public:
  WorkerContext();

  inline bool enqueue(Task val) {
    if (queue.enqueue(std::move(val))) {
      pendingWork.fetch_add(1, std::memory_order_relaxed);
      return true;
    }
    return false;
  }

  inline bool enqueueBulk(const std::span<Task>& span) {
    return queue.enqueue_bulk(std::make_move_iterator(span.begin()), span.size());
  }

  inline void dequeue(Task& out) { return queue.wait_dequeue(out); }

  inline void poison() {
    for (u32 i = 0; i < threads.size(); ++i) { enqueue(POISON); }
  }

  inline void spawn(u32 count) {
    for (u32 i = 0; i < count; ++i) {
      threads.emplace_back(std::thread([this] { work(); }));
    }
  }

  inline void join() {
    stopping.store(true, std::memory_order_release);
    if (pendingWork.load(std::memory_order_acquire) == 0) { poison(); }
    for (auto& t : threads) { t.join(); }
  }

  static inline const Task POISON{"WORKER_POISON"};

private:
  moodycamel::BlockingConcurrentQueue<Task> queue{8 << 10};
  std::vector<std::thread>                  threads{};
  std::atomic<u32>                          pendingWork{0};
  std::atomic<bool>                         stopping{false};

  void                                      work();
};
