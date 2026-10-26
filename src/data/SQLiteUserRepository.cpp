#include "lms/common/Time.hpp"
#include "lms/data/SQLiteRepositories.hpp"

namespace lms {

namespace {

const char* const kUserSelect =
    "SELECT user_id, full_name, username, password_hash, role, active, department_id FROM users ";

std::unique_ptr<User> readUser(const Statement& st) {
  return UserFactory::create(roleFromString(st.columnText(4)), st.columnInt(0), st.columnText(1),
                             st.columnText(2), st.columnText(3), st.columnInt(5) != 0,
                             st.columnOptionalInt(6));
}

int scalarInt(Statement& st) { return st.step() ? static_cast<int>(st.columnInt(0)) : 0; }

}  // namespace

std::unique_ptr<User> SQLiteUserRepository::findById(Id id) const {
  Statement st(db_, std::string(kUserSelect) + "WHERE user_id = ?");
  st.bindInt(1, id);
  return st.step() ? readUser(st) : nullptr;
}

std::unique_ptr<User> SQLiteUserRepository::findByUsername(const std::string& username) const {
  Statement st(db_, std::string(kUserSelect) + "WHERE username = ?");
  st.bindText(1, username);
  return st.step() ? readUser(st) : nullptr;
}

std::vector<std::unique_ptr<User>> SQLiteUserRepository::findAll() const {
  Statement st(db_, std::string(kUserSelect) + "ORDER BY role, full_name, user_id");
  std::vector<std::unique_ptr<User>> out;
  while (st.step()) out.push_back(readUser(st));
  return out;
}

Id SQLiteUserRepository::insert(const NewUser& u) {
  Statement st(db_,
               "INSERT INTO users (full_name, username, password_hash, role, active, department_id) "
               "VALUES (?, ?, ?, ?, 1, ?)");
  st.bindText(1, u.fullName)
      .bindText(2, u.username)
      .bindText(3, u.passwordHash)
      .bindText(4, toString(u.role))
      .bindOptionalInt(5, u.departmentId);
  st.run();
  return db_.lastInsertRowId();
}

void SQLiteUserRepository::updateProfile(Id id, const std::string& fullName, Role role,
                                         std::optional<Id> departmentId) {
  Statement st(db_, "UPDATE users SET full_name = ?, role = ?, department_id = ? WHERE user_id = ?");
  st.bindText(1, fullName).bindText(2, toString(role)).bindOptionalInt(3, departmentId).bindInt(4, id);
  st.run();
}

void SQLiteUserRepository::setActive(Id id, bool active) {
  Statement st(db_, "UPDATE users SET active = ? WHERE user_id = ?");
  st.bindInt(1, active ? 1 : 0).bindInt(2, id);
  st.run();
}

void SQLiteUserRepository::updatePasswordHash(Id id, const std::string& hash) {
  Statement st(db_, "UPDATE users SET password_hash = ? WHERE user_id = ?");
  st.bindText(1, hash).bindInt(2, id);
  st.run();
}

int SQLiteUserRepository::countActiveAdministrators() const {
  Statement st(db_, "SELECT COUNT(*) FROM users WHERE role = 'Administrator' AND active = 1");
  return scalarInt(st);
}

int SQLiteUserRepository::countAll() const {
  Statement st(db_, "SELECT COUNT(*) FROM users");
  return scalarInt(st);
}

bool SQLiteUserRepository::hasAcademicRecords(Id id) const {
  Statement st(db_,
               "SELECT (SELECT COUNT(*) FROM courses WHERE lecturer_id = ?1) "
               "+ (SELECT COUNT(*) FROM enrollments WHERE student_id = ?1) "
               "+ (SELECT COUNT(*) FROM submissions WHERE student_id = ?1) "
               "+ (SELECT COUNT(*) FROM assignments WHERE created_by = ?1)");
  st.bindInt(1, id);
  return scalarInt(st) > 0;
}

// ---------------------------------------------------------------- departments

Id SQLiteDepartmentRepository::insert(const std::string& name) {
  Statement st(db_, "INSERT INTO departments (name) VALUES (?)");
  st.bindText(1, name);
  st.run();
  return db_.lastInsertRowId();
}

std::optional<Department> SQLiteDepartmentRepository::findById(Id id) const {
  Statement st(db_, "SELECT department_id, name FROM departments WHERE department_id = ?");
  st.bindInt(1, id);
  if (!st.step()) return std::nullopt;
  return Department{st.columnInt(0), st.columnText(1)};
}

std::optional<Department> SQLiteDepartmentRepository::findByName(const std::string& name) const {
  Statement st(db_, "SELECT department_id, name FROM departments WHERE name = ?");
  st.bindText(1, name);
  if (!st.step()) return std::nullopt;
  return Department{st.columnInt(0), st.columnText(1)};
}

std::vector<Department> SQLiteDepartmentRepository::findAll() const {
  Statement st(db_, "SELECT department_id, name FROM departments ORDER BY name");
  std::vector<Department> out;
  while (st.step()) out.push_back(Department{st.columnInt(0), st.columnText(1)});
  return out;
}

}  // namespace lms
