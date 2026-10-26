#pragma once
#include <optional>
#include <string>

#include "lms/common/Types.hpp"
#include "lms/domain/Enums.hpp"

namespace lms {

/// The mark and feedback for one submission. `rawScore` is what the lecturer awarded;
/// `finalScore` is after the late penalty and is the mark the student sees.
class MarkRecord {
 public:
  MarkRecord(Id id, Id submissionId, double rawScore, double finalScore, std::string feedback,
             ModerationState state, std::optional<Timestamp> releasedAt)
      : markId_(id),
        submissionId_(submissionId),
        rawScore_(rawScore),
        finalScore_(finalScore),
        feedback_(std::move(feedback)),
        state_(state),
        releasedAt_(releasedAt) {}

  Id markId() const noexcept { return markId_; }
  Id submissionId() const noexcept { return submissionId_; }
  double rawScore() const noexcept { return rawScore_; }
  double finalScore() const noexcept { return finalScore_; }
  const std::string& feedback() const noexcept { return feedback_; }
  ModerationState state() const noexcept { return state_; }
  std::optional<Timestamp> releasedAt() const noexcept { return releasedAt_; }
  bool isReleased() const noexcept { return state_ == ModerationState::Released; }

  void updateMark(double rawScore, double finalScore, const std::string& feedback) {
    rawScore_ = rawScore;
    finalScore_ = finalScore;
    feedback_ = feedback;
  }
  void setState(ModerationState s) noexcept {
    state_ = s;
    if (s != ModerationState::Released) releasedAt_.reset();
  }
  void release(Timestamp when) noexcept {
    state_ = ModerationState::Released;
    releasedAt_ = when;
  }
  void setId(Id id) noexcept { markId_ = id; }

 private:
  Id markId_;
  Id submissionId_;
  double rawScore_;
  double finalScore_;
  std::string feedback_;
  ModerationState state_;
  std::optional<Timestamp> releasedAt_;
};

}  // namespace lms
