#pragma once
#include <string>

#include "lms/common/Types.hpp"

namespace lms {

class Assignment {
 public:
  Assignment(Id id, Id courseId, Id createdBy, std::string title, std::string description,
             Timestamp deadline, double maxScore);

  Id assignmentId() const noexcept { return assignmentId_; }
  Id courseId() const noexcept { return courseId_; }
  Id createdBy() const noexcept { return createdBy_; }
  const std::string& title() const noexcept { return title_; }
  const std::string& description() const noexcept { return description_; }
  Timestamp deadline() const noexcept { return deadline_; }
  double maxScore() const noexcept { return maxScore_; }

  /// True while the normal (unextended) deadline has not passed.
  bool isOpen(Timestamp now) const noexcept { return now <= deadline_; }

  /// Moves the deadline later. Throws BusinessRuleError if `newDeadline` is not later than
  /// the current one (shortening a deadline after students have planned for it is unfair).
  void setDeadline(Timestamp newDeadline);

 private:
  Id assignmentId_;
  Id courseId_;
  Id createdBy_;
  std::string title_;
  std::string description_;
  Timestamp deadline_;
  double maxScore_;
};

}  // namespace lms
