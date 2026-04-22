#pragma once

#include "memory/types.h"
#include "string/string.h"

struct App {
  u32                         maxDepth{0};
  bool                        pipe{false};
  bool                        hidden{false};
  string::String              pattern{};
  std::vector<string::String> paths{};
};

inline UP<App> g_app = std::make_unique<App>();
