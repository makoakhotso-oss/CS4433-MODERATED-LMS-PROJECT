#include <map>

#include "lms/services/MarkingTypes.hpp"

namespace lms {

SubmissionStatus MarkingRowBuilder::deriveStatus(int lateDays,
                                                 const std::optional<MarkRecord>& mark) {
  if (!mark) return lateDays > 0 ? SubmissionStatus::Late : SubmissionStatus::Submitted;
  switch (mark->state()) {
    case ModerationState::Marked: return SubmissionStatus::Marked;
    case ModerationState::UnderModeration: return SubmissionStatus::UnderModeration;
    case ModerationState::ChangesRequested: return SubmissionStatus::ChangesRequested;
    case ModerationState::Released: return SubmissionStatus::Released;
  }
  return SubmissionStatus::Submitted;
}

std::vector<MarkingRow> MarkingRowBuilder::build(const Assignment& assignment) const {
  std::map<Id, MarkRecord> marksBySubmission;
  for (const MarkRecord& m : marks_.findByAssignment(assignment.assignmentId())) {
    marksBySubmission.emplace(m.submissionId(), m);
  }
  std::map<Id, Extension> extensionsByStudent;
  for (const Extension& e : extensions_.findByAssignment(assignment.assignmentId())) {
    extensionsByStudent.emplace(e.studentId, e);
  }

  std::vector<MarkingRow> rows;
  for (const SubmissionWithStudent& item : submissions_.findCurrentByAssignment(assignment.assignmentId())) {
    MarkingRow row{item.submission, item.studentName, item.studentUsername, 0, 0, 0.0, std::nullopt,
                   SubmissionStatus::Submitted};
    std::optional<Extension> ext;
    if (const auto it = extensionsByStudent.find(item.submission.studentId());
        it != extensionsByStudent.end()) {
      ext = it->second;
    }
    row.applicableDeadline = applicableDeadline(assignment, ext);
    row.lateDays = item.submission.calculateLateDays(row.applicableDeadline);
    row.penaltyPercent = policy_.calculatePenalty(row.lateDays, 100.0);
    if (const auto it = marksBySubmission.find(item.submission.submissionId());
        it != marksBySubmission.end()) {
      row.mark = it->second;
    }
    row.status = deriveStatus(row.lateDays, row.mark);
    row.submission.setLateInfo(row.lateDays, ext && ext->newDeadline > assignment.deadline());
    row.submission.updateStatus(row.status);
    rows.push_back(std::move(row));
  }
  return rows;
}

}  // namespace lms
