#include "lms/common/Time.hpp"
#include "lms/data/SQLiteRepositories.hpp"

namespace lms {

namespace {

// Columns of a submission row, in this order, with the table alias "s".
const char* const kSubmissionColumns =
    "s.submission_id, s.assignment_id, s.student_id, s.attempt_no, s.submitted_at, s.file_path, "
    "s.comments";

Submission readSubmission(const Statement& st) {
  return Submission(st.columnInt(0), st.columnInt(1), st.columnInt(2),
                    static_cast<int>(st.columnInt(3)), fromDbString(st.columnText(4)),
                    st.columnText(5), st.columnText(6));
}

// "Current" = the highest attempt of each student for each assignment.
const char* const kIsLatestAttempt =
    "s.attempt_no = (SELECT MAX(s2.attempt_no) FROM submissions s2 "
    "WHERE s2.assignment_id = s.assignment_id AND s2.student_id = s.student_id)";

}  // namespace

bool SQLiteSubmissionRepository::save(Submission& s) {
  Statement st(db_,
               "INSERT INTO submissions (assignment_id, student_id, attempt_no, submitted_at, "
               "file_path, comments) VALUES (?, ?, ?, ?, ?, ?)");
  st.bindInt(1, s.assignmentId())
      .bindInt(2, s.studentId())
      .bindInt(3, s.attemptNo())
      .bindText(4, toDbString(s.submittedAt()))
      .bindText(5, s.getFilePath())
      .bindText(6, s.comments());
  st.run();
  s.setId(db_.lastInsertRowId());
  return true;
}

std::optional<Submission> SQLiteSubmissionRepository::findById(Id id) const {
  Statement st(db_, std::string("SELECT ") + kSubmissionColumns +
                        " FROM submissions s WHERE s.submission_id = ?");
  st.bindInt(1, id);
  if (!st.step()) return std::nullopt;
  return readSubmission(st);
}

std::vector<Submission> SQLiteSubmissionRepository::findByAssignment(Id assignmentId) const {
  Statement st(db_, std::string("SELECT ") + kSubmissionColumns +
                        " FROM submissions s WHERE s.assignment_id = ? "
                        "ORDER BY s.student_id, s.attempt_no");
  st.bindInt(1, assignmentId);
  std::vector<Submission> out;
  while (st.step()) out.push_back(readSubmission(st));
  return out;
}

std::vector<Submission> SQLiteSubmissionRepository::findAttempts(Id studentId,
                                                                 Id assignmentId) const {
  Statement st(db_, std::string("SELECT ") + kSubmissionColumns +
                        " FROM submissions s WHERE s.student_id = ? AND s.assignment_id = ? "
                        "ORDER BY s.attempt_no");
  st.bindInt(1, studentId).bindInt(2, assignmentId);
  std::vector<Submission> out;
  while (st.step()) out.push_back(readSubmission(st));
  return out;
}

std::optional<Submission> SQLiteSubmissionRepository::findLatest(Id studentId,
                                                                 Id assignmentId) const {
  Statement st(db_, std::string("SELECT ") + kSubmissionColumns +
                        " FROM submissions s WHERE s.student_id = ? AND s.assignment_id = ? "
                        "ORDER BY s.attempt_no DESC LIMIT 1");
  st.bindInt(1, studentId).bindInt(2, assignmentId);
  if (!st.step()) return std::nullopt;
  return readSubmission(st);
}

std::vector<SubmissionWithStudent> SQLiteSubmissionRepository::findCurrentByAssignment(
    Id assignmentId) const {
  Statement st(db_, std::string("SELECT ") + kSubmissionColumns +
                        ", u.full_name, u.username FROM submissions s "
                        "JOIN users u ON u.user_id = s.student_id "
                        "WHERE s.assignment_id = ? AND " + kIsLatestAttempt +
                        " ORDER BY u.full_name, u.user_id");
  st.bindInt(1, assignmentId);
  std::vector<SubmissionWithStudent> out;
  while (st.step()) {
    out.push_back(SubmissionWithStudent{readSubmission(st), st.columnText(7), st.columnText(8)});
  }
  return out;
}

std::vector<Submission> SQLiteSubmissionRepository::findCurrentByStudent(Id studentId) const {
  Statement st(db_, std::string("SELECT ") + kSubmissionColumns +
                        " FROM submissions s WHERE s.student_id = ? AND " + kIsLatestAttempt +
                        " ORDER BY s.assignment_id");
  st.bindInt(1, studentId);
  std::vector<Submission> out;
  while (st.step()) out.push_back(readSubmission(st));
  return out;
}

int SQLiteSubmissionRepository::nextAttemptNo(Id studentId, Id assignmentId) const {
  Statement st(db_,
               "SELECT COALESCE(MAX(attempt_no), 0) + 1 FROM submissions "
               "WHERE student_id = ? AND assignment_id = ?");
  st.bindInt(1, studentId).bindInt(2, assignmentId);
  return st.step() ? static_cast<int>(st.columnInt(0)) : 1;
}

}  // namespace lms
