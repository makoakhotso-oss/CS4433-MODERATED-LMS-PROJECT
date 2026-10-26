#pragma once
#include "lms/services/ILatePenaltyPolicy.hpp"

namespace lms {

/// School policy (HoD statement, FR-09): 10% of the available mark is lost for each day
/// late, for up to three days; later than that the mark is zero.
class StandardLatePenaltyPolicy final : public ILatePenaltyPolicy {
 public:
  explicit StandardLatePenaltyPolicy(double fractionPerDay = 0.10, int maxPenalisedDays = 3)
      : fractionPerDay_(fractionPerDay), maxDays_(maxPenalisedDays) {}

  double calculatePenalty(int lateDays, double mark) const override;

 private:
  double fractionPerDay_;
  int maxDays_;
};

}  // namespace lms
