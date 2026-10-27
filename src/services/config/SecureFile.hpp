#pragma once

#include <string>

namespace clockin {

// Atomically replaces `path` with `contents` as a 0600 file: write to a
// temporary sibling, fsync, rename, then fsync the directory. On failure the
// previous file is untouched and `error` holds the system error text.
bool WriteSecureFile(const std::string& path,
                     const std::string& contents,
                     std::string& error);

}  // namespace clockin
