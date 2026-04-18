#pragma once

#include "memory/types.h"
#include "memory/arena.h"

using namespace memory;

struct String8 {
  using StorageType = u8;

private:
  StorageType*  data_{nullptr};
  u64           size_{0};

  static Arena& memory() {
    thread_local static Arena arena{MiB(64)};
    return arena;
  }

public:
  String8() = default;

  explicit String8(u64 size) :
      data_(static_cast<StorageType*>(memory().alloc(size + 1))), size_(size) {
    data_[size] = 0;
  }

  ~String8() { release(); }

  // non-copyable
  String8(const String8&)            = delete;
  String8& operator=(const String8&) = delete;

  // movable
  String8(String8&& o) noexcept :
      data_(std::exchange(o.data_, nullptr)), size_(std::exchange(o.size_, 0)) {}
  String8& operator=(String8&& o) noexcept {
    if (this == &o) { return *this; }
    release();

    data_ = std::exchange(o.data_, nullptr);
    size_ = std::exchange(o.size_, 0);
    return *this;
  }

  u64            size() const { return size_; }

  static String8 from(const u8* buf, u64 len) {
    if (!buf || len == 0) { return String8{}; }

    String8 str(len);
    memcpy(str.data_, buf, len);

    return str;
  }

  static String8 from(const char* cstr) {
    if (!cstr) return String8::from(nullptr, 0);
    return from(reinterpret_cast<const StorageType*>(cstr), strlen(cstr));
  }

  static String8 from(const std::string& str) {
    return from(reinterpret_cast<const StorageType*>(str.data()), str.size());
  }

  void release() {
    if (data_) {
      // memory().release(data_, size_);
    }
  }
};
