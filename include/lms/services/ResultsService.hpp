#pragma once
#include <string>
#include <vector>

#include "lms/data/IRepositories.hpp"

namespace lms {

/// What a student sees for a released mark.
struct ReleasedMarkView {
  std::string courseCode;
  std::string courseName;
  std::string assignmentTitle;
  double maxScore{0.0};
  double rawScore{0.0};
  double finalScore{0.0};
  double penaltyPoints{0.0};
  int lateDays{0};
  int attemptNo{1};
  std::string feedback;
  Timestamp releasedAt{0};
};

/// Student access to released marks and feedback (FR-13, FR-15).
class ResultsService {
 public:
  ResultsService(IMarkRepository& marks, IExtensionRepository& extensions)
      : marks_(marks), extensions_(extensions) {}

  /// Only the student's own marks that the HoD has released. Unreleased marks are never
  /// returned - the repository query itself filters them out.
  std::vector<ReleasedMarkView> releasedMarks(const User& student) const;

 private:
  IMarkRepository& marks_;
  IExtensionRepository& extensions_;
};

}  // namespace lms
