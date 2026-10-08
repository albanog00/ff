#include "ignore.h"

#include <cassert>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>

static IgnoreRule rule(const char* base, const char* pattern, IgnoreRuleFlags flags) {
  return IgnoreRule{
      .pattern  = string::String::from(pattern),
      .basePath = string::String::from(base),
      .flags    = flags,
  };
}

static void testGlobMatching() {
  assert(matchGlob(string::String::from("*.tmp"), string::String::from("drop.tmp")));
  assert(matchGlob(string::String::from("file?.cc"), string::String::from("file1.cc")));
  assert(!matchGlob(string::String::from("file?.cc"), string::String::from("file12.cc")));
  assert(!matchGlob(string::String::from("*.tmp"), string::String::from("src/drop.tmp")));
}

static void testRuleMatching() {
  IgnoreRule basename = rule("/repo", "*.tmp", IgnoreRuleFlags::BasenameOnly);
  assert(ruleMatches(basename, string::String::from("/repo/src/drop.tmp"), false));
  assert(!ruleMatches(basename, string::String::from("/other/drop.tmp"), false));

  IgnoreRule dirOnly =
      rule("/repo", "build", IgnoreRuleFlags::DirectoryOnly | IgnoreRuleFlags::BasenameOnly);
  assert(ruleMatches(dirOnly, string::String::from("/repo/build/"), true));
  assert(!ruleMatches(dirOnly, string::String::from("/repo/build"), false));
}

static void testContextPrecedence() {
  memory::Arc<IgnoreContext> parent{IgnoreContext{memory::Arc<IgnoreContext>{}}};
  parent->rules.emplace_back(rule("/repo", "*.tmp", IgnoreRuleFlags::BasenameOnly));

  memory::Arc<IgnoreContext> child{IgnoreContext{parent}};
  child->rules.emplace_back(
      rule("/repo", "important.tmp", IgnoreRuleFlags::Negated | IgnoreRuleFlags::BasenameOnly));

  assert(isIgnored(child, string::String::from("/repo/drop.tmp"), false));
  assert(!isIgnored(child, string::String::from("/repo/important.tmp"), false));
}

static void testReadGitignore() {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "ff-ignore-tests";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "build");
  std::filesystem::create_directories(root / "logs");

  std::ofstream gitignore(root / ".gitignore");
  gitignore << "# comment\n";
  gitignore << "build\n";
  gitignore << "*.tmp\n";
  gitignore << "!important.tmp\n";
  gitignore << "logs/\n";
  gitignore.close();

  i32 dirFd = open(root.c_str(), O_RDONLY | O_DIRECTORY);
  assert(dirFd != -1);

  string::String             dirPath = string::String::from(root.string());
  memory::Arc<IgnoreContext> ctx     = readGitignore(dirPath, dirFd, memory::Arc<IgnoreContext>{});
  close(dirFd);

  assert(ctx);
  assert(isIgnored(ctx, string::String::from((root / "build/").string()), true));
  assert(isIgnored(ctx, string::String::from((root / "drop.tmp").string()), false));
  assert(!isIgnored(ctx, string::String::from((root / "important.tmp").string()), false));
  assert(isIgnored(ctx, string::String::from((root / "logs/").string()), true));

  std::filesystem::remove_all(root);
}

int main() {
  testGlobMatching();
  testRuleMatching();
  testContextPrecedence();
  testReadGitignore();
  return 0;
}
