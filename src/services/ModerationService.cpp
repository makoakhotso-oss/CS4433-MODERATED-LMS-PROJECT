#include "lms/services/ModerationService.hpp"

#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"
#include "lms/common/Validation.hpp"
#include "lms/services/AccessControl.hpp"
#include "lms/services/CourseAccess.hpp"

namespace lms {

ModerationService::ModerationService(ISubmissionRepository& submissions, IMarkRepository& marks,
                                     IAssignmentRepository& assignments,
                                     ICourseRepository& courses, IExtensionRepository& extensions,
                                     IModerationRepository& moderation,
                                     const ILatePenaltyPolicy& penaltyPolicy,
                                     IUnitOfWork& unitOfWork, const IClock& clock)
    : submissions_(submissions),
      marks_(marks),
      assignments_(assignments),
      courses_(courses),
      moderation_(moderation),
      unitOfWork_(unitOfWork),
      clock_(clock),
      rows_(submissions, marks, extensions, penaltyPolicy) {}

ModerationService::Scope ModerationService::scopeFor(const User& hod, Id assignmentId) const {
  AccessControl::require(hod, Permission::ModerateMarks);
  const std::optional<Assignment> a = assignments_.findById(assignmentId);
  const std::optional<Course> course = a ? courses_.findById(a->courseId()) : std::nullopt;
  if (!course || !CourseAccess::isDepartmentHead(hod, *course)) {
    throw NotFoundError("Assignment not found in your department.");
  }
  return Scope{*a, *course};
}

ModerationReview ModerationService::reviewMarks(const User& hod, Id assignmentId) const {
  const Scope scope = scopeFor(hod, assignmentId);
  ModerationReview review{scope.assignment, scope.course, rows_.build(scope.assignment), 0,
                          moderation_.findByAssignment(assignmentId)};
  for (const MarkingRow& r : review.rows) {
    if (r.mark && r.mark->state() == ModerationState::UnderModeration) ++review.awaitingModeration;
  }
  return review;
}

int ModerationService::approveMarks(const User& hod, Id assignmentId, const std::string& commentsIn) {
  const Scope scope = scopeFor(hod, assignmentId);
  const std::string comments = validation::requireText(commentsIn, "Comments", 500, true);

  std::vector<MarkRecord> pending;
  for (const MarkingRow& r : rows_.build(scope.assignment)) {
    if (!r.mark || r.mark->state() != ModerationState::UnderModeration) continue;
    // Completeness check (UC-03 exception flow): a mark must have feedback and a valid score.
    if (r.mark->feedback().empty() || r.mark->finalScore() < 0.0 ||
        r.mark->finalScore() > scope.assignment.maxScore()) {
      throw BusinessRuleError("The marks of " + r.studentName + " are incomplete; approval is not possible.");
    }
    pending.push_back(*r.mark);
  }
  if (pending.empty()) throw BusinessRuleError("No marks are awaiting moderation for this assignment.");

  const Timestamp now = clock_.now();
  auto tx = unitOfWork_.begin();
  ModerationDecision d;
  d.assignmentId = assignmentId;
  d.moderatorId = hod.userId();
  d.decision = DecisionType::Approved;
  d.comments = comments;
  d.decidedAt = now;
  moderation_.insert(d);  // the database only lets marks be released after this row exists
  for (MarkRecord& m : pending) {
    m.release(now);
    marks_.save(m);
  }
  tx->commit();
  Logger::instance().info("Marks of assignment " + std::to_string(assignmentId) + " approved and released (" +
                          std::to_string(pending.size()) + ") by '" + hod.username() + "'");
  return static_cast<int>(pending.size());
}

int ModerationService::rejectMarks(const User& hod, Id assignmentId, const std::string& reasonIn) {
  const Scope scope = scopeFor(hod, assignmentId);
  const std::string reason = validation::requireText(reasonIn, "Reason", 500);

  std::vector<MarkRecord> pending;
  for (const MarkingRow& r : rows_.build(scope.assignment)) {
    if (r.mark && r.mark->state() == ModerationState::UnderModeration) pending.push_back(*r.mark);
  }
  if (pending.empty()) throw BusinessRuleError("No marks are awaiting moderation for this assignment.");

  auto tx = unitOfWork_.begin();
  ModerationDecision d;
  d.assignmentId = assignmentId;
  d.moderatorId = hod.userId();
  d.decision = DecisionType::ChangesRequested;
  d.comments = reason;
  d.decidedAt = clock_.now();
  moderation_.insert(d);
  for (MarkRecord& m : pending) {
    m.setState(ModerationState::ChangesRequested);
    marks_.save(m);
  }
  tx->commit();
  Logger::instance().info("Marks of assignment " + std::to_string(assignmentId) +
                          " returned for correction by '" + hod.username() + "'");
  return static_cast<int>(pending.size());
}

}  // namespace lms
