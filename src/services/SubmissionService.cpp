#include "lms/services/SubmissionService.hpp"

#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"
#include "lms/common/Validation.hpp"
#include "lms/services/AccessControl.hpp"

namespace lms {

SubmissionService::SubmissionService(ISubmissionRepository& submissions,
                                     IAssignmentRepository& assignments,
                                     IEnrolmentRepository& enrolments,
                                     IExtensionRepository& extensions, IMarkRepository& marks,
                                     ILatePenaltyPolicy& penaltyPolicy, const FileStorage& storage,
                                     IUnitOfWork& unitOfWork, const IClock& clock,
                                     SubmissionOptions options)
    : submissions_(submissions),
      assignments_(assignments),
      enrolments_(enrolments),
      extensions_(extensions),
      marks_(marks),
      penaltyPolicy_(penaltyPolicy),
      storage_(storage),
      unitOfWork_(unitOfWork),
      clock_(clock),
      options_(options) {}

Timestamp SubmissionService::applicableDeadlineFor(const User& student, Id assignmentId) const {
  AccessControl::require(student, Permission::SubmitAssignment);
  const std::optional<Assignment> a = assignments_.findById(assignmentId);
  // "Not available to you" is reported for missing and for non-enrolled alike.
  if (!a || !enrolments_.isEnrolled(student.userId(), a->courseId())) {
    throw NotFoundError("Assignment not found.");
  }
  return applicableDeadline(*a, extensions_.find(assignmentId, student.userId()));
}

std::vector<Submission> SubmissionService::history(const User& student, Id assignmentId) const {
  applicableDeadlineFor(student, assignmentId);  // permission + enrolment check
  return submissions_.findAttempts(student.userId(), assignmentId);
}

SubmissionReceipt SubmissionService::resubmit(const User& student, Id submissionId,
                                              const std::filesystem::path& file,
                                              const std::string& comments) {
  AccessControl::require(student, Permission::SubmitAssignment);
  const std::optional<Submission> previous = submissions_.findById(submissionId);
  if (!previous || previous->studentId() != student.userId()) {
    throw NotFoundError("Submission not found.");
  }
  const std::optional<Submission> latest =
      submissions_.findLatest(student.userId(), previous->assignmentId());
  if (!latest || latest->submissionId() != submissionId) {
    throw BusinessRuleError("Only your latest submission can be replaced.");
  }
  return submit(student, previous->assignmentId(), file, comments);
}

SubmissionReceipt SubmissionService::submit(const User& student, Id assignmentId,
                                            const std::filesystem::path& file,
                                            const std::string& commentsIn) {
  AccessControl::require(student, Permission::SubmitAssignment);
  const std::string comments = validation::requireText(commentsIn, "Comments", 1000, true);
  const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
  if (!assignment || !enrolments_.isEnrolled(student.userId(), assignment->courseId())) {
    throw NotFoundError("Assignment not found.");
  }

  const std::optional<Extension> extension = extensions_.find(assignmentId, student.userId());
  const Timestamp deadline = applicableDeadline(*assignment, extension);
  const Timestamp now = clock_.now();
  const int lateDays = wholeDaysLate(deadline, now);

  const std::optional<Submission> previous = submissions_.findLatest(student.userId(), assignmentId);
  if (previous) {
    if (lateDays > 0) {
      throw BusinessRuleError("Resubmission is not permitted after the deadline.");
    }
  } else if (options_.rejectBeyondLatePeriod && lateDays > 0 &&
             penaltyPolicy_.calculatePenalty(lateDays, 100.0) >= 100.0) {  // zero outcome
    throw BusinessRuleError("The late-submission period has ended; this assignment can no longer be submitted.");
  }

  SubmissionReceipt receipt;
  std::string storedPath;
  try {
    auto tx = unitOfWork_.begin();
    const int attempt = submissions_.nextAttemptNo(student.userId(), assignmentId);
    storedPath = storage_.store(assignmentId, student.userId(), attempt, file);
    Submission s(kNoId, assignmentId, student.userId(), attempt, now, storedPath, comments);
    submissions_.save(s);
    receipt.submissionId = s.submissionId();
    receipt.attemptNo = attempt;
    tx->commit();
  } catch (...) {
    if (!storedPath.empty()) storage_.remove(storedPath);  // never keep a file with no record
    throw;
  }
  receipt.isResubmission = previous.has_value();
  receipt.submittedAt = now;
  receipt.applicableDeadline = deadline;
  receipt.extensionApplied = extension && extension->newDeadline > assignment->deadline();
  receipt.lateDays = lateDays;
  receipt.penaltyPercent = penaltyPolicy_.calculatePenalty(lateDays, 100.0);
  Logger::instance().info("Submission: student '" + student.username() + "' assignment " +
                          std::to_string(assignmentId) + " attempt " +
                          std::to_string(receipt.attemptNo) + " late days " +
                          std::to_string(lateDays));
  return receipt;
}

}  // namespace lms
