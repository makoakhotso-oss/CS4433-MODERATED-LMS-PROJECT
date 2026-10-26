#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/common/Time.hpp"
#include "lms/data/IRepositories.hpp"
#include "lms/services/CourseAccess.hpp"

namespace lms {

/// Courses and enrolments (FR-04, FR-06). Lecturers create/manage their own courses
/// (SRS FR-04); the Administrator can also create courses for any lecturer, assign
/// lecturers and enrol students (case study).
class CourseService {
 public:
  CourseService(ICourseRepository& courses, IEnrolmentRepository& enrolments,
                IUserRepository& users, const IClock& clock);

  Course createCourse(const User& actor, const std::string& code, const std::string& name,
                      std::optional<Id> lecturerId = std::nullopt);
  void renameCourse(const User& actor, Id courseId, const std::string& newName);
  void assignLecturer(const User& actor, Id courseId, Id lecturerId);
  void enrolStudent(const User& actor, Id courseId, Id studentId);
  void unenrolStudent(const User& actor, Id courseId, Id studentId);

  std::vector<Course> listCourses(const User& actor) const;
  Course getCourse(const User& actor, Id courseId) const;
  std::vector<UserSummary> listEnrolledStudents(const User& actor, Id courseId) const;

 private:
  ICourseRepository& courses_;
  IEnrolmentRepository& enrolments_;
  IUserRepository& users_;
  const IClock& clock_;
  CourseAccess access_;
};

}  // namespace lms
