#pragma once

#include "memory/types.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

inline bool isDirectory(struct dirent* entry, i32 dirFd) {
  if (entry->d_type == DT_DIR) {
    return true;
  } else if (entry->d_type == DT_UNKNOWN) {
    struct stat st;
    return (fstatat(dirFd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) && S_ISDIR(st.st_mode);
  }
  return false;
};
