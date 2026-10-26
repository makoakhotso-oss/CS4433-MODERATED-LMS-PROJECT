#include "lms/services/ResultsService.hpp"

#include "lms/common/Validation.hpp"
#include "lms/services/AccessControl.hpp"

namespace lms {

std::vector<ReleasedMarkView> ResultsService::releasedMarks(const User& student) const {
  AccessControl::require(student, Permission::ViewReleasedMarks);
  std::vector<ReleasedMarkView> out;
  for (const ReleasedMarkRow& row : marks_.findReleasedByStudent(student.userId())) {
    const std::optional<Extension> ext = extensions_.find(row.assignment.assignmentId(), student.userId());
    ReleasedMarkView v;
    v.courseCode = row.courseCode;
    v.courseName = row.courseName;
    v.assignmentTitle = row.assignment.title();
    v.maxScore = row.assignment.maxScore();
    v.rawScore = row.mark.rawScore();
    v.finalScore = row.mark.finalScore();
    v.penaltyPoints = validation::roundMark(row.mark.rawScore() - row.mark.finalScore());
    v.lateDays = row.submission.calculateLateDays(applicableDeadline(row.assignment, ext));
    v.attemptNo = row.submission.attemptNo();
    v.feedback = row.mark.feedback();
    v.releasedAt = row.mark.releasedAt().value_or(0);
    out.push_back(std::move(v));
  }
  return out;
}

}  // namespace lms
