#pragma once
// Exception hierarchy. Every error the user can cause or recover from derives
// from LmsError, so the user interface can catch one type, show the message and
// carry on instead of crashing.
#include <stdexcept>
#include <string>

namespace lms {

class LmsError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// Input is malformed or out of range (bad mark, empty title, weak password...).
class ValidationError : public LmsError {
 public:
  using LmsError::LmsError;
};

/// Login failed or the account is locked.
class AuthenticationError : public LmsError {
 public:
  using LmsError::LmsError;
};

/// The logged-in user's role (or ownership of the record) does not allow the action.
class PermissionDeniedError : public LmsError {
 public:
  using LmsError::LmsError;
};

/// A referenced record does not exist.
class NotFoundError : public LmsError {
 public:
  using LmsError::LmsError;
};

/// A business rule or workflow rule forbids the action (e.g. resubmitting after the deadline).
class BusinessRuleError : public LmsError {
 public:
  using LmsError::LmsError;
};

/// SQLite reported a failure.
class DatabaseError : public LmsError {
 public:
  explicit DatabaseError(const std::string& message, bool constraintViolation = false)
      : LmsError(message), constraint_(constraintViolation) {}
  bool isConstraintViolation() const noexcept { return constraint_; }

 private:
  bool constraint_;
};

/// File storage for submissions failed.
class StorageError : public LmsError {
 public:
  using LmsError::LmsError;
};

}  // namespace lms
