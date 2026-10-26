#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/data/IRepositories.hpp"

namespace lms {

/// Records and reads the mark audit history (FR-14, NFR-06). Entries can only be
/// appended: the database refuses to update or delete them.
class AuditLog {
 public:
  AuditLog(IAuditRepository& audit, IAssignmentRepository& assignments, ICourseRepository& courses)
      : audit_(audit), assignments_(assignments), courses_(courses) {}

  /// Appends one entry (who, when, previous value, new value, reason).
  Id recordChange(Id markId, Id userId, std::optional<double> previousMark, double newMark,
                  const std::string& reason, Timestamp when);

  /// Mark change history of an assignment. Lecturers see their own courses, a HoD sees the
  /// courses of their department.
  std::vector<AuditRow> history(const User& actor, Id assignmentId) const;

 private:
  IAuditRepository& audit_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
};

}  // namespace lms
