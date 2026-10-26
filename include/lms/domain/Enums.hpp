#pragma once
// Roles, permissions and workflow states used across the system.
#include <string>

namespace lms {

enum class Role { Administrator, Lecturer, HeadOfDepartment, Student };

/// Individual actions that the access-control component checks (FR-03, NFR-04).
enum class Permission {
  ManageUsers,        // create / update / deactivate accounts, reset passwords
  ManageDepartments,  // create departments
  ManageAllCourses,   // create courses for any lecturer, assign lecturers
  ManageEnrolments,   // enrol / unenrol students
  CreateCourse,       // create a course (lecturer: own course)
  ManageOwnCourses,   // rename own courses
  CreateAssignment,   // create assignments, move deadlines
  GrantExtension,     // individual extensions
  MarkSubmissions,    // record marks and feedback
  SubmitForModeration,
  ModerateMarks,      // review / approve / return marks (HoD)
  ViewDepartment,     // read department courses and assessment records (HoD)
  ViewAuditLog,
  ViewOwnCourses,     // student: enrolled courses and assignments
  SubmitAssignment,
  ViewReleasedMarks,
  ChangeOwnPassword
};

/// Life cycle of a submission as seen by staff (Class Diagram: SubmissionStatus).
enum class SubmissionStatus {
  Submitted,
  Late,
  Marked,
  UnderModeration,
  ChangesRequested,
  Approved,
  Released
};

/// Moderation state stored on each mark (marks.moderation_state).
enum class ModerationState { Marked, UnderModeration, ChangesRequested, Released };

/// Outcome recorded in moderation_decisions.
enum class DecisionType { Approved, ChangesRequested };

std::string toString(Role r);
Role roleFromString(const std::string& text);  // throws ValidationError
std::string toString(Permission p);
std::string toString(SubmissionStatus s);
std::string toString(ModerationState s);
ModerationState moderationStateFromString(const std::string& text);
std::string toString(DecisionType d);
DecisionType decisionFromString(const std::string& text);

}  // namespace lms
