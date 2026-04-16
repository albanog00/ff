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

  inline u64 alignUp(u64 value, u64 alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }

  struct Arena {
    u64 reservedSize;
    u64 committedSize;
    u64 currentOffset;

    // allocate `reserveSize` of virtual memory addressed
    static Arena* alloc(u64 reserveSize) {
      const u64 pageSize = getPageSize();
      reserveSize        = alignUp(reserveSize, pageSize);

      // reserve memory addresses
      void* block = mmap(NULL, static_cast<size_t>(reserveSize), PROT_NONE,
          MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

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

      // Arena is part of the allocated block itself
      // right at the start of the block
      Arena* arena = new (block) Arena{
          .reservedSize  = reserveSize,
          .committedSize = pageSize,
          .currentOffset = alignUp(sizeof(Arena), alignof(Arena)),
      };
      return arena;
    }

    void release() { munmap(reinterpret_cast<void*>(this), reservedSize); }
  };
}
