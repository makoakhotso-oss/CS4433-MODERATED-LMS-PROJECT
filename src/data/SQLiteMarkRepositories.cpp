#include "lms/common/Time.hpp"
#include "lms/data/SQLiteRepositories.hpp"

namespace lms {

namespace {

// "Current" = the highest attempt of each student for each assignment.
const char* const kIsLatestAttempt =
    "s.attempt_no = (SELECT MAX(s2.attempt_no) FROM submissions s2 "
    "WHERE s2.assignment_id = s.assignment_id AND s2.student_id = s.student_id)";

// Mark columns with table alias "m" (7 columns, indexes 0..6).
const char* const kMarkColumns =
    "m.mark_id, m.submission_id, m.raw_score, m.final_score, m.feedback, m.moderation_state, "
    "m.released_at";

MarkRecord readMark(const Statement& st) {
  std::optional<Timestamp> released;
  if (!st.columnIsNull(6)) released = fromDbString(st.columnText(6));
  return MarkRecord(st.columnInt(0), st.columnInt(1), st.columnDouble(2), st.columnDouble(3),
                    st.columnText(4), moderationStateFromString(st.columnText(5)), released);
}

AuditEntry readAudit(const Statement& st) {
  AuditEntry e;
  e.id = st.columnInt(0);
  e.markId = st.columnInt(1);
  e.changedBy = st.columnInt(2);
  if (!st.columnIsNull(3)) e.previousScore = st.columnDouble(3);
  e.newScore = st.columnDouble(4);
  e.changedAt = fromDbString(st.columnText(5));
  e.reason = st.columnText(6);
  return e;
}

// Audit columns (0..6) followed by: changed-by name (7), student name (8), assignment id (9).
const char* const kAuditSelect =
    "SELECT a.audit_id, a.mark_id, a.changed_by, a.previous_score, a.new_score, a.changed_at, "
    "a.reason, cu.full_name, su.full_name, s.assignment_id "
    "FROM mark_audit_log a "
    "JOIN marks m ON m.mark_id = a.mark_id "
    "JOIN submissions s ON s.submission_id = m.submission_id "
    "JOIN users cu ON cu.user_id = a.changed_by "
    "JOIN users su ON su.user_id = s.student_id ";

AuditRow readAuditRow(const Statement& st) {
  return AuditRow{readAudit(st), st.columnText(7), st.columnText(8), st.columnInt(9)};
}

}  // namespace

// ----------------------------------------------------------------------- marks

bool SQLiteMarkRepository::save(MarkRecord& m) {
  if (m.markId() == kNoId) {
    Statement st(db_,
                 "INSERT INTO marks (submission_id, raw_score, final_score, feedback, "
                 "moderation_state, released_at) VALUES (?, ?, ?, ?, ?, ?)");
    st.bindInt(1, m.submissionId())
        .bindDouble(2, m.rawScore())
        .bindDouble(3, m.finalScore())
        .bindText(4, m.feedback())
        .bindText(5, toString(m.state()));
    if (m.releasedAt()) {
      st.bindText(6, toDbString(*m.releasedAt()));
    } else {
      st.bindNull(6);
    }
    st.run();
    m.setId(db_.lastInsertRowId());
    return true;
  }
  Statement st(db_,
               "UPDATE marks SET raw_score = ?, final_score = ?, feedback = ?, "
               "moderation_state = ?, released_at = ? WHERE mark_id = ?");
  st.bindDouble(1, m.rawScore())
      .bindDouble(2, m.finalScore())
      .bindText(3, m.feedback())
      .bindText(4, toString(m.state()));
  if (m.releasedAt()) {
    st.bindText(5, toDbString(*m.releasedAt()));
  } else {
    st.bindNull(5);
  }
  st.bindInt(6, m.markId());
  st.run();
  return true;
}

std::optional<MarkRecord> SQLiteMarkRepository::findById(Id markId) const {
  Statement st(db_, std::string("SELECT ") + kMarkColumns + " FROM marks m WHERE m.mark_id = ?");
  st.bindInt(1, markId);
  if (!st.step()) return std::nullopt;
  return readMark(st);
}

std::optional<MarkRecord> SQLiteMarkRepository::findBySubmission(Id submissionId) const {
  Statement st(db_, std::string("SELECT ") + kMarkColumns +
                        " FROM marks m WHERE m.submission_id = ?");
  st.bindInt(1, submissionId);
  if (!st.step()) return std::nullopt;
  return readMark(st);
}

std::vector<MarkRecord> SQLiteMarkRepository::findByAssignment(Id assignmentId) const {
  Statement st(db_, std::string("SELECT ") + kMarkColumns +
                        " FROM marks m JOIN submissions s ON s.submission_id = m.submission_id "
                        "WHERE s.assignment_id = ? ORDER BY m.mark_id");
  st.bindInt(1, assignmentId);
  std::vector<MarkRecord> out;
  while (st.step()) out.push_back(readMark(st));
  return out;
}

std::vector<ReleasedMarkRow> SQLiteMarkRepository::findReleasedByStudent(Id studentId) const {
  // Columns: mark (0..6), submission (7..13), assignment (14..20), course code/name (21, 22).
  Statement st(db_,
               std::string("SELECT ") + kMarkColumns +
                   ", s.submission_id, s.assignment_id, s.student_id, s.attempt_no, s.submitted_at, "
                   "s.file_path, s.comments, a.assignment_id, a.course_id, a.created_by, a.title, a.description, "
                   "a.deadline, a.max_score, c.course_code, c.course_name "
                   "FROM marks m "
                   "JOIN submissions s ON s.submission_id = m.submission_id "
                   "JOIN assignments a ON a.assignment_id = s.assignment_id "
                   "JOIN courses c ON c.course_id = a.course_id "
                   "WHERE s.student_id = ? AND m.moderation_state = 'Released' "
                   "AND m.released_at IS NOT NULL AND " + kIsLatestAttempt +
                   " ORDER BY c.course_code, a.deadline, a.assignment_id");
  st.bindInt(1, studentId);
  std::vector<ReleasedMarkRow> out;
  while (st.step()) {
    MarkRecord mark = readMark(st);
    Submission sub(st.columnInt(7), st.columnInt(8), st.columnInt(9),
                   static_cast<int>(st.columnInt(10)), fromDbString(st.columnText(11)),
                   st.columnText(12), st.columnText(13));
    Assignment asg(st.columnInt(14), st.columnInt(15), st.columnInt(16), st.columnText(17),
                   st.columnText(18), fromDbString(st.columnText(19)), st.columnDouble(20));
    out.push_back(ReleasedMarkRow{std::move(mark), std::move(sub), std::move(asg),
                                  st.columnText(21), st.columnText(22)});
  }
  return out;
}

// ------------------------------------------------------------------ moderation

Id SQLiteModerationRepository::insert(const ModerationDecision& d) {
  Statement st(db_,
               "INSERT INTO moderation_decisions (assignment_id, moderator_id, decision, comments, "
               "decided_at) VALUES (?, ?, ?, ?, ?)");
  st.bindInt(1, d.assignmentId)
      .bindInt(2, d.moderatorId)
      .bindText(3, toString(d.decision))
      .bindText(4, d.comments)
      .bindText(5, toDbString(d.decidedAt));
  st.run();
  return db_.lastInsertRowId();
}

std::vector<ModerationDecision> SQLiteModerationRepository::findByAssignment(
    Id assignmentId) const {
  Statement st(db_,
               "SELECT decision_id, assignment_id, moderator_id, decision, comments, decided_at "
               "FROM moderation_decisions WHERE assignment_id = ? ORDER BY decision_id");
  st.bindInt(1, assignmentId);
  std::vector<ModerationDecision> out;
  while (st.step()) {
    ModerationDecision d;
    d.id = st.columnInt(0);
    d.assignmentId = st.columnInt(1);
    d.moderatorId = st.columnInt(2);
    d.decision = decisionFromString(st.columnText(3));
    d.comments = st.columnText(4);
    d.decidedAt = fromDbString(st.columnText(5));
    out.push_back(std::move(d));
  }
  return out;
}

// ----------------------------------------------------------------------- audit

Id SQLiteAuditRepository::append(const AuditEntry& e) {
  Statement st(db_,
               "INSERT INTO mark_audit_log (mark_id, changed_by, previous_score, new_score, "
               "changed_at, reason) VALUES (?, ?, ?, ?, ?, ?)");
  st.bindInt(1, e.markId).bindInt(2, e.changedBy);
  if (e.previousScore) {
    st.bindDouble(3, *e.previousScore);
  } else {
    st.bindNull(3);
  }
  st.bindDouble(4, e.newScore).bindText(5, toDbString(e.changedAt)).bindText(6, e.reason);
  st.run();
  return db_.lastInsertRowId();
}

std::vector<AuditRow> SQLiteAuditRepository::findByMark(Id markId) const {
  Statement st(db_, std::string(kAuditSelect) + "WHERE a.mark_id = ? ORDER BY a.audit_id");
  st.bindInt(1, markId);
  std::vector<AuditRow> out;
  while (st.step()) out.push_back(readAuditRow(st));
  return out;
}

std::vector<AuditRow> SQLiteAuditRepository::findByAssignment(Id assignmentId) const {
  Statement st(db_, std::string(kAuditSelect) +
                        "WHERE s.assignment_id = ? ORDER BY a.audit_id");
  st.bindInt(1, assignmentId);
  std::vector<AuditRow> out;
  while (st.step()) out.push_back(readAuditRow(st));
  return out;
}

}  // namespace lms
