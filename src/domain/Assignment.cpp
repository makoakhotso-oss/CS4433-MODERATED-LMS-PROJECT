#include "lms/domain/Assignment.hpp"

#include <cmath>

#include "lms/common/Errors.hpp"

namespace lms {

Assignment::Assignment(Id id, Id courseId, Id createdBy, std::string title,
                       std::string description, Timestamp deadline, double maxScore)
    : assignmentId_(id),
      courseId_(courseId),
      createdBy_(createdBy),
      title_(std::move(title)),
      description_(std::move(description)),
      deadline_(deadline),
      maxScore_(maxScore) {
  if (title_.empty()) throw ValidationError("Assignment title must not be empty.");
  if (!std::isfinite(maxScore_) || maxScore_ <= 0.0) {
    throw ValidationError("Maximum mark must be greater than zero.");
  }
}

void Assignment::setDeadline(Timestamp newDeadline) {
  if (newDeadline <= deadline_) {
    throw BusinessRuleError("The new deadline must be later than the current deadline.");
  }
  deadline_ = newDeadline;
}

}  // namespace lms
