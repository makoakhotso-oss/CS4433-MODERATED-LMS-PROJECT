#pragma once
// Stores uploaded submission files under a data directory (copied, never moved).
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>

#include "lms/common/Types.hpp"

namespace lms {

class FileStorage {
 public:
  explicit FileStorage(std::filesystem::path root, std::uintmax_t maxBytes = 10u * 1024u * 1024u);

  /// Validates and copies `source` to <root>/assignments/<a>/<s>/attempt<N>_<safe name>.
  /// Returns the path relative to the root (this is what the database stores).
  /// Throws StorageError for a missing/empty/oversized file or a disallowed type.
  std::string store(Id assignmentId, Id studentId, int attemptNo,
                    const std::filesystem::path& source) const;

  /// Deletes a stored file (used to undo a copy when the database write fails).
  void remove(const std::string& relativePath) const noexcept;

  std::filesystem::path resolve(const std::string& relativePath) const {
    return root_ / relativePath;
  }

 private:
  std::filesystem::path root_;
  std::uintmax_t maxBytes_;
  std::set<std::string> allowedExtensions_;
};

}  // namespace lms
