#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "lms/common/Time.hpp"
#include "lms/data/FileStorage.hpp"
#include "lms/data/IRepositories.hpp"
#include "lms/data/IUnitOfWork.hpp"
#include "lms/services/ILatePenaltyPolicy.hpp"

namespace lms {

struct SubmissionOptions {
  /// UC-01 exception flow says a submission after the late period is rejected, while the
  /// Sequence Diagram (3.3.4) records it with a zero outcome. The default follows the
  /// diagram and the Student Representative ("late work should not simply be rejected");
  /// set to true to reject such submissions outright.
  bool rejectBeyondLatePeriod{false};
};

struct SubmissionReceipt {
  Id submissionId{kNoId};
  int attemptNo{1};
  bool isResubmission{false};
  Timestamp submittedAt{0};
  Timestamp applicableDeadline{0};
  bool extensionApplied{false};
  int lateDays{0};
  double penaltyPercent{0.0};  // share of the available mark that will be lost (100 = zero)
};

/// Submission and resubmission rules (FR-07, FR-08, FR-09; UC-01).
class SubmissionService {
 public:
  SubmissionService(ISubmissionRepository& submissions, IAssignmentRepository& assignments,
                    IEnrolmentRepository& enrolments, IExtensionRepository& extensions,
                    IMarkRepository& marks, ILatePenaltyPolicy& penaltyPolicy,
                    const FileStorage& storage, IUnitOfWork& unitOfWork, const IClock& clock,
                    SubmissionOptions options = {});

  /// Submits the file as a new attempt, with an optional comment (UC-01 step 3). The first
  /// submission may be late; a replacement is only allowed up to the applicable deadline and
  /// only until the work has been marked.
  SubmissionReceipt submit(const User& student, Id assignmentId,
                           const std::filesystem::path& file, const std::string& comments = "");

  /// Replaces the student's current submission with a new attempt (Class Diagram:
  /// resubmit(submissionId, filePath)). The old attempt stays in the history.
  SubmissionReceipt resubmit(const User& student, Id submissionId,
                             const std::filesystem::path& file, const std::string& comments = "");

  /// The deadline that applies to this student (their extension, if any).
  Timestamp applicableDeadlineFor(const User& student, Id assignmentId) const;

  /// All attempts of the student for the assignment, oldest first.
  std::vector<Submission> history(const User& student, Id assignmentId) const;

  /// Percentage of the available mark lost for work `lateDays` late (100 = zero outcome).
  double calculatePenalty(int lateDays) const {
    return penaltyPolicy_.calculatePenalty(lateDays, 100.0);
  }

 private:
  ISubmissionRepository& submissions_;
  IAssignmentRepository& assignments_;
  IEnrolmentRepository& enrolments_;
  IExtensionRepository& extensions_;
  IMarkRepository& marks_;
  ILatePenaltyPolicy& penaltyPolicy_;
  const FileStorage& storage_;
  IUnitOfWork& unitOfWork_;
  const IClock& clock_;
  SubmissionOptions options_;
};

}  // namespace lms
