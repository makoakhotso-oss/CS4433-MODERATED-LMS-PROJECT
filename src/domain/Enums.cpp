#include "lms/domain/Enums.hpp"

#include "lms/common/Errors.hpp"

namespace lms {

std::string toString(Role r) {
  switch (r) {
    case Role::Administrator: return "Administrator";
    case Role::Lecturer: return "Lecturer";
    case Role::HeadOfDepartment: return "HeadOfDepartment";
    case Role::Student: return "Student";
  }
  return "Unknown";
}

Role roleFromString(const std::string& text) {
  if (text == "Administrator") return Role::Administrator;
  if (text == "Lecturer") return Role::Lecturer;
  if (text == "HeadOfDepartment") return Role::HeadOfDepartment;
  if (text == "Student") return Role::Student;
  throw ValidationError("Unknown role: " + text);
}

std::string toString(Permission p) {
  switch (p) {
    case Permission::ManageUsers: return "ManageUsers";
    case Permission::ManageDepartments: return "ManageDepartments";
    case Permission::ManageAllCourses: return "ManageAllCourses";
    case Permission::ManageEnrolments: return "ManageEnrolments";
    case Permission::CreateCourse: return "CreateCourse";
    case Permission::ManageOwnCourses: return "ManageOwnCourses";
    case Permission::CreateAssignment: return "CreateAssignment";
    case Permission::GrantExtension: return "GrantExtension";
    case Permission::MarkSubmissions: return "MarkSubmissions";
    case Permission::SubmitForModeration: return "SubmitForModeration";
    case Permission::ModerateMarks: return "ModerateMarks";
    case Permission::ViewDepartment: return "ViewDepartment";
    case Permission::ViewAuditLog: return "ViewAuditLog";
    case Permission::ViewOwnCourses: return "ViewOwnCourses";
    case Permission::SubmitAssignment: return "SubmitAssignment";
    case Permission::ViewReleasedMarks: return "ViewReleasedMarks";
    case Permission::ChangeOwnPassword: return "ChangeOwnPassword";
  }
  return "Unknown";
}

std::string toString(SubmissionStatus s) {
  switch (s) {
    case SubmissionStatus::Submitted: return "Submitted";
    case SubmissionStatus::Late: return "Late";
    case SubmissionStatus::Marked: return "Marked";
    case SubmissionStatus::UnderModeration: return "Under moderation";
    case SubmissionStatus::ChangesRequested: return "Changes requested";
    case SubmissionStatus::Approved: return "Approved";
    case SubmissionStatus::Released: return "Released";
  }
  return "Unknown";
}

std::string toString(ModerationState s) {
  switch (s) {
    case ModerationState::Marked: return "Marked";
    case ModerationState::UnderModeration: return "UnderModeration";
    case ModerationState::ChangesRequested: return "ChangesRequested";
    case ModerationState::Released: return "Released";
  }
  return "Unknown";
}

ModerationState moderationStateFromString(const std::string& text) {
  if (text == "Marked") return ModerationState::Marked;
  if (text == "UnderModeration") return ModerationState::UnderModeration;
  if (text == "ChangesRequested") return ModerationState::ChangesRequested;
  if (text == "Released") return ModerationState::Released;
  throw ValidationError("Unknown moderation state: " + text);
}

std::string toString(DecisionType d) {
  return d == DecisionType::Approved ? "Approved" : "ChangesRequested";
}

DecisionType decisionFromString(const std::string& text) {
  if (text == "Approved") return DecisionType::Approved;
  if (text == "ChangesRequested") return DecisionType::ChangesRequested;
  throw ValidationError("Unknown moderation decision: " + text);
}

}  // namespace lms
