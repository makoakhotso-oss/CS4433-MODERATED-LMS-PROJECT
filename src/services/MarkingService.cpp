#include "lms/services/MarkingService.hpp"

#include <algorithm>

#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"
#include "lms/common/Validation.hpp"
#include "lms/services/AccessControl.hpp"
#include "lms/services/CourseAccess.hpp"

namespace lms {

MarkingService::MarkingService(ISubmissionRepository& submissions, IMarkRepository& marks,
                               IAssignmentRepository& assignments, ICourseRepository& courses,
                               IExtensionRepository& extensions,
                               const ILatePenaltyPolicy& penaltyPolicy, AuditLog& auditLog,
                               IUnitOfWork& unitOfWork, const IClock& clock)
    : submissions_(submissions),
      marks_(marks),
      assignments_(assignments),
      courses_(courses),
      extensions_(extensions),
      policy_(penaltyPolicy),
      auditLog_(auditLog),
      unitOfWork_(unitOfWork),
      clock_(clock),
      rows_(submissions, marks, extensions, penaltyPolicy) {}

Assignment MarkingService::assignmentFor(const User& lecturer, Id assignmentId) const {
  const std::optional<Assignment> a = assignments_.findById(assignmentId);
  const std::optional<Course> course = a ? courses_.findById(a->courseId()) : std::nullopt;
  // Not found and not yours look the same to the caller.
  if (!course || !CourseAccess::isCourseLecturer(lecturer, *course)) {
    throw NotFoundError("Assignment not found.");
  }
  return *a;
}

MarkingService::Context MarkingService::contextFor(const User& lecturer, Id submissionId) const {
  const std::optional<Submission> s = submissions_.findById(submissionId);
  if (!s) throw NotFoundError("Submission not found.");
  const Assignment a = assignmentFor(lecturer, s->assignmentId());
  const std::optional<Course> course = courses_.findById(a.courseId());
  return Context{*s, a, *course};
}

std::vector<MarkingRow> MarkingService::listSubmissions(const User& lecturer,
                                                        Id assignmentId) const {
  AccessControl::require(lecturer, Permission::MarkSubmissions);
  return rows_.build(assignmentFor(lecturer, assignmentId));
}

MarkRecord MarkingService::recordMark(const User& lecturer, Id submissionId, double rawScore,
                                      const std::string& feedbackIn, const std::string& reasonIn) {
  AccessControl::require(lecturer, Permission::MarkSubmissions);
  const Context ctx = contextFor(lecturer, submissionId);
  const double raw = validation::requireScore(rawScore, ctx.assignment.maxScore(), "Mark");
  const std::string feedback = validation::requireText(feedbackIn, "Feedback", 2000);
  const std::string reason = validation::requireText(reasonIn, "Reason", 500, true);

  const std::optional<Submission> latest =
      submissions_.findLatest(ctx.submission.studentId(), ctx.submission.assignmentId());
  if (!latest || latest->submissionId() != submissionId) {
    throw BusinessRuleError("Only the student's current (latest) submission can be marked.");
  }

  const std::optional<Extension> ext =
      extensions_.find(ctx.assignment.assignmentId(), ctx.submission.studentId());
  const int lateDays = ctx.submission.calculateLateDays(applicableDeadline(ctx.assignment, ext));
  const double penalty = policy_.calculatePenalty(lateDays, ctx.assignment.maxScore());
  const double finalScore = validation::roundMark(std::max(0.0, raw - penalty));

  std::optional<MarkRecord> existing = marks_.findBySubmission(submissionId);
  const Timestamp now = clock_.now();
  auto tx = unitOfWork_.begin();
  MarkRecord mark(kNoId, submissionId, raw, finalScore, feedback, ModerationState::Marked,
                  std::nullopt);
  if (!existing) {
    marks_.save(mark);
    const Id id = mark.markId();
    auditLog_.recordChange(id, lecturer.userId(), std::nullopt, finalScore,
                           reason.empty() ? "Initial mark" : reason, now);
  } else {
    if (reason.empty()) throw ValidationError("A reason is required when changing an existing mark.");
    if (existing->rawScore() == raw && existing->feedback() == feedback) {
      throw BusinessRuleError("Nothing has changed.");
    }
    const double previous = existing->finalScore();
    existing->updateMark(raw, finalScore, feedback);
    // Anything already sent to the HoD, or released, must be moderated again.
    if (existing->state() == ModerationState::UnderModeration ||
        existing->state() == ModerationState::Released) {
      existing->setState(ModerationState::Marked);
    }
    marks_.save(*existing);
    auditLog_.recordChange(existing->markId(), lecturer.userId(), previous, finalScore, reason, now);
    mark = *existing;
  }
  tx->commit();
  return mark;
}

int MarkingService::submitForModeration(const User& lecturer, Id assignmentId) {
  AccessControl::require(lecturer, Permission::SubmitForModeration);
  const Assignment assignment = assignmentFor(lecturer, assignmentId);
  const std::vector<MarkingRow> rows = rows_.build(assignment);
  if (rows.empty()) throw BusinessRuleError("There are no submissions for this assignment.");

  int unmarked = 0;
  for (const MarkingRow& r : rows) {
    if (!r.mark) ++unmarked;
  }
  if (unmarked > 0) {
    throw BusinessRuleError(std::to_string(unmarked) +
                            " submission(s) have not been marked yet. Mark all work first.");
  }

  auto tx = unitOfWork_.begin();
  int sent = 0;
  for (const MarkingRow& r : rows) {
    if (r.mark->state() == ModerationState::Marked ||
        r.mark->state() == ModerationState::ChangesRequested) {
      MarkRecord m = *r.mark;
      m.setState(ModerationState::UnderModeration);
      marks_.save(m);
      ++sent;
    }
  }
  if (sent == 0) {
    throw BusinessRuleError("No marks are waiting to be sent: they are already under moderation or released.");
  }
  tx->commit();
  Logger::instance().info("Marks of assignment " + std::to_string(assignmentId) + " (" +
                          std::to_string(sent) + ") sent for moderation by '" + lecturer.username() + "'");
  return sent;
}

}  // namespace lms
