#include "app.h"
#include "explorer.h"

#include <cstring>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <span>
#include <print>
#include <stdexcept>

struct Error {
  std::string message;
};

struct Result {
  bool  success;
  Error error;
};

constexpr void printUsage(char* cmd) {
  std::println("usage: {} <pattern> <filepath> [--option=<value>|-o=<value>]", cmd);
  std::println("\nOptions:");
  std::println("--help,      -h   Show help.");
  std::println("--max-depth, -d   Set the maximum depth for directory traversal.");
  std::println("--hidden,    -H   Include hidden files in search.");
  exit(1);
}

enum class NumberSign : i32 {
  Positive = 1 << 0,
  Negative = 1 << 1
};

bool operator&(NumberSign a, NumberSign b) {
  return ((static_cast<i32>(a) & static_cast<i32>(b)) > 0);
};

NumberSign operator|(NumberSign a, NumberSign b) {
  return static_cast<NumberSign>(static_cast<i32>(a) | static_cast<i32>(b));
}

template <typename T>
Result parseNumber(std::string_view str, T& out, NumberSign sign) {
  auto res = std::from_chars(str.begin(), str.end(), out, 10);
  if (res.ec != std::errc{}) {
    return Result{.success = false,
        .error             = Error{
                        .message = std::format("invalid value for conversion to number, got value `{}`", str)}};
  }

  if (out < 0 && !(sign & NumberSign::Negative)) {
    return Result{.success = false,
        .error =
            Error{.message = std::format("value must be a positive number, got value `{}`", out)}};
  }
  if (out > 0 && !(sign & NumberSign::Positive)) {
    return Result{.success = false,
        .error =
            Error{.message = std::format("value must be a negative number, got value `{}`", out)}};
  }

  return Result{.success = true};
}

void parseOption(std::string_view opt) {
  if (opt.starts_with("--max-depth=") || opt.starts_with("-d=")) {
    i32    pos = opt.find_first_of('=');

    Result res = parseNumber(opt.begin() + pos + 1, g_app->maxDepth, NumberSign::Positive);
    if (!res.success) {
      spdlog::error(res.error.message);
      throw std::logic_error("invalid value for `max-depth` option");
    }

    spdlog::debug("parsed `maxDepth` with value: `{}`", g_app->maxDepth);
  } else if (opt.starts_with("--hidden") || opt.starts_with("-H")) {
    g_app->hidden = true;
  }
}

void parsePath(const char* p) {
  if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
    g_app->path = string::String::from(std::filesystem::canonical(p).native());
  } else {
    spdlog::warn("discarding invalid path `{}`", p);
  }
}

bool init(std::span<char*> args) {
  if (args.size() <= 1) { printUsage(args[0]); }

  // is output piped?
  g_app->pipe = !isatty(STDOUT_FILENO);

  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i][0] == '-') {
      if (strncmp(args[i], "--help", 6) == 0 || strncmp(args[i], "-h", 2) == 0) {
        printUsage(args[0]);
      }
      parseOption(args[i]);
    } else if (g_app->pattern.empty()) {
      g_app->pattern = string::String::from(args[i]);
    } else if (g_app->path.empty()) {
      parsePath(args[i]);
    }
  }

  if (g_app->pattern.empty()) {
    spdlog::error("no pattern provided");
    return false;
  }

  if (g_app->path.empty()) {
    spdlog::error("no valid paths provided");
    return false;
  }

  return true;
}

void run() {
  ExplorerContext explorerCtx{g_app->path};
  explorerCtx.join();
}

i32 main(i32 argc, char** argv) {
  std::span<char*> args{argv, static_cast<size_t>(argc)};
  if (!init(args)) { return 1; }
  run();
  return 0;
}
