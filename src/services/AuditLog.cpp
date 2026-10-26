#include "lms/services/AuditLog.hpp"

#include "lms/common/Errors.hpp"
#include "lms/services/AccessControl.hpp"
#include "lms/services/CourseAccess.hpp"

namespace lms {

Id AuditLog::recordChange(Id markId, Id userId, std::optional<double> previousMark, double newMark,
                          const std::string& reason, Timestamp when) {
  AuditEntry e;
  e.markId = markId;
  e.changedBy = userId;
  e.previousScore = previousMark;
  e.newScore = newMark;
  e.changedAt = when;
  e.reason = reason;
  return audit_.append(e);
}

std::vector<AuditRow> AuditLog::history(const User& actor, Id assignmentId) const {
  AccessControl::require(actor, Permission::ViewAuditLog);
  const std::optional<Assignment> a = assignments_.findById(assignmentId);
  const std::optional<Course> course = a ? courses_.findById(a->courseId()) : std::nullopt;
  if (!course || !(CourseAccess::isCourseLecturer(actor, *course) ||
                   CourseAccess::isDepartmentHead(actor, *course))) {
    throw NotFoundError("Assignment not found.");
  }
  return audit_.findByAssignment(assignmentId);
}

}  // namespace lms
