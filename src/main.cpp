#include "app.h"
#include "explorer.h"

#include <cstring>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <span>
#include <print>

struct Error {
  std::string message;
};

struct Result {
  bool  success;
  Error error;
};

constexpr void printUsage(char* cmd) {
  std::println(
      "usage: {} <pattern> [<filepath1> <filepath2> ...] [--option=<value>|-o=<value>]", cmd);
  std::println("\nOptions:");
  std::println("--help,      -h   Show help.");
  std::println("--max-depth, -d   Set the maximum depth for directory traversal.");
  std::println("--hidden,    -H   Include hidden files in search.");
  std::println("--type,      -t   Filter filetype: directory,dir,d,file,f\n"
               "                    comma separated: --type=dir,file");
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
    u64    pos = opt.find_first_of('=') + 1;

    Result res = parseNumber(opt.begin() + pos, g_app->maxDepth, NumberSign::Positive);
    if (!res.success) {
      spdlog::error(res.error.message);
      exit(1);
    }

    spdlog::debug("parsed `maxDepth` with value: `{}`", g_app->maxDepth);
  } else if (opt.starts_with("--hidden") || opt.starts_with("-H")) {
    g_app->hidden = true;
  } else if (opt.starts_with("--type=") || opt.starts_with("-t=")) {
    // types are splitted by comma `,`
    u64 pos  = opt.find_first_of("=") + 1;
    u64 last = pos;
    while (true) {
      if (pos = opt.find(',', pos); pos == std::string_view::npos) { break; }
      g_app->type |= FileType::parse(opt.substr(last, pos - last));
      pos += 1;
      last = pos;
    }
    g_app->type |= FileType::parse(opt.substr(last));
  } else {
    spdlog::error("unknown option `{}`", opt);
    exit(1);
  }
}

void parsePath(const char* p) {
  if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
    g_app->paths.push_back(string::String::from(std::filesystem::canonical(p).native()));
  } else {
    spdlog::warn("discarding invalid path `{}`", p);
  }
}

bool init(std::span<char*> args) {
  // set default logger on stderr
  spdlog::set_default_logger(spdlog::stderr_color_mt("stderr"));

#ifdef DEBUG
  spdlog::set_level(spdlog::level::debug);
#else
  spdlog::set_level(spdlog::level::info);
#endif

  if (args.size() <= 1) { printUsage(args[0]); }

  // is output piped?
  g_app->pipe = !isatty(STDOUT_FILENO);

  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i][0] == '-') {
      if (strncmp(args[i], "--help", 6) == 0 || strncmp(args[i], "-h", 2) == 0) {
        printUsage(args[0]);
      }
      parseOption(args[i]);
    } else if (g_app->pattern == nullptr) {
      i32    errorNumber;
      size_t errorOffset;

      g_app->pattern = pcre2_compile(reinterpret_cast<u8*>(args[i]), PCRE2_ZERO_TERMINATED, 0,
          &errorNumber, &errorOffset, NULL);

      if (g_app->pattern == nullptr) {
        u8 buffer[256];
        pcre2_get_error_message(errorNumber, buffer, sizeof(buffer));
        printf("PCRE2 compilation failed at offset %d: %s\n", (int)errorOffset, buffer);
        exit(1);
      }

    } else {
      parsePath(args[i]);
    }
  }

  if (g_app->pattern == nullptr) {
    spdlog::error("no pattern provided");
    return false;
  }

  if (g_app->paths.empty()) {
    spdlog::error("no valid paths provided");
    return false;
  }

  return true;
}

void run() {
  ExplorerContext explorerCtx{g_app->paths};
  explorerCtx.join();
#if DEBUG
  string::String::dumpPoolStats();
#endif
}

i32 main(i32 argc, char** argv) {
  std::span<char*> args{argv, static_cast<size_t>(argc)};
  if (!init(args)) { return 1; }
  run();
  return 0;
}
