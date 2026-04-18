#pragma once

#include "constants.h"

#include <vector>
#include <thread>

struct ExplorerContext {
public:
  ExplorerContext(std::string startPath);

  inline bool enqueue(Task val) { return queue.enqueue(std::move(val)); }

  inline bool tryEnqueue(Task val) { return queue.try_enqueue(std::move(val)); }

  inline bool enqueueBulk(const std::span<Task>& span) {
    return queue.enqueue_bulk(std::make_move_iterator(span.begin()), span.size());
  }

  inline u32 dequeueBulk(const std::span<Task>& span) {
    return queue.wait_dequeue_bulk(span.begin(), span.size());
  }

  inline void dequeue(Task& out) { return queue.wait_dequeue(out); }

  inline void poison() {
    for (u32 i = 0; i < threads.size(); ++i) { enqueue(POISON); }
  }

  inline void join() {
    for (auto& t : threads) { t.join(); }
  }

  static inline const Task POISON;

private:
  BlockingQueue            queue{8 << 10};
  std::vector<std::thread> threads{};
  std::atomic<u32>         dirsInFlight{1};

  void                     walk();
};
