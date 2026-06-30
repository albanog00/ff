#pragma once

#include "memory/types.h"
#include "memory/arena.h"
#include <array>
#include <cassert>
#include <atomic>
#include <bit>
#include <spdlog/spdlog.h>
#include <type_traits>
#include <vector>

namespace string {
  inline u32 upperPowerOfTwo(u32 x) { return std::bit_ceil(x); }

  template <typename T>
    requires std::is_arithmetic_v<T>
  struct StringPool {
  private:
    static constexpr u32 Alignment = 64;
    static constexpr u32 MinBlock  = 64;
    static constexpr u32 MaxBlock  = 1024;
    static constexpr u32 Buckets   = 5;

    struct Node {
      Node* next;
    };

    memory::Arena                           internalStorage{MiB(64)};
    std::array<std::atomic<Node*>, Buckets> freeLists{};
#if DEBUG
    std::array<u32, Buckets> count{};
#endif

    inline u32 getBucketIdx(u32 capacity) {
      u32 idx = 0;
      u32 val = MinBlock;
      while (val < capacity) {
        val <<= 1;
        idx += 1;
      }
      return idx;
    }

  public:
    StringPool(u32 initialCapacityPerBucket = 4) {
      u32 blockSize = MinBlock;
      u32 totalMemory =
          MaxBlock * initialCapacityPerBucket * 2 - (MinBlock * initialCapacityPerBucket);
      u8* memory        = reinterpret_cast<u8*>(internalStorage.alloc(totalMemory, Alignment));
      u32 currentOffset = 0;

      for (u32 i = 0; i < Buckets; ++i, blockSize <<= 1) {
        Node* head = nullptr;

        for (u32 j = 0; j < initialCapacityPerBucket; ++j) {
          Node* node = reinterpret_cast<Node*>(memory + currentOffset);
          node->next = head;
          head       = node;
          currentOffset += blockSize;
        }

        freeLists[i].store(head, std::memory_order_relaxed);
#if DEBUG
        count[i] = initialCapacityPerBucket;
#endif
      }
    }

    T* acquire(u32 size, u32& outCapacity) {
      size += 1; // reserve space for null-terminator

      outCapacity = std::max(MinBlock, upperPowerOfTwo(size));
      u32 idx     = getBucketIdx(outCapacity);

      if (outCapacity <= MaxBlock) {
        // get next free node if available
        Node* head = freeLists[idx].load(std::memory_order_acquire);
        while (head) {
          assert(internalStorage.owns(head));
          assert(reinterpret_cast<uintptr_t>(head) % alignof(Node) == 0);
          Node* next = head->next;
          if (freeLists[idx].compare_exchange_weak(
                  head, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return reinterpret_cast<T*>(head);
          }
        }
#if DEBUG
        count[idx] += 1;
#endif
      }
      // no free node or allocation > MaxBlock
      // fallback and alloc on arena
      return reinterpret_cast<T*>(internalStorage.alloc(outCapacity, Alignment));
    }

    void release(T* ptr, u32 capacity) {
      if (!ptr || !internalStorage.owns(ptr)) {
        return;
      }
      if (capacity < MinBlock) {
        return;
      }
      if (capacity > MaxBlock) {
        return;
      }

      Node* node = reinterpret_cast<Node*>(ptr);
      u32   idx  = getBucketIdx(capacity);
      Node* head = freeLists[idx].load(std::memory_order_acquire);
      do {
        node->next = head;
      } while (!freeLists[idx].compare_exchange_weak(
          head, node, std::memory_order_acq_rel, std::memory_order_acquire));
    }

#if DEBUG
    void dumpStats() {
      spdlog::debug("StringPool=(thread_id={}, blocks=(64bytes={}, 128bytes={}, "
                    "256bytes={}, 512bytes={}, 1024bytes={}))",
          pthread_self(), count[0], count[1], count[2], count[3], count[4]);
    }
#endif
  };

  inline StringPool<u8>& getStringPool() {
    static std::mutex                      registryMutex;
    static std::vector<UP<StringPool<u8>>> pools;

    thread_local StringPool<u8>*           pool = [&] {
      std::lock_guard lk(registryMutex);
      pools.emplace_back(std::make_unique<StringPool<u8>>());
      return pools.back().get();
    }();

    return *pool;
  }
}
