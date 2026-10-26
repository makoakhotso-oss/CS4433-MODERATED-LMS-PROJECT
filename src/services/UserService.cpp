#include "lms/services/UserService.hpp"

#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"
#include "lms/common/Validation.hpp"
#include "lms/services/AccessControl.hpp"

namespace lms {

UserService::UserService(IUserRepository& users, IDepartmentRepository& departments,
                         const PasswordHasher& hasher)
    : users_(users), departments_(departments), hasher_(hasher) {}

std::optional<Id> UserService::checkedDepartment(Role role, std::optional<Id> departmentId) const {
  const bool needsDepartment = role == Role::Lecturer || role == Role::HeadOfDepartment;
  if (!needsDepartment) return std::nullopt;  // administrators and students have no department
  if (!departmentId) throw ValidationError("A department is required for this role.");
  if (!departments_.findById(*departmentId)) throw NotFoundError("Department not found.");
  return departmentId;
}

Id UserService::createUser(const User& actor, const NewUserRequest& req) {
  AccessControl::require(actor, Permission::ManageUsers);
  const std::string fullName = validation::requireText(req.fullName, "Full name", 100);
  const std::string username = validation::normaliseUsername(req.username);
  validation::requirePasswordStrength(req.password);
  const std::optional<Id> dept = checkedDepartment(req.role, req.departmentId);
  if (users_.findByUsername(username)) throw BusinessRuleError("That username is already in use.");

  NewUser u;
  u.fullName = fullName;
  u.username = username;
  u.passwordHash = hasher_.hash(req.password);
  u.role = req.role;
  u.departmentId = dept;
  const Id id = users_.insert(u);
  Logger::instance().info("User created: '" + username + "' (" + toString(req.role) + ") by '" +
                          actor.username() + "'");
  return id;
}

Id UserService::createInitialAdministrator(const std::string& fullName, const std::string& username,
                                           const std::string& password) {
  if (users_.countAll() != 0) {
    throw BusinessRuleError("The initial administrator can only be created in an empty system.");
  }
  NewUser u;
  u.fullName = validation::requireText(fullName, "Full name", 100);
  u.username = validation::normaliseUsername(username);
  validation::requirePasswordStrength(password);
  u.passwordHash = hasher_.hash(password);
  u.role = Role::Administrator;
  const Id id = users_.insert(u);
  Logger::instance().info("Initial administrator created: '" + u.username + "'");
  return id;
}

void UserService::updateUser(const User& actor, Id userId, const std::string& fullName, Role role,
                             std::optional<Id> departmentId) {
  AccessControl::require(actor, Permission::ManageUsers);
  const std::unique_ptr<User> target = users_.findById(userId);
  if (!target) throw NotFoundError("User not found.");
  const std::string name = validation::requireText(fullName, "Full name", 100);
  const std::optional<Id> dept = checkedDepartment(role, departmentId);

  if (role != target->role()) {
    if (target->userId() == actor.userId()) {
      throw BusinessRuleError("You cannot change your own role.");
    }
    if (target->role() == Role::Administrator && target->isActive() &&
        users_.countActiveAdministrators() <= 1) {
      throw BusinessRuleError("The last active administrator cannot be changed to another role.");
    }
    if (users_.hasAcademicRecords(userId)) {
      throw BusinessRuleError(
          "The role cannot be changed because the user already has courses, enrolments or "
          "submissions.");
    }
  }
  users_.updateProfile(userId, name, role, dept);
  Logger::instance().info("User updated: '" + target->username() + "' by '" + actor.username() + "'");
}

void UserService::setActive(const User& actor, Id userId, bool active) {
  AccessControl::require(actor, Permission::ManageUsers);
  const std::unique_ptr<User> target = users_.findById(userId);
  if (!target) throw NotFoundError("User not found.");
  if (!active) {
    if (target->userId() == actor.userId()) {
      throw BusinessRuleError("You cannot deactivate your own account.");
    }
    if (target->role() == Role::Administrator && target->isActive() &&
        users_.countActiveAdministrators() <= 1) {
      throw BusinessRuleError("The last active administrator cannot be deactivated.");
    }
  }
  users_.setActive(userId, active);
  Logger::instance().info(std::string(active ? "User activated: '" : "User deactivated: '") +
                          target->username() + "' by '" + actor.username() + "'");
}

void UserService::resetPassword(const User& actor, Id userId, const std::string& newPassword) {
  AccessControl::require(actor, Permission::ManageUsers);
  const std::unique_ptr<User> target = users_.findById(userId);
  if (!target) throw NotFoundError("User not found.");
  validation::requirePasswordStrength(newPassword);
  users_.updatePasswordHash(userId, hasher_.hash(newPassword));
  Logger::instance().info("Password reset for '" + target->username() + "' by '" +
                          actor.username() + "'");
}

std::vector<std::unique_ptr<User>> UserService::listUsers(const User& actor) const {
  AccessControl::require(actor, Permission::ManageUsers);
  return users_.findAll();
}

Id UserService::createDepartment(const User& actor, const std::string& name) {
  AccessControl::require(actor, Permission::ManageDepartments);
  const std::string n = validation::requireText(name, "Department name", 100);
  if (departments_.findByName(n)) throw BusinessRuleError("That department already exists.");
  return departments_.insert(n);
}

std::vector<Department> UserService::listDepartments(const User& actor) const {
  AccessControl::require(actor, Permission::ManageDepartments);
  return departments_.findAll();
}

}  // namespace lms
