#include "lms/services/AuthService.hpp"

#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"
#include "lms/common/Validation.hpp"
#include "lms/services/AccessControl.hpp"

namespace lms {

namespace {
constexpr const char* kGenericFailure = "Invalid username or password.";
}

AuthService::AuthService(IUserRepository& users, const PasswordHasher& hasher, const IClock& clock,
                         int maxFailedAttempts, int lockSeconds)
    : users_(users),
      hasher_(hasher),
      clock_(clock),
      maxFailures_(maxFailedAttempts),
      lockSeconds_(lockSeconds),
      dummyHash_(hasher.hash("not-a-real-password-1")) {}

void AuthService::fail(const std::string& name, const std::string& reason) {
  Attempts& a = attempts_[name];
  ++a.failures;
  if (a.failures >= maxFailures_) {
    a.lockedUntil = clock_.now() + lockSeconds_;
    a.failures = 0;
    Logger::instance().warn("Account '" + name + "' temporarily locked after repeated failures");
  }
  Logger::instance().warn("Login failed for '" + name + "': " + reason);
  throw AuthenticationError(kGenericFailure);
}

std::unique_ptr<User> AuthService::login(const std::string& username, const std::string& password) {
  std::string name;
  try {
    name = validation::normaliseUsername(username);
  } catch (const ValidationError&) {
    throw AuthenticationError(kGenericFailure);  // malformed names can never match an account
  }

  const auto it = attempts_.find(name);
  if (it != attempts_.end() && it->second.lockedUntil > clock_.now()) {
    const Timestamp minutes = (it->second.lockedUntil - clock_.now() + 59) / 60;
    throw AuthenticationError("Too many failed attempts. Try again in " + std::to_string(minutes) +
                              " minute(s).");
  }

  std::unique_ptr<User> user = users_.findByUsername(name);
  if (!user) {
    PasswordHasher::verify(dummyHash_, password);  // same work as a real check
    fail(name, "unknown user");
  }
  if (!user->login(name, password)) fail(name, user->isActive() ? "wrong password" : "account deactivated");

  attempts_.erase(name);
  if (hasher_.needsRehash(user->passwordHash())) {
    try {
      users_.updatePasswordHash(user->userId(), hasher_.hash(password));
    } catch (const LmsError&) {
      // Upgrading the hash is best-effort; the login itself succeeded.
    }
  }
  Logger::instance().info("Login: '" + name + "' (" + user->getRole() + ")");
  return user;
}

void AuthService::changePassword(const User& user, const std::string& oldPassword,
                                 const std::string& newPassword) {
  AccessControl::require(user, Permission::ChangeOwnPassword);
  std::unique_ptr<User> fresh = users_.findById(user.userId());
  if (!fresh || !PasswordHasher::verify(fresh->passwordHash(), oldPassword)) {
    throw AuthenticationError("The current password is incorrect.");
  }
  validation::requirePasswordStrength(newPassword);
  users_.updatePasswordHash(user.userId(), hasher_.hash(newPassword));
  Logger::instance().info("Password changed for '" + user.username() + "'");
}

}  // namespace lms
