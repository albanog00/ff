#pragma once

#include "memory/types.h"
#include "memory/arena.h"
#include <bit>
#include <spdlog/spdlog.h>
#include <type_traits>

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

    static inline thread_local std::array<Node*, Buckets> freeLists{};
#if DEBUG
    static inline thread_local std::array<u32, Buckets> count{0};
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
    StringPool(u32 initialCapacityPerBucket = 16) {
      u32 blockSize = MinBlock;
      u32 totalMemory =
          MaxBlock * initialCapacityPerBucket * 2 - (MinBlock * initialCapacityPerBucket);
      u8* memory        = reinterpret_cast<u8*>(memory::getArena().alloc(totalMemory, Alignment));
      u32 currentOffset = 0;

      for (u32 i = 0; i < Buckets; ++i, blockSize <<= 1) {
        Node* head = nullptr;

        for (u32 j = 0; j < initialCapacityPerBucket; ++j) {
          Node* node = reinterpret_cast<Node*>(memory + currentOffset);
          node->next = head;
          head       = node;
          currentOffset += blockSize;
        }

        freeLists[i] = head;
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
        Node* head = freeLists[idx];
        if (head) {
          freeLists[idx] = head->next;
          return reinterpret_cast<T*>(head);
        }
      }
      // no free node or allocation > MaxBlock
      // fallback and alloc on arena

#if DEBUG
      if (idx < Buckets) { count[idx] += 1; }
#endif
      return reinterpret_cast<T*>(memory::getArena().alloc(outCapacity, Alignment));
    }

    void release(T* ptr, u32 capacity) {
      if (!ptr || !memory::getArena().owns(ptr)) { return; }
      if (capacity < MinBlock) { return; }
      if (capacity > MaxBlock) {
        // there are no bucket available to handle a block of this size.
        // we split it in multiple middle-sized blocks
        i32              count128  = capacity >> 8;
        static const u32 bucket128 = 1;
        Node*            head128   = freeLists[bucket128];
        T*               block     = ptr;
        while (--count128 >= 0) {
          Node* node = reinterpret_cast<Node*>(block);
          node->next = head128;
          head128    = node;
          block += 128;
        }
        freeLists[bucket128] = head128;
#if DEBUG
        count[bucket128] += count128;
#endif
        return;
      }

      Node* node     = reinterpret_cast<Node*>(ptr);
      u32   idx      = getBucketIdx(capacity);
      Node* head     = freeLists[idx];
      node->next     = head;
      freeLists[idx] = node;
    }

#if DEBUG
    void dumpStats() {
      spdlog::debug("StringPool=(thread_id={}, blocks=(64bytes={}, 128bytes={}, "
                    "256bytes={}, 512bytes={}, 1024bytes={}))",
          pthread_self(), count[0], count[1], count[2], count[3], count[4]);
    }
#endif
  };

  static thread_local inline StringPool<u8> u8StringPool;
}
