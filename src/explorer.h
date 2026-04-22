#pragma once

#include "constants.h"

#include <vector>
#include <thread>

inline string::String POISON = string::String::from("Poison");

struct ExplorerContext {

public:
  ExplorerContext(string::String startPath);

  inline bool enqueue(Task&& val) { return queue.enqueue(std::move(val)); }

  inline bool tryEnqueue(Task&& val) { return queue.try_enqueue(std::move(val)); }

  inline bool enqueueBulk(std::vector<Task>& vec) {
    return queue.enqueue_bulk(std::make_move_iterator(vec.begin()), vec.size());
  }

  inline u32 dequeueBulk(const std::span<Task>& span) {
    return queue.wait_dequeue_bulk(span.begin(), span.size());
  }

  inline void dequeue(Task& out) { return queue.wait_dequeue(out); }

  inline void poison() { enqueue(Task{POISON, 0}); }

  inline void join() {
    for (auto& t : threads) { t.join(); }
  }

private:
  BlockingQueue            queue{8 << 10};
  std::vector<std::thread> threads{};
  std::atomic<u32>         dirsInFlight{1};

  void                     walk();
};
