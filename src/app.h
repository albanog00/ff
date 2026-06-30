#pragma once

#include "memory/types.h"
#include "string/string.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

enum Search : u8 {
  Linear = 0,
  Regex  = 1,
};

struct FileType {
  enum FileTypeValue : u8 {
    None      = 1 << 0,
    File      = 1 << 1,
    Directory = 1 << 2,
  };

  bool     operator>(u8 o) const { return static_cast<u8>(value) > o; }
  bool     operator>=(u8 o) const { return static_cast<u8>(value) >= o; }
  bool     operator<(u8 o) const { return static_cast<u8>(value) < o; }
  bool     operator<=(u8 o) const { return static_cast<u8>(value) <= o; }

  FileType operator&(const FileType& o) const {
    return FileType{static_cast<FileTypeValue>(static_cast<u8>(value) & static_cast<u8>(o.value))};
  }

  void operator&=(const FileType& o) {
    value = static_cast<FileTypeValue>(static_cast<u8>(value) & static_cast<u8>(o.value));
  }

  FileType operator|(const FileType& o) const {
    return FileType{static_cast<FileTypeValue>(static_cast<u8>(value) | static_cast<u8>(o.value))};
  }

  void operator|=(const FileType& o) {
    value = static_cast<FileTypeValue>(static_cast<u8>(value) | static_cast<u8>(o.value));
  }

  FileType() = default;
  constexpr FileType(FileTypeValue val) : value(val) {}

  constexpr bool  operator==(FileType a) const { return value == a.value; }
  constexpr bool  operator!=(FileType a) const { return value != a.value; }

  static FileType parse(std::string_view view) {
    if (view == "directory" || view == "dir" || view == "d") {
      return {FileTypeValue::Directory};
    } else if (view == "file" || view == "f") {
      return {FileTypeValue::File};
    }
    spdlog::error("unknown filetype `{}`", view);
    exit(1);
    return {FileTypeValue::None};
  }

private:
  FileTypeValue value{FileTypeValue::None};
};

struct App {
  ~App() {
    if (pattern) {
      pcre2_code_free(pattern);
    }
  }

  u32                         maxDepth{0};
  bool                        pipe{false};
  bool                        hidden{false};
  FileType                    type{FileType::None};

  pcre2_code*                 pattern{nullptr};
  std::vector<string::String> paths{};
};

inline UP<App> g_app = std::make_unique<App>();
