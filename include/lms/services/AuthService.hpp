#pragma once
#include <map>
#include <memory>
#include <string>

#include "lms/common/Time.hpp"
#include "lms/data/IRepositories.hpp"
#include "lms/security/PasswordHasher.hpp"

namespace lms {

class AuthService {
 public:
  AuthService(IUserRepository& users, const PasswordHasher& hasher, const IClock& clock,
              int maxFailedAttempts = 5, int lockSeconds = 300);

  /// Returns the logged-in user. Throws AuthenticationError with a deliberately generic
  /// message for an unknown user, wrong password or deactivated account (FR-02).
  std::unique_ptr<User> login(const std::string& username, const std::string& password);

  /// Changes the logged-in user's own password (the old password must be correct).
  void changePassword(const User& user, const std::string& oldPassword,
                      const std::string& newPassword);

 private:
  struct Attempts {
    int failures{0};
    Timestamp lockedUntil{0};
  };
  [[noreturn]] void fail(const std::string& normalisedUsername, const std::string& reason);

  IUserRepository& users_;
  const PasswordHasher& hasher_;
  const IClock& clock_;
  int maxFailures_;
  int lockSeconds_;
  std::map<std::string, Attempts> attempts_;
  std::string dummyHash_;  // verified when the username is unknown, to equalise timing
};

}  // namespace lms
