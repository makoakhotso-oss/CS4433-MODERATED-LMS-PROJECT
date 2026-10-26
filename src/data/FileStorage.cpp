#include "lms/data/FileStorage.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "lms/common/Errors.hpp"

namespace lms {

namespace fs = std::filesystem;

namespace {

std::string sanitiseName(const std::string& name) {
  std::string out;
  for (char c : name) {
    const unsigned char uc = static_cast<unsigned char>(c);
    out.push_back((std::isalnum(uc) && uc < 128) || c == '.' || c == '-' || c == '_' ? c : '_');
  }
  while (!out.empty() && out.front() == '.') out.erase(out.begin());
  if (out.size() > 100) out = out.substr(out.size() - 100);
  return out.empty() ? "file" : out;
}

}  // namespace

FileStorage::FileStorage(fs::path root, std::uintmax_t maxBytes)
    : root_(std::move(root)),
      maxBytes_(maxBytes),
      allowedExtensions_{".pdf", ".doc", ".docx", ".odt", ".txt", ".md",  ".rtf", ".zip",
                         ".csv", ".xls", ".xlsx", ".ppt", ".pptx", ".png", ".jpg", ".jpeg",
                         ".cpp", ".hpp", ".h",   ".c",   ".py",  ".java"} {}

std::string FileStorage::store(Id assignmentId, Id studentId, int attemptNo,
                               const fs::path& source) const {
  std::error_code ec;
  if (!fs::is_regular_file(source, ec)) {
    throw StorageError("The file '" + source.string() + "' was not found.");
  }
  const std::uintmax_t size = fs::file_size(source, ec);
  if (ec || size == 0) throw StorageError("The file is empty or cannot be read.");
  if (size > maxBytes_) {
    throw StorageError("The file is too large (limit " + std::to_string(maxBytes_ / (1024 * 1024)) +
                       " MB).");
  }
  std::string ext = source.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (allowedExtensions_.count(ext) == 0) {
    throw StorageError("Files of type '" + (ext.empty() ? std::string("(none)") : ext) +
                       "' cannot be submitted.");
  }

  const fs::path relativeDir = fs::path("assignments") / std::to_string(assignmentId) /
                               std::to_string(studentId);
  const std::string fileName = "attempt" + std::to_string(attemptNo) + "_" +
                               sanitiseName(source.filename().string());
  const fs::path relative = relativeDir / fileName;
  fs::create_directories(root_ / relativeDir, ec);
  if (ec) throw StorageError("The storage folder could not be created: " + ec.message());
  fs::copy_file(source, root_ / relative, fs::copy_options::none, ec);
  if (ec) throw StorageError("The file could not be saved: " + ec.message());
  return relative.generic_string();
}

void FileStorage::remove(const std::string& relativePath) const noexcept {
  std::error_code ec;
  fs::remove(root_ / relativePath, ec);
}

}  // namespace lms
