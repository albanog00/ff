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
    u64  reservedSize{0};
    u64  committedSize{0};
    u64  currentOffset{0};

    bool commit(u64 newOffset) {
      assert(newOffset >= currentOffset);

      const u64 pageSize     = getPageSize();
      u64       commitTarget = alignUp(newOffset, pageSize);
      if (commitTarget > reservedSize) { commitTarget = reservedSize; }

      u64   sizeToCommit    = commitTarget - committedSize;
      void* commitStartAddr = block_ + committedSize;
      if (mprotect(commitStartAddr, sizeToCommit, PROT_READ | PROT_WRITE) != 0) {
        spdlog::error("mprotect allocation failed");
        return false;
      }

      committedSize = commitTarget;
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

      u64 alignedOffset = alignUp(currentOffset, alignment);
      u64 newOffset     = alignedOffset + size;

      if (newOffset > reservedSize) { return nullptr; } //  out of memory
      if (newOffset > committedSize && !commit(newOffset)) { return nullptr; }

      currentOffset = newOffset;

      return static_cast<void*>(block_ + alignedOffset);
    }

    // void        release(T*, u64) {}

    inline bool owns(const void* ptr) const {
      return ptr < block_ + currentOffset && ptr >= block_;
    }
  };
}
