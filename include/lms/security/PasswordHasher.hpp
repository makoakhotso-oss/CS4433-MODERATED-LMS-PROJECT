#pragma once
// Salted password hashing with libsodium (Argon2id). See docs/SECURITY.md for the
// justification of this choice.
#include <cstddef>
#include <string>

namespace lms {

class PasswordHasher {
 public:
  enum class Cost {
    Interactive,  // production setting (libsodium "interactive": 2 passes, 64 MiB)
    Minimal       // cheapest setting, ONLY for automated tests that create hundreds of users
  };

  explicit PasswordHasher(Cost cost = Cost::Interactive);

  /// Returns a self-describing string ("$argon2id$v=19$m=...,t=...,p=...$<salt>$<hash>");
  /// a fresh random 16-byte salt is generated on every call.
  std::string hash(const std::string& password) const;

  /// Constant-time verification. The cost parameters are read from the stored string,
  /// so hashes created with older settings still verify.
  static bool verify(const std::string& storedHash, const std::string& password) noexcept;

  /// True if the stored hash was made with weaker parameters than this hasher uses.
  bool needsRehash(const std::string& storedHash) const noexcept;

 private:
  unsigned long long opsLimit_;
  std::size_t memLimit_;
};

}  // namespace lms
