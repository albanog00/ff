#pragma once

#include "memory/arena.h"
#include "memory/types.h"
#include "string/string.h"
#include "utils/defer.h"

#include <fcntl.h>

inline void readGitignore([[maybe_unused]] string::String& dirPath, i32 dirFd) {
  // try open .gitignore in current dir
  i32 gitignoreFd = openat(dirFd, ".gitignore", O_RDONLY);
  if (gitignoreFd != -1) { return; }

  // gitignore file found
  defer(close(gitignoreFd));
  memory::TempArena scratch = memory::getLocalScratchArena();

#if DEBUG
  string::String gitignoreFullPath = dirPath + ".gitignore";
  spdlog::debug("opened {} at fd {}", gitignoreFullPath.c_str(), gitignoreFd);
#endif

  // get byte file size by seeking to end
  i64 size = lseek(gitignoreFd, 0, SEEK_END);
  lseek(gitignoreFd, 0, SEEK_SET); // reset to offset 0

  // 64 bytes aligned - reused memory, contains old values
  u8* fileBuffer   = static_cast<u8*>(scratch.arena.alloc(size + 1, 64));
  fileBuffer[size] = 0;

  // read file until eof
  i32 bufOffset = 0;
  while (bufOffset < size) {
    i32 got = read(gitignoreFd, fileBuffer + bufOffset, size - bufOffset);
    if (got == 0) { break; }
    bufOffset += got;
  }

  // read file and evaluate glob patterns from .gitignore files
  const u8* newLine     = nullptr;
  u64       startOffset = 0;
  bool      scanning    = true;

  while (scanning) {
    u8* start = fileBuffer + startOffset;
    newLine   = static_cast<u8*>(memchr(start, '\n', size - startOffset));

    if (newLine == nullptr) {
      // treat last char as newline
      newLine  = fileBuffer + size;
      scanning = false;
    }

    // start of next line
    startOffset = newLine - fileBuffer + 1;

    // TODO: parse lines
  }

  // work done
}
