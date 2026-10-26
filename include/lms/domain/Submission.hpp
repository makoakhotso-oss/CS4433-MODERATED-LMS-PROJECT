#pragma once
#include <string>

#include "lms/common/Time.hpp"
#include "lms/common/Types.hpp"
#include "lms/domain/Enums.hpp"

namespace lms {

/// One submission attempt. Resubmissions are separate attempts (history is kept).
/// The attempt with the highest attemptNo is the "current" submission used for marking.
class Submission {
 public:
  Submission(Id id, Id assignmentId, Id studentId, int attemptNo, Timestamp submittedAt,
             std::string filePath, std::string comments = "")
      : submissionId_(id),
        assignmentId_(assignmentId),
        studentId_(studentId),
        attemptNo_(attemptNo),
        submittedAt_(submittedAt),
        filePath_(std::move(filePath)),
        comments_(std::move(comments)) {}

  Id submissionId() const noexcept { return submissionId_; }
  Id assignmentId() const noexcept { return assignmentId_; }
  Id studentId() const noexcept { return studentId_; }
  int attemptNo() const noexcept { return attemptNo_; }
  Timestamp submittedAt() const noexcept { return submittedAt_; }
  const std::string& getFilePath() const noexcept { return filePath_; }
  const std::string& comments() const noexcept { return comments_; }  // optional note from the student
  void setId(Id id) noexcept { submissionId_ = id; }

  /// Whole days after `applicableDeadline` (the student's extension if there is one).
  int calculateLateDays(Timestamp applicableDeadline) const {
    return wholeDaysLate(applicableDeadline, submittedAt_);
  }

  // Derived, not stored: filled in by the services when a submission is presented.
  SubmissionStatus status() const noexcept { return status_; }
  int lateDays() const noexcept { return lateDays_; }
  bool extensionApproved() const noexcept { return extensionApproved_; }
  void updateStatus(SubmissionStatus s) noexcept { status_ = s; }
  void setLateInfo(int lateDays, bool extensionApproved) noexcept {
    lateDays_ = lateDays;
    extensionApproved_ = extensionApproved;
  }

 private:
  Id submissionId_;
  Id assignmentId_;
  Id studentId_;
  int attemptNo_;
  Timestamp submittedAt_;
  std::string filePath_;
  std::string comments_;
  SubmissionStatus status_{SubmissionStatus::Submitted};
  int lateDays_{0};
  bool extensionApproved_{false};
};

}  // namespace lms
