#pragma once

#include "memory/types.h"
#include "string/string.h"

struct App {
  u32            maxDepth{0};
  bool           pipe{false};
  bool           hidden{false};
  string::String pattern{};
  string::String path{};
};

inline UP<App> g_app = std::make_unique<App>();
