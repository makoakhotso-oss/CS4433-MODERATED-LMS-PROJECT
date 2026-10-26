#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/common/Time.hpp"
#include "lms/data/IRepositories.hpp"
#include "lms/services/CourseAccess.hpp"

namespace lms {

/// One line of a student's assignment list.
struct StudentAssignmentRow {
  Assignment assignment;
  std::string courseCode;
  std::string courseName;
  std::optional<Submission> latest;  // the student's current attempt, if any
  std::string statusText;
};

/// Assignments and individual extensions (FR-05, FR-06, FR-10).
class AssignmentService {
 public:
  AssignmentService(IAssignmentRepository& assignments, ICourseRepository& courses,
                    IEnrolmentRepository& enrolments, IExtensionRepository& extensions,
                    ISubmissionRepository& submissions, IMarkRepository& marks,
                    IUserRepository& users, const IClock& clock);

  Assignment createAssignment(const User& actor, Id courseId, const std::string& title,
                              const std::string& description, Timestamp deadline, double maxScore);
  /// Gives ONE student a later deadline for ONE assignment (replaces an earlier extension).
  Extension grantExtension(const User& actor, Id assignmentId, Id studentId, Timestamp newDeadline,
                           const std::string& reason);

  std::vector<Assignment> listAssignments(const User& actor, Id courseId) const;
  Assignment getAssignment(const User& actor, Id assignmentId) const;
  std::vector<StudentAssignmentRow> listForStudent(const User& actor) const;

 private:
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  IEnrolmentRepository& enrolments_;
  IExtensionRepository& extensions_;
  ISubmissionRepository& submissions_;
  IMarkRepository& marks_;
  IUserRepository& users_;
  const IClock& clock_;
  CourseAccess access_;
};

}  // namespace lms
