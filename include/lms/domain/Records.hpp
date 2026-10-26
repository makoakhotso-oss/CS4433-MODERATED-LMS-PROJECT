#pragma once
// Plain data records that have no behaviour of their own.
#include <optional>
#include <string>

#include "lms/common/Types.hpp"
#include "lms/domain/Assignment.hpp"
#include "lms/domain/Enums.hpp"

namespace lms {

struct Department {
  Id id{kNoId};
  std::string name;
};

/// Short description of a user for lists (no password hash!).
struct UserSummary {
  Id id{kNoId};
  std::string fullName;
  std::string username;
};

/// An individually approved deadline for one student and one assignment (FR-10).
struct Extension {
  Id id{kNoId};
  Id assignmentId{kNoId};
  Id studentId{kNoId};
  Id approvedBy{kNoId};
  Timestamp newDeadline{0};
  std::string reason;
};

/// A HoD decision on the marks of an assignment (moderation_decisions).
struct ModerationDecision {
  Id id{kNoId};
  Id assignmentId{kNoId};
  Id moderatorId{kNoId};
  DecisionType decision{DecisionType::Approved};
  std::string comments;
  Timestamp decidedAt{0};
};

/// One row of the mark audit log (FR-14, NFR-06).
struct AuditEntry {
  Id id{kNoId};
  Id markId{kNoId};
  Id changedBy{kNoId};
  std::optional<double> previousScore;
  double newScore{0.0};
  Timestamp changedAt{0};
  std::string reason;
};

/// The deadline that applies to a student: their approved extension if it is later than
/// the assignment deadline, otherwise the assignment deadline.
inline Timestamp applicableDeadline(const Assignment& assignment,
                                    const std::optional<Extension>& extension) {
  if (extension && extension->newDeadline > assignment.deadline()) return extension->newDeadline;
  return assignment.deadline();
}

}  // namespace lms
