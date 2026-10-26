#include "lms/services/AssignmentService.hpp"

#include <cmath>
#include <map>

#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"
#include "lms/common/Validation.hpp"
#include "lms/services/AccessControl.hpp"

namespace lms {

AssignmentService::AssignmentService(IAssignmentRepository& assignments, ICourseRepository& courses,
                                     IEnrolmentRepository& enrolments,
                                     IExtensionRepository& extensions,
                                     ISubmissionRepository& submissions, IMarkRepository& marks,
                                     IUserRepository& users, const IClock& clock)
    : assignments_(assignments),
      courses_(courses),
      enrolments_(enrolments),
      extensions_(extensions),
      submissions_(submissions),
      marks_(marks),
      users_(users),
      clock_(clock),
      access_(enrolments) {}

Assignment AssignmentService::createAssignment(const User& actor, Id courseId,
                                               const std::string& title,
                                               const std::string& description, Timestamp deadline,
                                               double maxScore) {
  AccessControl::require(actor, Permission::CreateAssignment);
  const std::string t = validation::requireText(title, "Title", 200);
  const std::string d = validation::requireText(description, "Description", 2000);
  if (!std::isfinite(maxScore) || maxScore <= 0.0 || maxScore > 1000.0) {
    throw ValidationError("Maximum mark must be greater than 0 and at most 1000.");
  }
  if (deadline <= clock_.now()) throw ValidationError("The deadline must be in the future.");
  const std::optional<Course> course = courses_.findById(courseId);
  if (!course) throw NotFoundError("Course not found.");
  if (!CourseAccess::isCourseLecturer(actor, *course)) {
    throw PermissionDeniedError("You can only create assignments in your own courses.");
  }
  NewAssignment a;
  a.courseId = courseId;
  a.createdBy = actor.userId();
  a.title = t;
  a.description = d;
  a.deadline = deadline;
  a.maxScore = validation::roundMark(maxScore);
  const Id id = assignments_.insert(a);
  return Assignment(id, courseId, actor.userId(), t, d, deadline, a.maxScore);
}

Extension AssignmentService::grantExtension(const User& actor, Id assignmentId, Id studentId,
                                            Timestamp newDeadline, const std::string& reason) {
  AccessControl::require(actor, Permission::GrantExtension);
  const std::string why = validation::requireText(reason, "Reason", 500);
  const std::optional<Assignment> a = assignments_.findById(assignmentId);
  if (!a) throw NotFoundError("Assignment not found.");
  const std::optional<Course> course = courses_.findById(a->courseId());
  if (!course || !CourseAccess::isCourseLecturer(actor, *course)) {
    throw PermissionDeniedError("You can only grant extensions in your own courses.");
  }
  if (!enrolments_.isEnrolled(studentId, course->courseId())) {
    throw NotFoundError("That student is not enrolled in this course.");
  }
  if (newDeadline <= a->deadline()) {
    throw ValidationError("The extended deadline must be later than the assignment deadline.");
  }
  // An extension granted after the student already submitted late would silently change
  // the penalty of marked work, so it is refused once the current attempt has a mark.
  if (const auto latest = submissions_.findLatest(studentId, assignmentId)) {
    if (marks_.findBySubmission(latest->submissionId())) {
      throw BusinessRuleError("The student's work has already been marked; an extension would change a recorded mark.");
    }
  }
  Extension e;
  e.assignmentId = assignmentId;
  e.studentId = studentId;
  e.approvedBy = actor.userId();
  e.newDeadline = newDeadline;
  e.reason = why;
  extensions_.upsert(e);
  Logger::instance().info("Extension granted: assignment " + std::to_string(assignmentId) +
                          ", student " + std::to_string(studentId) + " by '" + actor.username() + "'");
  return *extensions_.find(assignmentId, studentId);
}

std::vector<Assignment> AssignmentService::listAssignments(const User& actor, Id courseId) const {
  if (!actor.isLoggedIn()) throw PermissionDeniedError("You are not allowed to perform this action.");
  const std::optional<Course> course = courses_.findById(courseId);
  if (!course || !access_.canView(actor, *course)) throw NotFoundError("Course not found.");
  return assignments_.findByCourse(courseId);
}

Assignment AssignmentService::getAssignment(const User& actor, Id assignmentId) const {
  if (!actor.isLoggedIn()) throw PermissionDeniedError("You are not allowed to perform this action.");
  const std::optional<Assignment> a = assignments_.findById(assignmentId);
  if (!a) throw NotFoundError("Assignment not found.");
  const std::optional<Course> course = courses_.findById(a->courseId());
  if (!course || !access_.canView(actor, *course)) throw NotFoundError("Assignment not found.");
  return *a;
}

std::vector<StudentAssignmentRow> AssignmentService::listForStudent(const User& actor) const {
  AccessControl::require(actor, Permission::ViewOwnCourses);
  std::map<Id, Submission> latestByAssignment;
  for (const Submission& s : submissions_.findCurrentByStudent(actor.userId())) {
    latestByAssignment.emplace(s.assignmentId(), s);
  }
  std::vector<StudentAssignmentRow> rows;
  for (const Course& course : courses_.findByStudent(actor.userId())) {
    for (const Assignment& a : assignments_.findByCourse(course.courseId())) {
      StudentAssignmentRow row{a, course.courseCode(), course.courseName(), std::nullopt, ""};
      const auto it = latestByAssignment.find(a.assignmentId());
      if (it == latestByAssignment.end()) {
        row.statusText = clock_.now() > a.deadline() ? "Not submitted (deadline passed)"
                                                     : "Not submitted";
      } else {
        row.latest = it->second;
        const std::optional<Extension> ext = extensions_.find(a.assignmentId(), actor.userId());
        const int late = it->second.calculateLateDays(applicableDeadline(a, ext));
        const std::optional<MarkRecord> mark = marks_.findBySubmission(it->second.submissionId());
        std::string text = "Submitted (attempt " + std::to_string(it->second.attemptNo()) + ")";
        if (late > 0) text += ", " + std::to_string(late) + " day(s) late";
        if (mark && mark->isReleased()) {
          text = "Mark released";
        } else if (mark) {
          text += " - awaiting release";
        }
        row.statusText = text;
      }
      rows.push_back(std::move(row));
    }
  }
  return rows;
}

}  // namespace lms
