#pragma once

#include "memory/types.h"

struct App {
  u32         maxDepth{0};
  bool        pipe{false};
  bool        hidden{false};
  std::string pattern;
  std::string path;
};

inline UP<App> g_app = std::make_unique<App>();
