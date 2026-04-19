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
    static constexpr u32 Alignment = 64;
    static constexpr u32 MinBlock  = 64;
    static constexpr u32 MaxBlock  = 4096;
    static constexpr u32 Buckets   = 7;

    struct Node {
      Node* next;
    };

    StringPool(u32 initialCapacityPerBucket = 16) {
      u32 blockSize = MinBlock;
      for (u32 i = 0; i < Buckets; ++i, blockSize <<= 1) {
        Node* head = nullptr;
        for (u32 j = 0; j < initialCapacityPerBucket; ++j) {
          Node* node = reinterpret_cast<Node*>(arena.alloc(blockSize, Alignment));
          node->next = head;
          head       = node;
        }
        freeLists[i] = head;
      }
    }

    memory::Arena                    arena{GiB(1)};
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
      return reinterpret_cast<T*>(arena.alloc(outCapacity, Alignment));
    }

    void release(T* ptr, u32 capacity) {
      if (!ptr || !arena.owns(ptr)) { return; }
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
