#pragma once
#include <string>
#include <vector>

#include "lms/common/Time.hpp"
#include "lms/data/IRepositories.hpp"
#include "lms/data/IUnitOfWork.hpp"
#include "lms/services/AuditLog.hpp"
#include "lms/services/MarkingTypes.hpp"

namespace lms {

/// Marking by the lecturer (FR-11, FR-14; UC-02).
class MarkingService {
 public:
  MarkingService(ISubmissionRepository& submissions, IMarkRepository& marks,
                 IAssignmentRepository& assignments, ICourseRepository& courses,
                 IExtensionRepository& extensions, const ILatePenaltyPolicy& penaltyPolicy,
                 AuditLog& auditLog, IUnitOfWork& unitOfWork, const IClock& clock);

  /// Current submissions of the assignment with lateness, penalty preview and mark state.
  std::vector<MarkingRow> listSubmissions(const User& lecturer, Id assignmentId) const;

  /// Records or corrects the mark and feedback of a submission. The late penalty is applied
  /// automatically. Correcting an existing mark needs a reason; if the mark was already under
  /// moderation or released it goes back to "Marked" and must be moderated again.
  MarkRecord recordMark(const User& lecturer, Id submissionId, double rawScore,
                        const std::string& feedback, const std::string& reason = "");

  /// Sends all marked work of the assignment to the HoD. Every current submission must have a
  /// mark. Returns the number of marks sent.
  int submitForModeration(const User& lecturer, Id assignmentId);

 private:
  struct Context {
    Submission submission;
    Assignment assignment;
    Course course;
  };
  Context contextFor(const User& lecturer, Id submissionId) const;
  Assignment assignmentFor(const User& lecturer, Id assignmentId) const;

  ISubmissionRepository& submissions_;
  IMarkRepository& marks_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  IExtensionRepository& extensions_;
  const ILatePenaltyPolicy& policy_;
  AuditLog& auditLog_;
  IUnitOfWork& unitOfWork_;
  const IClock& clock_;
  MarkingRowBuilder rows_;
};

}  // namespace lms
