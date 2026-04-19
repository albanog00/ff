#pragma once

#include "constants.h"

#include <vector>
#include <thread>

inline string::String POISON = string::String::from("Poison");

struct ExplorerContext {

public:
  ExplorerContext(std::string startPath);

  inline bool enqueue(string::String&& val) { return queue.enqueue(std::move(val)); }

  inline bool tryEnqueue(string::String&& val) { return queue.try_enqueue(std::move(val)); }

  inline bool enqueueBulk(std::vector<string::String>& vec) {
    return queue.enqueue_bulk(std::make_move_iterator(vec.begin()), vec.size());
  }

  inline u32 dequeueBulk(const std::span<string::String>& span) {
    return queue.wait_dequeue_bulk(span.begin(), span.size());
  }

  inline void dequeue(string::String& out) { return queue.wait_dequeue(out); }

  inline void poison() {
    string::String copy = string::String::from(POISON);
    enqueue(std::move(copy));
  }

  inline void join() {
    for (auto& t : threads) { t.join(); }
  }

private:
  BlockingQueue            queue{8 << 10};
  std::vector<std::thread> threads{};
  std::atomic<u32>         dirsInFlight{1};

  void                     walk();
};
