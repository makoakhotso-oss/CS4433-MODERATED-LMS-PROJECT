#include "lms/services/CourseService.hpp"

#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"
#include "lms/common/Validation.hpp"
#include "lms/services/AccessControl.hpp"

namespace lms {

CourseService::CourseService(ICourseRepository& courses, IEnrolmentRepository& enrolments,
                             IUserRepository& users, const IClock& clock)
    : courses_(courses), enrolments_(enrolments), users_(users), clock_(clock), access_(enrolments) {}

Course CourseService::createCourse(const User& actor, const std::string& rawCode,
                                   const std::string& rawName, std::optional<Id> lecturerId) {
  AccessControl::require(actor, Permission::CreateCourse);
  const std::string code = validation::normaliseCourseCode(rawCode);
  const std::string name = validation::requireText(rawName, "Course name", 100);
  if (courses_.findByCode(code)) throw BusinessRuleError("A course with that code already exists.");

  Id lecturer;
  std::optional<Id> department;
  if (actor.role() == Role::Lecturer) {
    lecturer = actor.userId();  // a lecturer can only create a course for themselves
    department = actor.departmentId();
  } else {
    AccessControl::require(actor, Permission::ManageAllCourses);
    if (!lecturerId) throw ValidationError("Choose the lecturer who will teach the course.");
    const std::unique_ptr<User> l = users_.findById(*lecturerId);
    if (!l || l->role() != Role::Lecturer) throw NotFoundError("Lecturer not found.");
    if (!l->isActive()) throw BusinessRuleError("That lecturer's account is deactivated.");
    lecturer = l->userId();
    department = l->departmentId();
  }
  if (!department) {
    throw BusinessRuleError("The lecturer has no department. Ask an administrator to set one.");
  }

  NewCourse c;
  c.code = code;
  c.name = name;
  c.lecturerId = lecturer;
  c.departmentId = *department;
  const Id id = courses_.insert(c);
  return Course(id, code, name, lecturer, *department);
}

void CourseService::renameCourse(const User& actor, Id courseId, const std::string& newName) {
  const std::string name = validation::requireText(newName, "Course name", 100);
  std::optional<Course> course = courses_.findById(courseId);
  if (!course) throw NotFoundError("Course not found.");
  if (actor.role() == Role::Lecturer) {
    AccessControl::require(actor, Permission::ManageOwnCourses);
    if (!CourseAccess::isCourseLecturer(actor, *course)) {
      throw PermissionDeniedError("You can only manage your own courses.");
    }
  } else {
    AccessControl::require(actor, Permission::ManageAllCourses);
  }
  course->rename(name);
  courses_.update(*course);
}

void CourseService::assignLecturer(const User& actor, Id courseId, Id lecturerId) {
  AccessControl::require(actor, Permission::ManageAllCourses);
  std::optional<Course> course = courses_.findById(courseId);
  if (!course) throw NotFoundError("Course not found.");
  const std::unique_ptr<User> l = users_.findById(lecturerId);
  if (!l || l->role() != Role::Lecturer) throw NotFoundError("Lecturer not found.");
  if (!l->isActive()) throw BusinessRuleError("That lecturer's account is deactivated.");
  if (!l->departmentId() || *l->departmentId() != course->departmentId()) {
    throw BusinessRuleError("The lecturer must belong to the department of the course.");
  }
  course->assignLecturer(lecturerId);
  courses_.update(*course);
}

void CourseService::enrolStudent(const User& actor, Id courseId, Id studentId) {
  AccessControl::require(actor, Permission::ManageEnrolments);
  if (!courses_.findById(courseId)) throw NotFoundError("Course not found.");
  const std::unique_ptr<User> s = users_.findById(studentId);
  if (!s || s->role() != Role::Student) throw NotFoundError("Student not found.");
  if (!s->isActive()) throw BusinessRuleError("That student's account is deactivated.");
  if (enrolments_.isEnrolled(studentId, courseId)) {
    throw BusinessRuleError("The student is already enrolled in this course.");
  }
  enrolments_.enrol(studentId, courseId, clock_.now());
}

void CourseService::unenrolStudent(const User& actor, Id courseId, Id studentId) {
  AccessControl::require(actor, Permission::ManageEnrolments);
  if (!enrolments_.isEnrolled(studentId, courseId)) {
    throw NotFoundError("The student is not enrolled in this course.");
  }
  if (enrolments_.hasSubmissions(studentId, courseId)) {
    throw BusinessRuleError("The student has submissions in this course and cannot be removed.");
  }
  enrolments_.unenrol(studentId, courseId);
}

std::vector<Course> CourseService::listCourses(const User& actor) const {
  switch (actor.role()) {
    case Role::Administrator:
      AccessControl::require(actor, Permission::ManageAllCourses);
      return courses_.findAll();
    case Role::Lecturer:
      AccessControl::require(actor, Permission::ManageOwnCourses);
      return courses_.findByLecturer(actor.userId());
    case Role::HeadOfDepartment:
      AccessControl::require(actor, Permission::ViewDepartment);
      if (!actor.departmentId()) return {};
      return courses_.findByDepartment(*actor.departmentId());
    case Role::Student:
      AccessControl::require(actor, Permission::ViewOwnCourses);
      return courses_.findByStudent(actor.userId());
  }
  return {};
}

Course CourseService::getCourse(const User& actor, Id courseId) const {
  if (!actor.isLoggedIn()) throw PermissionDeniedError("You are not allowed to perform this action.");
  const std::optional<Course> course = courses_.findById(courseId);
  // The same message for "missing" and "not yours" avoids revealing which ids exist.
  if (!course || !access_.canView(actor, *course)) throw NotFoundError("Course not found.");
  return *course;
}

std::vector<UserSummary> CourseService::listEnrolledStudents(const User& actor,
                                                             Id courseId) const {
  if (actor.role() == Role::Student) {
    throw PermissionDeniedError("You are not allowed to perform this action.");
  }
  const Course course = getCourse(actor, courseId);
  return enrolments_.findStudents(course.courseId());
}

}  // namespace lms
