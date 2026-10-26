#pragma once
#include <string>
#include <vector>

#include "lms/common/Time.hpp"
#include "lms/data/IRepositories.hpp"
#include "lms/data/IUnitOfWork.hpp"
#include "lms/services/MarkingTypes.hpp"

namespace lms {

struct ModerationReview {
  Assignment assignment;
  Course course;
  std::vector<MarkingRow> rows;
  int awaitingModeration{0};
  std::vector<ModerationDecision> decisions;
};

/// Moderation and release of marks by the Head of Department (FR-12, FR-13; UC-03).
/// A HoD can only moderate courses of their own department.
class ModerationService {
 public:
  ModerationService(ISubmissionRepository& submissions, IMarkRepository& marks,
                    IAssignmentRepository& assignments, ICourseRepository& courses,
                    IExtensionRepository& extensions, IModerationRepository& moderation,
                    const ILatePenaltyPolicy& penaltyPolicy, IUnitOfWork& unitOfWork,
                    const IClock& clock);

  ModerationReview reviewMarks(const User& hod, Id assignmentId) const;

  /// Approves the marks that are under moderation and releases them to the students in one
  /// step. Returns the number of marks released.
  int approveMarks(const User& hod, Id assignmentId, const std::string& comments = "");

  /// Returns the marks to the lecturer for correction; a reason is required.
  int rejectMarks(const User& hod, Id assignmentId, const std::string& reason);

 private:
  struct Scope {
    Assignment assignment;
    Course course;
  };
  Scope scopeFor(const User& hod, Id assignmentId) const;

  ISubmissionRepository& submissions_;
  IMarkRepository& marks_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  IModerationRepository& moderation_;
  IUnitOfWork& unitOfWork_;
  const IClock& clock_;
  MarkingRowBuilder rows_;
};

}  // namespace lms
