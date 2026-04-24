#pragma once

#include "memory/types.h"
#include "pool.h"

#include <cstring>
#include <string>
#include <utility>

namespace string {
  struct String {
  private:
    u8*                          data_{nullptr};
    u32                          capacity_{0};
    u32                          size_{0};

    static inline StringPool<u8> pool{};

    void                         alloc(u32 size) {
      data_    = pool.acquire(size, capacity_);
      size_    = 0;
      data_[0] = 0;
    }

    void release() {
      if (data_) { pool.release(data_, capacity_); }
      data_     = nullptr;
      size_     = 0;
      capacity_ = 0;
    }

    void realloc(u32 minSize) {
      u32 newCapacity = 0;
      u32 size        = size_;
      u8* newData     = pool.acquire(minSize, newCapacity);

      if (size_ > 0) { memcpy(newData, data_, size); }
      newData[size] = 0;

      release();

      data_     = newData;
      capacity_ = newCapacity;
      size_     = size;
    }

    void copy(const String& o) {
      if (capacity_ <= o.size_) {
        release();
        alloc(o.size_);
      }

      size_ = o.size_;
      if (o.data_) { memcpy(data_, o.data_, o.size_); }
      data_[size_] = 0;
    }

  public:
    String() noexcept = default;

    explicit String(u32 initialCapacity) { alloc(initialCapacity); }

    ~String() { release(); }

    // copyable
    String(const String& o) noexcept { copy(o); }

    String& operator=(const String& o) noexcept {
      if (this == &o) { return *this; }
      copy(o);
      return *this;
    }

    // movable
    String(String&& o) noexcept :
        data_(std::exchange(o.data_, nullptr)), capacity_(std::exchange(o.capacity_, 0)),
        size_(std::exchange(o.size_, 0)) {}

    String& operator=(String&& o) noexcept {
      if (this == &o) { return *this; }
      release();
      data_     = std::exchange(o.data_, nullptr);
      size_     = std::exchange(o.size_, 0);
      capacity_ = std::exchange(o.capacity_, 0);
      return *this;
    }

    u32              size() const { return size_; }
    const u8*        data() const { return data_; }
    const char*      c_str() const { return data_ ? reinterpret_cast<const char*>(data_) : ""; }
    std::string      string() const { return data_ ? std::string{c_str(), size_} : ""; }
    std::string_view view() const { return data_ ? std::string_view{c_str(), size_} : ""; }

    static String    from(const u8* buf, u32 size) {
      if (!buf || size == 0) { return String{}; }
      String str(size);
      memcpy(str.data_, buf, size);
      str.data_[size] = 0;
      str.size_       = size;
      return str;
    }

    static String from(const String& str) { return String::from(str.data_, str.size_); }

    static String from(const char* cstr) {
      if (!cstr) return String::from(nullptr, 0);
      return from(reinterpret_cast<const u8*>(cstr), strlen(cstr));
    }

    static String from(const std::string& str) {
      return from(reinterpret_cast<const u8*>(str.data()), str.size());
    }

    inline bool equals(const u8* buf, u32 size) const {
      if (size_ == 0) return size_ == size;
      return size_ == size && memcmp(data_, buf, size) == 0;
    }

    inline bool operator==(const String& o) const { return equals(o.data_, o.size_); }
    inline bool operator!=(const String& o) const { return !(*this == o); }

    inline bool operator==(const char* o) const {
      if (!o) return size_ == 0;
      return equals(reinterpret_cast<const u8*>(o), strlen(o));
    }
    inline bool operator!=(const char* o) const { return !(*this == o); }

    inline bool operator==(const std::string& o) const {
      return equals(reinterpret_cast<const u8*>(o.data()), o.size());
    }
    inline bool operator!=(const std::string& o) const { return !(*this == o); }

    inline bool startsWith(const char* prefix) const {
      if (!prefix) { return size_ == 0; }
      u32 len = strlen(prefix);
      return size_ >= len && memcmp(data_, prefix, len) == 0;
    }

    inline void clear() {
      size_ = 0;
      if (data_) { data_[0] = 0; }
    }

    void append(const u8* o, u32 size) {
      if (size_ + size >= capacity_) { realloc(size_ + size); }
      memmove(data_ + size_, o, size);
      size_ += size;
      data_[size_] = 0;
    }

    void        append(const char* o, u32 size) { append(reinterpret_cast<const u8*>(o), size); }
    void        append(const String& o) { append(o.data_, o.size_); }
    void        append(const char* o) { append(o, strlen(o)); }
    void        append(const std::string& o) { append(o.data(), o.size()); }
    void        append(const std::string_view& o) { append(o.data(), o.size()); }

    void        operator+=(const String& o) { append(o); }
    void        operator+=(const char* o) { append(o); }
    void        operator+=(const std::string& o) { append(o); }
    void        operator+=(const std::string_view& o) { append(o); }

    inline bool empty() const { return size_ == 0; }

    inline u8   back() const {
      if (data_ && size_ > 0) { return data_[size_ - 1]; }
      return 0;
    }

    inline u8 front() const {
      if (data_ && size_ > 0) { return data_[0]; }
      return 0;
    }
  };
}
