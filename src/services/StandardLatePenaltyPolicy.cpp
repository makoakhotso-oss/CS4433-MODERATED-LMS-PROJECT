#include "lms/services/StandardLatePenaltyPolicy.hpp"

#include "lms/common/Validation.hpp"

namespace lms {

double StandardLatePenaltyPolicy::calculatePenalty(int lateDays, double mark) const {
  if (lateDays <= 0) return 0.0;
  if (lateDays > maxDays_) return validation::roundMark(mark);  // zero outcome: everything is lost
  return validation::roundMark(fractionPerDay_ * static_cast<double>(lateDays) * mark);
}

}  // namespace lms
