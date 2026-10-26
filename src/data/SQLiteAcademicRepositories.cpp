#include "lms/common/Time.hpp"
#include "lms/data/SQLiteRepositories.hpp"

namespace lms {

namespace {

const char* const kCourseSelect =
    "SELECT course_id, course_code, course_name, lecturer_id, department_id FROM courses ";

Course readCourse(const Statement& st) {
  return Course(st.columnInt(0), st.columnText(1), st.columnText(2), st.columnInt(3),
                st.columnInt(4));
}

std::vector<Course> readCourses(Statement& st) {
  std::vector<Course> out;
  while (st.step()) out.push_back(readCourse(st));
  return out;
}

const char* const kAssignmentSelect =
    "SELECT assignment_id, course_id, created_by, title, description, deadline, max_score "
    "FROM assignments ";

Assignment readAssignment(const Statement& st) {
  return Assignment(st.columnInt(0), st.columnInt(1), st.columnInt(2), st.columnText(3),
                    st.columnText(4), fromDbString(st.columnText(5)), st.columnDouble(6));
}

Extension readExtension(const Statement& st) {
  Extension e;
  e.id = st.columnInt(0);
  e.assignmentId = st.columnInt(1);
  e.studentId = st.columnInt(2);
  e.approvedBy = st.columnInt(3);
  e.newDeadline = fromDbString(st.columnText(4));
  e.reason = st.columnText(5);
  return e;
}

}  // namespace

// --------------------------------------------------------------------- courses

Id SQLiteCourseRepository::insert(const NewCourse& c) {
  Statement st(db_,
               "INSERT INTO courses (course_code, course_name, lecturer_id, department_id) "
               "VALUES (?, ?, ?, ?)");
  st.bindText(1, c.code).bindText(2, c.name).bindInt(3, c.lecturerId).bindInt(4, c.departmentId);
  st.run();
  return db_.lastInsertRowId();
}

std::optional<Course> SQLiteCourseRepository::findById(Id id) const {
  Statement st(db_, std::string(kCourseSelect) + "WHERE course_id = ?");
  st.bindInt(1, id);
  if (!st.step()) return std::nullopt;
  return readCourse(st);
}

std::optional<Course> SQLiteCourseRepository::findByCode(const std::string& code) const {
  Statement st(db_, std::string(kCourseSelect) + "WHERE course_code = ?");
  st.bindText(1, code);
  if (!st.step()) return std::nullopt;
  return readCourse(st);
}

std::vector<Course> SQLiteCourseRepository::findAll() const {
  Statement st(db_, std::string(kCourseSelect) + "ORDER BY course_code");
  return readCourses(st);
}

std::vector<Course> SQLiteCourseRepository::findByLecturer(Id lecturerId) const {
  Statement st(db_, std::string(kCourseSelect) + "WHERE lecturer_id = ? ORDER BY course_code");
  st.bindInt(1, lecturerId);
  return readCourses(st);
}

std::vector<Course> SQLiteCourseRepository::findByDepartment(Id departmentId) const {
  Statement st(db_, std::string(kCourseSelect) + "WHERE department_id = ? ORDER BY course_code");
  st.bindInt(1, departmentId);
  return readCourses(st);
}

std::vector<Course> SQLiteCourseRepository::findByStudent(Id studentId) const {
  Statement st(db_,
               "SELECT c.course_id, c.course_code, c.course_name, c.lecturer_id, c.department_id "
               "FROM courses c JOIN enrollments e ON e.course_id = c.course_id "
               "WHERE e.student_id = ? ORDER BY c.course_code");
  st.bindInt(1, studentId);
  return readCourses(st);
}

void SQLiteCourseRepository::update(const Course& c) {
  Statement st(db_, "UPDATE courses SET course_name = ?, lecturer_id = ? WHERE course_id = ?");
  st.bindText(1, c.courseName()).bindInt(2, c.lecturerId()).bindInt(3, c.courseId());
  st.run();
}

// ----------------------------------------------------------------- enrolments

void SQLiteEnrolmentRepository::enrol(Id studentId, Id courseId, Timestamp when) {
  Statement st(db_, "INSERT INTO enrollments (student_id, course_id, enrolled_at) VALUES (?, ?, ?)");
  st.bindInt(1, studentId).bindInt(2, courseId).bindText(3, toDbString(when));
  st.run();
}

void SQLiteEnrolmentRepository::unenrol(Id studentId, Id courseId) {
  Statement st(db_, "DELETE FROM enrollments WHERE student_id = ? AND course_id = ?");
  st.bindInt(1, studentId).bindInt(2, courseId);
  st.run();
}

bool SQLiteEnrolmentRepository::isEnrolled(Id studentId, Id courseId) const {
  Statement st(db_, "SELECT 1 FROM enrollments WHERE student_id = ? AND course_id = ?");
  st.bindInt(1, studentId).bindInt(2, courseId);
  return st.step();
}

std::vector<UserSummary> SQLiteEnrolmentRepository::findStudents(Id courseId) const {
  Statement st(db_,
               "SELECT u.user_id, u.full_name, u.username FROM users u "
               "JOIN enrollments e ON e.student_id = u.user_id "
               "WHERE e.course_id = ? ORDER BY u.full_name, u.user_id");
  st.bindInt(1, courseId);
  std::vector<UserSummary> out;
  while (st.step()) out.push_back(UserSummary{st.columnInt(0), st.columnText(1), st.columnText(2)});
  return out;
}

bool SQLiteEnrolmentRepository::hasSubmissions(Id studentId, Id courseId) const {
  Statement st(db_,
               "SELECT 1 FROM submissions s JOIN assignments a ON a.assignment_id = s.assignment_id "
               "WHERE s.student_id = ? AND a.course_id = ? LIMIT 1");
  st.bindInt(1, studentId).bindInt(2, courseId);
  return st.step();
}

// ---------------------------------------------------------------- assignments

Id SQLiteAssignmentRepository::insert(const NewAssignment& a) {
  Statement st(db_,
               "INSERT INTO assignments (course_id, created_by, title, description, deadline, "
               "max_score) VALUES (?, ?, ?, ?, ?, ?)");
  st.bindInt(1, a.courseId)
      .bindInt(2, a.createdBy)
      .bindText(3, a.title)
      .bindText(4, a.description)
      .bindText(5, toDbString(a.deadline))
      .bindDouble(6, a.maxScore);
  st.run();
  return db_.lastInsertRowId();
}

std::optional<Assignment> SQLiteAssignmentRepository::findById(Id id) const {
  Statement st(db_, std::string(kAssignmentSelect) + "WHERE assignment_id = ?");
  st.bindInt(1, id);
  if (!st.step()) return std::nullopt;
  return readAssignment(st);
}

std::vector<Assignment> SQLiteAssignmentRepository::findByCourse(Id courseId) const {
  Statement st(db_, std::string(kAssignmentSelect) + "WHERE course_id = ? ORDER BY deadline, assignment_id");
  st.bindInt(1, courseId);
  std::vector<Assignment> out;
  while (st.step()) out.push_back(readAssignment(st));
  return out;
}

void SQLiteAssignmentRepository::updateDeadline(Id assignmentId, Timestamp deadline) {
  Statement st(db_, "UPDATE assignments SET deadline = ? WHERE assignment_id = ?");
  st.bindText(1, toDbString(deadline)).bindInt(2, assignmentId);
  st.run();
}

// ----------------------------------------------------------------- extensions

void SQLiteExtensionRepository::upsert(const Extension& e) {
  Statement st(db_,
               "INSERT INTO extensions (assignment_id, student_id, approved_by, new_deadline, reason) "
               "VALUES (?, ?, ?, ?, ?) "
               "ON CONFLICT (assignment_id, student_id) DO UPDATE SET "
               "approved_by = excluded.approved_by, new_deadline = excluded.new_deadline, "
               "reason = excluded.reason");
  st.bindInt(1, e.assignmentId)
      .bindInt(2, e.studentId)
      .bindInt(3, e.approvedBy)
      .bindText(4, toDbString(e.newDeadline))
      .bindText(5, e.reason);
  st.run();
}

std::optional<Extension> SQLiteExtensionRepository::find(Id assignmentId, Id studentId) const {
  Statement st(db_,
               "SELECT extension_id, assignment_id, student_id, approved_by, new_deadline, reason "
               "FROM extensions WHERE assignment_id = ? AND student_id = ?");
  st.bindInt(1, assignmentId).bindInt(2, studentId);
  if (!st.step()) return std::nullopt;
  return readExtension(st);
}

std::vector<Extension> SQLiteExtensionRepository::findByAssignment(Id assignmentId) const {
  Statement st(db_,
               "SELECT extension_id, assignment_id, student_id, approved_by, new_deadline, reason "
               "FROM extensions WHERE assignment_id = ?");
  st.bindInt(1, assignmentId);
  std::vector<Extension> out;
  while (st.step()) out.push_back(readExtension(st));
  return out;
}

}  // namespace lms
