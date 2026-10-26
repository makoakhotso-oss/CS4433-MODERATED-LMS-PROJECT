#pragma once
// Abstract User and its four role classes (Class Diagram section 3.2.2).
// The role classes differ in the permissions they carry; polymorphism lets all the
// services work with a `const User&` without caring about the concrete role (LSP).
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "lms/common/Types.hpp"
#include "lms/domain/Enums.hpp"

namespace lms {

class User {
 public:
  User(Id id, std::string fullName, std::string username, std::string passwordHash, bool active,
       std::optional<Id> departmentId);
  virtual ~User() = default;
  User(const User&) = delete;
  User& operator=(const User&) = delete;

  /// Checks the credentials against the stored salted hash. Returns false for a wrong
  /// username/password or a deactivated account; on success the user is marked logged in.
  bool login(const std::string& username, const std::string& password);
  void logout() noexcept { loggedIn_ = false; }
  bool isLoggedIn() const noexcept { return loggedIn_; }

  std::string getRole() const { return toString(role()); }
  virtual Role role() const noexcept = 0;
  virtual const std::set<Permission>& permissions() const = 0;
  bool hasPermission(Permission p) const { return permissions().count(p) != 0; }

  Id userId() const noexcept { return userId_; }
  const std::string& fullName() const noexcept { return fullName_; }
  const std::string& username() const noexcept { return username_; }
  const std::string& passwordHash() const noexcept { return passwordHash_; }
  bool isActive() const noexcept { return active_; }
  std::optional<Id> departmentId() const noexcept { return departmentId_; }

 protected:
  Id userId_;
  std::string fullName_;
  std::string username_;
  std::string passwordHash_;
  bool active_;
  std::optional<Id> departmentId_;
  bool loggedIn_{false};
};

class Administrator final : public User {
 public:
  using User::User;
  Role role() const noexcept override { return Role::Administrator; }
  const std::set<Permission>& permissions() const override;
};

class Lecturer final : public User {
 public:
  using User::User;
  Role role() const noexcept override { return Role::Lecturer; }
  const std::set<Permission>& permissions() const override;
};

class HeadOfDepartment final : public User {
 public:
  using User::User;
  Role role() const noexcept override { return Role::HeadOfDepartment; }
  const std::set<Permission>& permissions() const override;
};

class Student final : public User {
 public:
  using User::User;
  Role role() const noexcept override { return Role::Student; }
  const std::set<Permission>& permissions() const override;
};

/// Creates the right concrete User for a role (used when rows are read from the database).
class UserFactory {
 public:
  static std::unique_ptr<User> create(Role role, Id id, std::string fullName, std::string username,
                                      std::string passwordHash, bool active,
                                      std::optional<Id> departmentId);
};

}  // namespace lms
