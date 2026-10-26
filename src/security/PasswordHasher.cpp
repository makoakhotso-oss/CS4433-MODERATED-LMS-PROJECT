#include "lms/security/PasswordHasher.hpp"

#include <sodium.h>

#include "lms/common/Errors.hpp"

namespace lms {

namespace {
void ensureSodiumInitialised() {
  static const int rc = sodium_init();  // thread-safe, runs once; returns -1 on failure
  if (rc < 0) throw LmsError("The cryptographic library could not be initialised.");
}
}  // namespace

PasswordHasher::PasswordHasher(Cost cost) {
  ensureSodiumInitialised();
  if (cost == Cost::Interactive) {
    opsLimit_ = crypto_pwhash_OPSLIMIT_INTERACTIVE;
    memLimit_ = crypto_pwhash_MEMLIMIT_INTERACTIVE;
  } else {
    opsLimit_ = crypto_pwhash_OPSLIMIT_MIN;
    memLimit_ = crypto_pwhash_MEMLIMIT_MIN;
  }
}

std::string PasswordHasher::hash(const std::string& password) const {
  char out[crypto_pwhash_STRBYTES];
  if (crypto_pwhash_str_alg(out, password.c_str(), password.size(), opsLimit_, memLimit_,
                            crypto_pwhash_ALG_ARGON2ID13) != 0) {
    throw LmsError("Password hashing failed (out of memory).");
  }
  return std::string(out);
}

bool PasswordHasher::verify(const std::string& storedHash, const std::string& password) noexcept {
  try {
    ensureSodiumInitialised();
  } catch (...) {
    return false;
  }
  return crypto_pwhash_str_verify(storedHash.c_str(), password.c_str(), password.size()) == 0;
}

bool PasswordHasher::needsRehash(const std::string& storedHash) const noexcept {
  return crypto_pwhash_str_needs_rehash(storedHash.c_str(), opsLimit_, memLimit_) != 0;
}

}  // namespace lms
