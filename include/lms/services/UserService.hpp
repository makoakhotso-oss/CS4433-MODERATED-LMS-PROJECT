#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lms/data/IRepositories.hpp"
#include "lms/security/PasswordHasher.hpp"

namespace lms {

struct NewUserRequest {
  std::string fullName;
  std::string username;
  std::string password;
  Role role{Role::Student};
  std::optional<Id> departmentId;
};

/// Administrator functions on accounts and departments (FR-01).
class UserService {
 public:
  UserService(IUserRepository& users, IDepartmentRepository& departments,
              const PasswordHasher& hasher);

  Id createUser(const User& actor, const NewUserRequest& request);
  void updateUser(const User& actor, Id userId, const std::string& fullName, Role role,
                  std::optional<Id> departmentId);
  void setActive(const User& actor, Id userId, bool active);  // activate / deactivate
  void resetPassword(const User& actor, Id userId, const std::string& newPassword);
  std::vector<std::unique_ptr<User>> listUsers(const User& actor) const;

  Id createDepartment(const User& actor, const std::string& name);
  std::vector<Department> listDepartments(const User& actor) const;

  /// First-run bootstrap: creates the initial administrator when the database has no users.
  /// Not available once any user exists.
  Id createInitialAdministrator(const std::string& fullName, const std::string& username,
                                const std::string& password);

 private:
  std::optional<Id> checkedDepartment(Role role, std::optional<Id> departmentId) const;

  IUserRepository& users_;
  IDepartmentRepository& departments_;
  const PasswordHasher& hasher_;
};

}  // namespace lms
