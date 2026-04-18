#pragma once

#include "memory/arena.h"
#include "types.h"

#include <atomic>
#include <mutex>
#include <new>
#include <cassert>
#include <type_traits>

namespace memory {
  template <typename T>
  struct Pool {
    const u64 BlockSize = 64;

  private:
    struct Node {
      alignas(T) u8 storage[sizeof(T)];
      Node*        next;

      inline T*    val() { return std::launder(reinterpret_cast<T*>(storage)); }
      static Node* from(T* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<u8*>(ptr) - offsetof(Node, storage));
      }
    };

    Arena              arena_{MiB(64)};
    std::atomic<Node*> freeList_{nullptr};
    std::mutex         growMutex_;
    std::atomic<u64>   count_{BlockSize};

  private:
    Node* allocateBlock() {
      Node* block = reinterpret_cast<Node*>(arena_.alloc(sizeof(Node) * BlockSize, alignof(Node)));
      for (u64 i = 0; i < BlockSize - 1; ++i) { block[i].next = &block[i + 1]; }
      block[BlockSize - 1].next = nullptr;
      return block;
    }

    void pushBlock(Node* block) {
      Node* tail = block;
      while (tail->next) { tail = tail->next; }
      Node* head = freeList_.load(std::memory_order_acquire);
      do {
        tail->next = head;
      } while (!freeList_.compare_exchange_weak(
          head, block, std::memory_order_release, std::memory_order_relaxed));
      count_.fetch_add(BlockSize, std::memory_order_release);
    }

  public:
    Pool() : freeList_(allocateBlock()) {}

    ~Pool() = default;

    T* acquire() {
      while (true) {
        Node* head = freeList_.load(std::memory_order_acquire);

        if (head) {
          Node* next = head->next;
          if (freeList_.compare_exchange_weak(
                  head, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return head->val();
          }
        } else {
          // slow path: grow
          std::scoped_lock lk(growMutex_);
          head = freeList_.load(std::memory_order_acquire);
          if (head) { continue; }

          Node* block = allocateBlock();
          pushBlock(block);
        }
      }
    }

    void release(T* ptr) {
      assert(ptr && arena_.owns(reinterpret_cast<void*>(ptr)));
      if constexpr (std::is_trivially_destructible_v<T>) { std::destroy_at(ptr); }

      Node* node = Node::from(ptr);
      Node* head = freeList_.load(std::memory_order_acquire);

      do {
        node->next = head;
      } while (!freeList_.compare_exchange_weak(
          head, node, std::memory_order_acq_rel, std::memory_order_acquire));
    }
  };
} // namespace memory
