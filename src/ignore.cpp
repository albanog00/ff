#include "ignore.h"
#include "utils/defer.h"

#include <cstring>
#include <spdlog/spdlog.h>
#include <unistd.h>

struct IgnoreMatch {
  bool              ignored{false};
  const IgnoreRule* rule{nullptr};
};

struct Line {
  u8* data{nullptr};
  u32 size{0};
};

static bool hasSlash(const string::String& str) {
  return memchr(str.data(), '/', str.size()) != nullptr;
}

static Line trimLine(u8* start, const u8* end) {
  u32 size = end - start;
  if (size > 0 && start[size - 1] == '\r')
    size -= 1;

  while (size > 0 && (start[size - 1] == ' ' || start[size - 1] == '\t'))
    size -= 1;

  return Line{.data = start, .size = size};
}

static bool parseRule(Line line, const string::String& basePath, IgnoreRule& out) {
  if (line.size == 0 || line.data[0] == '#')
    return false;

  IgnoreRuleFlags flags        = IgnoreRuleFlags::None;
  u32             patternStart = 0;
  u32             patternSize  = line.size;

  if (line.data[0] == '!') {
    flags        = flags | IgnoreRuleFlags::Negated;
    patternStart = 1;
    patternSize -= 1;
  }

  if (patternSize == 0)
    return false;

  if (line.data[patternStart] == '/') {
    flags = flags | IgnoreRuleFlags::Anchored;
    patternStart += 1;
    patternSize -= 1;
  }

  while (patternSize > 0 && line.data[patternStart + patternSize - 1] == '/') {
    flags = flags | IgnoreRuleFlags::DirectoryOnly;
    patternSize -= 1;
  }

  if (patternSize == 0)
    return false;

  out.pattern  = string::String::from(line.data + patternStart, patternSize);
  out.basePath = string::String::from(basePath);
  out.flags    = hasSlash(out.pattern) ? flags : (flags | IgnoreRuleFlags::BasenameOnly);
  return true;
}

static string::String relativePathFrom(const string::String& fullPath, const string::String& basePath) {
  if (basePath.empty())
    return string::String::from(fullPath);

  if (fullPath.size() < basePath.size())
    return {};

  if (memcmp(fullPath.data(), basePath.data(), basePath.size()) != 0)
    return {};

  u32 start = basePath.size();
  if (start < fullPath.size() && fullPath[start] == '/')
    start += 1;

  if (start >= fullPath.size())
    return {};

  return string::String::from(fullPath.data() + start, fullPath.size() - start);
}

static bool basenameMatches(const string::String& pattern, const string::String& relativePath) {
  u32 componentStart = 0;

  for (u32 i = 0; i <= relativePath.size(); ++i) {
    if (i < relativePath.size() && relativePath[i] != '/')
      continue;

    if (i > componentStart) {
      string::String component = string::String::from(relativePath.data() + componentStart,
          i - componentStart);
      if (matchGlob(pattern, component))
        return true;
    }

    componentStart = i + 1;
  }

  return false;
}

static IgnoreMatch evaluateIgnore(IgnoreContext* ctx, const string::String& fullPath, bool isDir) {
  if (!ctx)
    return {};

  IgnoreMatch match = evaluateIgnore(ctx->parent.get(), fullPath, isDir);

  for (usize i = 0; i < ctx->rules.size(); ++i) {
    const IgnoreRule& rule = ctx->rules[i];
    if (!ruleMatches(rule, fullPath, isDir))
      continue;

    match.ignored = !(rule.flags & IgnoreRuleFlags::Negated);
    match.rule    = &rule;
  }

  return match;
}

static void logOpenedGitignore([[maybe_unused]] const string::String& dirPath,
    [[maybe_unused]] i32 gitignoreFd) {
#if DEBUG
  string::String gitignoreFullPath = dirPath;
  if (gitignoreFullPath.back() != '/')
    gitignoreFullPath += "/";
  gitignoreFullPath += ".gitignore";
  spdlog::debug("opened {} at fd {}", gitignoreFullPath.c_str(), gitignoreFd);
#endif
}

