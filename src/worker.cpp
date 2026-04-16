#include "app.h"
#include "worker.h"

#include <spdlog/spdlog.h>

WorkerContext::WorkerContext() {
  threads.reserve(nWorkers);
  for (u32 i = 0; i < nWorkers; ++i) {
    threads.emplace_back(std::thread([this] { work(); }));
  }
}

void WorkerContext::work() {
  const std::string& pattern = g_app->pattern;
  const u64          size    = pattern.size();
  const bool         is_pipe = g_app->pipe;

  std::string        buffer;
  buffer.reserve(KiB(32));

  auto flushBuffer = [&] {
    u64 written = 0;
    while (written < buffer.size()) {
      ssize_t n = write(STDOUT_FILENO, buffer.c_str(), buffer.size());
      if (n <= 0) { break; }
      written += static_cast<u64>(n);
    }
    buffer.clear();
  };

  // view will most likely move forward
  auto highlightPattern = [&](u64 it, std::string_view& view) {
    while ((it = view.find(pattern)) != std::string::npos) {
      buffer.append(view.substr(0, it));    // append till start of pattern
      buffer += "\033[31m";                 // color
      buffer.append(view.substr(it, size)); // append pattern
      buffer += "\033[0m";                  // reset color
      view.remove_prefix(it + size);        // increment post pattern
    }
  };

  auto formatPath = [&](u64 pos, std::string_view view) {
    if (!is_pipe) { highlightPattern(pos, view); }
    buffer.append(view); // append rest of string
    buffer += "\n";
  };

  Task filePathRef;

  while (true) {
    dequeue(filePathRef);
    [[unlikely]] if (filePathRef == POISON) { break; }

    u64              pos = 0;
    std::string_view view{*filePathRef};

    if ((pos = view.find(pattern)) != std::string::npos) {
      formatPath(pos, view);
      [[unlikely]] if (buffer.size() >= KiB(30)) { flushBuffer(); }
    }

    if (pendingWork.fetch_sub(1, std::memory_order_acq_rel) == 1 && stopping) { poison(); }
  }

  flushBuffer();
}
