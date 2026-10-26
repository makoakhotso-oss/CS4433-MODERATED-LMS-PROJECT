#pragma once
#include "lms/data/IRepositories.hpp"

namespace lms {

/// Who may see a course and what belongs to it. Shared by the course, assignment and
/// moderation services so the rule is written once.
///   Administrator: every course        Lecturer: the courses they teach
///   HoD: courses of their department   Student: courses they are enrolled in
class CourseAccess {
 public:
  explicit CourseAccess(const IEnrolmentRepository& enrolments) : enrolments_(enrolments) {}
  bool canView(const User& actor, const Course& course) const;
  /// Staff who may manage the content of the course (its lecturer).
  static bool isCourseLecturer(const User& actor, const Course& course) {
    return actor.role() == Role::Lecturer && course.isTaughtBy(actor.userId());
  }
  /// The HoD of the course's department.
  static bool isDepartmentHead(const User& actor, const Course& course) {
    return actor.role() == Role::HeadOfDepartment && actor.departmentId() &&
           *actor.departmentId() == course.departmentId();
  }

 private:
  const IEnrolmentRepository& enrolments_;
};

}  // namespace lms
