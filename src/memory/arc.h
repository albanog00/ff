#pragma once

#include "memory/arena.h"
#include "types.h"

#include <cassert>
#include <atomic>
#include <utility>

namespace memory {
  template <typename T>
  struct Arc {
  private:
    struct Block {
      T                val_;
      std::atomic<u32> refCount_{1};

      Block(const T& val) : val_(val) {}

      // non copyable/movable
      Block(const Block&)            = delete;
      Block(Block&&)                 = delete;
      Block& operator=(const Block&) = delete;
      Block& operator=(Block&&)      = delete;

      template <typename... Args>
      Block(Args&&... args) : val_(std::forward<Args>(args)...) {}
      Block(T&& val) : val_(std::move(val)) {}
    };

    Block* block_{nullptr};

    void   release() {
      if (block_ && block_->refCount_.fetch_sub(1, std::memory_order_relaxed) == 1) {
        // release pointer
      }
      block_ = nullptr;
    }

  public:
    Arc() noexcept = default;

    template <typename... Args>
      requires(!(sizeof...(Args) == 1 && (std::same_as<std::remove_cvref_t<Args>, Arc> && ...)))
    Arc(Args&&... args) {
      block_ =
          new (getArena().alloc(sizeof(Block), alignof(Block))) Block(std::forward<Args>(args)...);
    }

    Arc(T&& val) {
      block_ = new (getArena().alloc(sizeof(Block), alignof(Block))) Block(std::move(val));
    }

    // copy
    Arc(const Arc& o) {
      block_ = o.block_;
      if (block_) { block_->refCount_.fetch_add(1, std::memory_order_relaxed); }
    }

    Arc& operator=(const Arc& o) {
      if (this == &o) { return *this; }
      block_ = o.block_;
      if (block_) { block_->refCount_.fetch_add(1, std::memory_order_relaxed); }
      return *this;
    }

    // move
    Arc(Arc&& o) noexcept : block_(std::exchange(o.block_, nullptr)) {}

    Arc& operator=(Arc&& o) noexcept {
      if (this == &o) { return *this; }
      release();
      block_ = std::exchange(o.block_, nullptr);
      return *this;
    }

    ~Arc() { release(); }

    u32 count() const { return block_ ? block_->refCount_.load(std::memory_order_relaxed) : 0; }

    T*  operator->() { return block_ ? &block_->val_ : nullptr; }
    const T* operator->() const { return block_ ? &block_->val_ : nullptr; }
    T&       operator*() { return block_ ? block_->val_ : nullptr; }
    const T& operator*() const { return block_ ? block_->val_ : nullptr; }

    explicit operator bool() const noexcept { return block_ != nullptr; }
    bool     operator==(const Arc& o) const noexcept { return block_ == o.block_; }
  };
}
