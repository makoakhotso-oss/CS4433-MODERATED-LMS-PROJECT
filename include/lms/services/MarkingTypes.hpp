#pragma once
#include <optional>
#include <string>
#include <vector>

#include "lms/data/IRepositories.hpp"
#include "lms/services/ILatePenaltyPolicy.hpp"

namespace lms {

/// One student's current submission with its lateness, penalty preview and mark.
struct MarkingRow {
  Submission submission;
  std::string studentName;
  std::string studentUsername;
  Timestamp applicableDeadline{0};
  int lateDays{0};
  double penaltyPercent{0.0};  // 0 = on time, 100 = zero outcome
  std::optional<MarkRecord> mark;
  SubmissionStatus status{SubmissionStatus::Submitted};
};

/// Builds MarkingRows with three queries per assignment (not three per student), which
/// keeps the lists fast for a class of hundreds of students (NFR-01).
class MarkingRowBuilder {
 public:
  MarkingRowBuilder(ISubmissionRepository& submissions, IMarkRepository& marks,
                    IExtensionRepository& extensions, const ILatePenaltyPolicy& policy)
      : submissions_(submissions), marks_(marks), extensions_(extensions), policy_(policy) {}

  std::vector<MarkingRow> build(const Assignment& assignment) const;

  static SubmissionStatus deriveStatus(int lateDays, const std::optional<MarkRecord>& mark);

 private:
  ISubmissionRepository& submissions_;
  IMarkRepository& marks_;
  IExtensionRepository& extensions_;
  const ILatePenaltyPolicy& policy_;
};

}  // namespace lms
