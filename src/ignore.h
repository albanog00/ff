#pragma once

#include "memory/arc.h"
#include "memory/types.h"
#include "string/string.h"

#include <fcntl.h>
#include <vector>

enum class IgnoreRuleFlags : u8 {
  None          = 0,
  Negated       = 1 << 0,
  DirectoryOnly = 1 << 1,
  Anchored      = 1 << 2,
  BasenameOnly  = 1 << 3
};

inline bool operator&(IgnoreRuleFlags a, IgnoreRuleFlags b) {
  return ((static_cast<i32>(a) & static_cast<i32>(b)) > 0);
};

inline IgnoreRuleFlags operator|(IgnoreRuleFlags a, IgnoreRuleFlags b) {
  return static_cast<IgnoreRuleFlags>(static_cast<i32>(a) | static_cast<i32>(b));
}

struct IgnoreRule {
  string::String  pattern;
  string::String  basePath;
  IgnoreRuleFlags flags;
};

struct IgnoreContext {
  memory::Arc<IgnoreContext> parent;
  std::vector<IgnoreRule>    rules;

  IgnoreContext(const memory::Arc<IgnoreContext>& parent) : parent(parent) {}
};

bool isIgnored(const memory::Arc<IgnoreContext>& ctx, const string::String& fullPath, bool isDir);

bool isIgnored(IgnoreContext* ctx, const string::String& fullPath, bool isDir);

bool ruleMatches(const IgnoreRule& rule, const string::String& fullPath, bool isDir);

bool matchGlob(const string::String& pattern, const string::String& str);

memory::Arc<IgnoreContext> readGitignore([[maybe_unused]] string::String& dirPath, i32 dirFd,
    const memory::Arc<IgnoreContext>& parentCtx);