static void logIgnoredMatch([[maybe_unused]] const string::String& fullPath,
    [[maybe_unused]] const IgnoreRule* rule) {
#if DEBUG
  if (!rule)
    return;

  spdlog::debug("ignored {} by pattern `{}` from `{}`", fullPath.c_str(), rule->pattern.c_str(),
      rule->basePath.c_str());
#endif
}

bool isIgnored(const memory::Arc<IgnoreContext>& ctx, const string::String& fullPath, bool isDir) {
  return isIgnored(ctx.get(), fullPath, isDir);
}

bool isIgnored(IgnoreContext* ctx, const string::String& fullPath, bool isDir) {
  IgnoreMatch match = evaluateIgnore(ctx, fullPath, isDir);
  if (match.ignored)
    logIgnoredMatch(fullPath, match.rule);
  return match.ignored;
}

bool ruleMatches(const IgnoreRule& rule, const string::String& fullPath, bool isDir) {
  if ((rule.flags & IgnoreRuleFlags::DirectoryOnly) && !isDir)
    return false;

  string::String relativePath = relativePathFrom(fullPath, rule.basePath);
  if (relativePath.empty())
    return false;

  if ((rule.flags & IgnoreRuleFlags::BasenameOnly))
    return basenameMatches(rule.pattern, relativePath);

  return matchGlob(rule.pattern, relativePath);
}

bool matchGlob(const string::String& pattern, const string::String& str) {
  usize p     = 0;
  usize s     = 0;
  usize star  = std::string::npos;
  usize match = 0;

  while (s < str.size()) {
    if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == str[s])) {
      if (str[s] == '/' && pattern[p] == '?')
        return false;
      ++p, ++s;
    } else if (p < pattern.size() && pattern[p] == '*') {
      star  = p++;
      match = s;
    } else if (star != std::string::npos) {
      p = star + 1;
      s = ++match;
      if (str[s - 1] == '/')
        return false;
    } else {
      return false;
    }
  }

  while (p < pattern.size() && pattern[p] == '*') ++p;

  return p == pattern.size();
}

memory::Arc<IgnoreContext> readGitignore([[maybe_unused]] string::String& dirPath, i32 dirFd,
    const memory::Arc<IgnoreContext>& parentCtx) {
  i32 gitignoreFd = openat(dirFd, ".gitignore", O_RDONLY);
  if (gitignoreFd == -1)
    return parentCtx;

  defer(close(gitignoreFd));
  logOpenedGitignore(dirPath, gitignoreFd);

  i64 size = lseek(gitignoreFd, 0, SEEK_END);
  if (size == 0)
    return parentCtx;

  lseek(gitignoreFd, 0, SEEK_SET);

  memory::TempArena scratch = memory::getLocalScratchArena();
  u8*               fileBuffer = static_cast<u8*>(scratch.arena.alloc(size + 1, 64));
  fileBuffer[size]             = 0;

  i64 bufOffset = 0;
  while (bufOffset < size) {
    i64 got = read(gitignoreFd, fileBuffer + bufOffset, size - bufOffset);
    if (got <= 0)
      break;
    bufOffset += got;
  }

  const u8*                  newLine     = nullptr;
  u64                        startOffset = 0;
  bool                       scanning    = true;
  memory::Arc<IgnoreContext> ignoreCtx{IgnoreContext{parentCtx}};

  while (scanning) {
    u8* start = fileBuffer + startOffset;
    newLine   = static_cast<u8*>(memchr(start, '\n', size - startOffset));

    if (newLine == nullptr) {
      newLine  = fileBuffer + size;
      scanning = false;
    }

    startOffset = newLine - fileBuffer + 1;

    IgnoreRule rule{};
    if (parseRule(trimLine(start, newLine), dirPath, rule))
      ignoreCtx->rules.emplace_back(std::move(rule));
  }

  return ignoreCtx;
}
