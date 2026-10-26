#pragma once
// Strategy pattern (Class Diagram 3.2.3): the late-penalty rule is behind this interface so that
// the submission/marking services never depend on one particular rule (OCP, DIP).
namespace lms {

class ILatePenaltyPolicy {
 public:
  virtual ~ILatePenaltyPolicy() = default;

  /// Marks to deduct when work is `lateDays` whole days late. `mark` is the mark that is
  /// available (the assignment's maximum). The final mark is max(0, awarded - penalty), so a
  /// penalty equal to `mark` is the "zero outcome" (more than three days late).
  virtual double calculatePenalty(int lateDays, double mark) const = 0;
};

}  // namespace lms
