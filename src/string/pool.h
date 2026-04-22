#pragma once

#include "memory/types.h"
#include "memory/arena.h"
#include <bit>
#include <type_traits>

namespace string {
  inline u32 upperPowerOfTwo(u32 x) { return std::bit_ceil(x); }

  template <typename T>
    requires std::is_arithmetic_v<T>
  struct StringPool {
  private:
    static constexpr u32 Alignment = 64;
    static constexpr u32 MinBlock  = 64;
    static constexpr u32 MaxBlock  = 4096;
    static constexpr u32 Buckets   = 7;

    struct Node {
      Node* next;
    };

    static inline thread_local Node* freeLists[Buckets]{};

    inline u32                       getBucketIdx(u32 capacity) {
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
      }
    }

    T* acquire(u32 size, u32& outCapacity) {
      size += 1; // reserve space for null-terminator

      outCapacity = std::max(MinBlock, upperPowerOfTwo(size));
      if (outCapacity <= MaxBlock) {
        u32 idx = getBucketIdx(outCapacity);
        // get next free node if available
        Node* head = freeLists[idx];
        if (head) {
          freeLists[idx] = head->next;
          return reinterpret_cast<T*>(head);
        }
      }
      // no free node or allocation > MaxBlock
      // fallback and alloc on arena
      return reinterpret_cast<T*>(memory::getArena().alloc(outCapacity, Alignment));
    }

    void release(T* ptr, u32 capacity) {
      if (!ptr || !memory::getArena().owns(ptr)) { return; }
      if (capacity < MinBlock || capacity > MaxBlock) { return; }
      if (!std::has_single_bit(capacity)) { return; }
      u32   idx      = getBucketIdx(capacity);
      Node* node     = reinterpret_cast<Node*>(ptr);
      Node* head     = freeLists[idx];
      node->next     = head;
      freeLists[idx] = node;
    }
  };
}
