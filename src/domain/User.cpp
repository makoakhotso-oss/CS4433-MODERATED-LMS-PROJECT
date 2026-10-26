#include "lms/domain/User.hpp"

#include "lms/security/PasswordHasher.hpp"

namespace lms {

User::User(Id id, std::string fullName, std::string username, std::string passwordHash, bool active,
           std::optional<Id> departmentId)
    : userId_(id),
      fullName_(std::move(fullName)),
      username_(std::move(username)),
      passwordHash_(std::move(passwordHash)),
      active_(active),
      departmentId_(departmentId) {}

bool User::login(const std::string& username, const std::string& password) {
  loggedIn_ = false;
  if (!active_ || username != username_) return false;
  if (!PasswordHasher::verify(passwordHash_, password)) return false;
  loggedIn_ = true;
  return true;
}

const std::set<Permission>& Administrator::permissions() const {
  static const std::set<Permission> p{Permission::ManageUsers,      Permission::ManageDepartments,
                                      Permission::ManageAllCourses, Permission::ManageEnrolments,
                                      Permission::CreateCourse,     Permission::ChangeOwnPassword};
  return p;
}

const std::set<Permission>& Lecturer::permissions() const {
  static const std::set<Permission> p{
      Permission::CreateCourse,    Permission::ManageOwnCourses, Permission::CreateAssignment,
      Permission::GrantExtension,  Permission::MarkSubmissions,  Permission::SubmitForModeration,
      Permission::ViewAuditLog,    Permission::ChangeOwnPassword};
  return p;
}

const std::set<Permission>& HeadOfDepartment::permissions() const {
  static const std::set<Permission> p{Permission::ModerateMarks, Permission::ViewDepartment,
                                      Permission::ViewAuditLog, Permission::ChangeOwnPassword};
  return p;
}

const std::set<Permission>& Student::permissions() const {
  static const std::set<Permission> p{Permission::ViewOwnCourses, Permission::SubmitAssignment,
                                      Permission::ViewReleasedMarks, Permission::ChangeOwnPassword};
  return p;
}

std::unique_ptr<User> UserFactory::create(Role role, Id id, std::string fullName,
                                          std::string username, std::string passwordHash,
                                          bool active, std::optional<Id> departmentId) {
  switch (role) {
    case Role::Administrator:
      return std::make_unique<Administrator>(id, std::move(fullName), std::move(username),
                                             std::move(passwordHash), active, departmentId);
    case Role::Lecturer:
      return std::make_unique<Lecturer>(id, std::move(fullName), std::move(username),
                                        std::move(passwordHash), active, departmentId);
    case Role::HeadOfDepartment:
      return std::make_unique<HeadOfDepartment>(id, std::move(fullName), std::move(username),
                                                std::move(passwordHash), active, departmentId);
    case Role::Student:
      return std::make_unique<Student>(id, std::move(fullName), std::move(username),
                                       std::move(passwordHash), active, departmentId);
  }
  return nullptr;  // unreachable: all roles handled above
}

}  // namespace lms
