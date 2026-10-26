#include "lms/services/CourseAccess.hpp"

namespace lms {

bool CourseAccess::canView(const User& actor, const Course& course) const {
  switch (actor.role()) {
    case Role::Administrator: return true;
    case Role::Lecturer: return isCourseLecturer(actor, course);
    case Role::HeadOfDepartment: return isDepartmentHead(actor, course);
    case Role::Student: return enrolments_.isEnrolled(actor.userId(), course.courseId());
  }
  return false;
}

}  // namespace lms
