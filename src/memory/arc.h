#pragma once

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

      template <typename... Args>
      Block(Args&&... args) : val_(std::forward<Args>(args)...) {}
      Block(T&& val) : val_(std::move(val)) {}
    };

    Block* block_{nullptr};

    void   release() {
      if (block_ && block_->refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete block_;
      }
      block_ = nullptr;
    }

  public:
    Arc() noexcept = default;

    Arc(T&& val) { block_ = new Block(std::move(val)); }

    template <typename... Args>
    Arc(Args&&... args) {
      block_ = new Block(std::forward<Args>(args)...);
    }

    Arc(const Arc& o) : block_(o.block_) {
      if (block_) { block_->refCount_.fetch_add(1, std::memory_order_relaxed); }
    }

    Arc& operator=(const Arc& o) {
      if (this == &o) { return *this; }
      release();
      block_ = o.block_;
      if (block_) { block_->refCount_.fetch_add(1, std::memory_order_relaxed); }
      return *this;
    }

    Arc(Arc&& o) noexcept : block_(std::exchange(o.block_, nullptr)) {}

    Arc& operator=(Arc&& o) noexcept {
      if (this == &o) { return *this; }
      release();
      block_ = std::exchange(o.block_, nullptr);
      return *this;
    }

    ~Arc() { release(); }

    T* operator->() {
      assert(block_);
      return &block_->val_;
    }

    const T* operator->() const {
      assert(block_);
      return &block_->val_;
    }

    T& operator*() {
      assert(block_);
      return block_->val_;
    }

    const T& operator*() const {
      assert(block_);
      return block_->val_;
    }

    explicit operator bool() const noexcept { return block_ != nullptr; }

    bool     operator==(const Arc& o) const noexcept { return block_ == o.block_; }
  };
}
