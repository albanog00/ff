#pragma once

#include "types.h"

#include <cstddef>
#include <spdlog/spdlog.h>
#include <system_error>
#include <unistd.h>
#include <sys/mman.h>

namespace memory {
  inline u64 getPageSize() {
    static u64 pageSize = static_cast<u64>(sysconf(_SC_PAGESIZE));
    return pageSize;
  }

  inline u64 getCommitSize() {
    static u64 commitSize = getPageSize() << 10; // ~4MiB for 4KiB pages
    return commitSize;
  }

  inline u64 alignUp(u64 value, u64 alignment = alignof(std::max_align_t)) {
    assert((alignment & (alignment - 1)) == 0 && "alignment now power of 2");
    return (value + alignment - 1) & ~(alignment - 1);
  }

  struct Arena {
  private:
    struct ChunkList {
      u64        startOffset;
      u64        size;
      u64        alignment;
      ChunkList* next{nullptr};
    };

    u8* block_{nullptr};
    // ChunkList* freeList{nullptr};
    u64              reservedSize{0};
    std::atomic<u64> committedSize{0};
    std::atomic<u64> currentOffset{0};
    std::mutex       commitMutex;

    bool             commit(u64 newOffset) {
      std::scoped_lock lk(commitMutex);

      u64              committed = committedSize.load(std::memory_order_acquire);
      if (committed >= newOffset) { return true; }

      u64 commitTarget = alignUp(newOffset, getCommitSize());
      commitTarget     = std::min(commitTarget, reservedSize);

      void* commitStartAddr = block_ + committed;
      u64   sizeToCommit    = commitTarget - committed;

      if (mprotect(commitStartAddr, sizeToCommit, PROT_READ | PROT_WRITE) != 0) {
        spdlog::error("mprotect allocation failed");
        assert(false);
        return false;
      }

      committedSize.store(commitTarget, std::memory_order_release);
      return true;
    }

  public:
    Arena() = default;

    // allocate `reserveSize` of virtual memory addressed
    Arena(u64 reserveSize = MiB(64)) {
      const u64 pageSize = getPageSize();
      reserveSize        = alignUp(reserveSize, pageSize);

      // reserve memory addresses
      u8* block = (u8*)mmap(NULL, reserveSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

      if (block == MAP_FAILED) {
        spdlog::error("mmap failed");
        throw std::system_error(errno, std::generic_category(), "mmap failed");
      }

      // allocate pageSize (likely 4KiB)
      if (mprotect(block, pageSize, PROT_READ | PROT_WRITE) != 0) {
        munmap(block, reserveSize);
        spdlog::error("mprotect allocation failed");
        throw std::system_error(errno, std::generic_category(), "mprotect failed");
      }

      block_        = block;
      reservedSize  = reserveSize;
      committedSize = pageSize;
      currentOffset = 0;
    }

    ~Arena() { munmap((void*)block_, reservedSize); }

    void* alloc(u64 size, u64 alignment = alignof(std::max_align_t)) {
      assert((alignment & (alignment - 1)) == 0 && "alignment now power of 2");
      if (size == 0) { return nullptr; }

      u64 current = currentOffset.load(std::memory_order_relaxed);
      while (true) {
        u64 alignedOffset = alignUp(current, alignment);
        u64 newOffset     = alignedOffset + size;

        if (newOffset > reservedSize) { return nullptr; }
        if (newOffset > committedSize.load(std::memory_order_acquire)) {
          if (!commit(newOffset)) { return nullptr; }
        }

        if (currentOffset.compare_exchange_weak(
                current, newOffset, std::memory_order_acq_rel, std::memory_order_relaxed)) {
          return static_cast<void*>(block_ + alignedOffset);
        }
      }

      return nullptr;
    }

    // void        release(T*, u64) {}

    inline bool owns(const void* ptr) const {
      return ptr < block_ + currentOffset.load(std::memory_order_relaxed) && ptr >= block_;
    }
  };
}
