#include <iomanip>
#include <sstream>

#include "lms/common/Errors.hpp"
#include "lms/common/Time.hpp"
#include "lms/ui/ConsoleApp.hpp"

namespace lms {

namespace {

std::string num(double v) {
  std::ostringstream os;
  os << std::fixed << std::setprecision(2) << v;
  return os.str();
}

std::string yesNo(bool b) { return b ? "yes" : "no"; }

std::optional<Role> askRole(Console& c) {
  c.print("  1) Administrator   2) Lecturer   3) Head of Department   4) Student");
  const auto n = c.askChoice("  Role number: ");
  if (!n || *n < 1 || *n > 4) {
    c.printError("Please choose a number from 1 to 4.");
    return std::nullopt;
  }
  static const Role roles[] = {Role::Administrator, Role::Lecturer, Role::HeadOfDepartment,
                               Role::Student};
  return roles[*n - 1];
}

}  // namespace

// ---------------------------------------------------------------------- menus

void ConsoleApp::administratorMenu(User& u) {
  runMenu(u, "Administrator - " + u.fullName(),
          {{"Create user", [&] { createUser(u); }},
           {"List users", [&] { listUsers(u); }},
           {"Update user (name, role, department)", [&] { updateUser(u); }},
           {"Activate / deactivate user", [&] { toggleActive(u); }},
           {"Reset a user's password", [&] { resetPassword(u); }},
           {"Create department", [&] { createDepartment(u); }},
           {"List departments", [&] { listDepartments(u); }},
           {"Create course", [&] { adminCreateCourse(u); }},
           {"List courses", [&] { listCourses(u, false); }},
           {"Assign lecturer to course", [&] { assignLecturer(u); }},
           {"Enrol student in course", [&] { enrol(u, true); }},
           {"Remove student from course", [&] { enrol(u, false); }},
           {"List students of a course", [&] { listEnrolled(u); }},
           {"Change my password", [&] { changePassword(u); }}});
}

void ConsoleApp::lecturerMenu(User& u) {
  runMenu(u, "Lecturer - " + u.fullName(),
          {{"My courses and assignments", [&] { listCourses(u, true); }},
           {"Create course", [&] { lecturerCreateCourse(u); }},
           {"Rename course", [&] { renameCourse(u); }},
           {"Students of a course", [&] { listEnrolled(u); }},
           {"Create assignment", [&] { createAssignment(u); }},
           {"Grant an individual extension", [&] { grantExtension(u); }},
           {"View submissions and mark", [&] { markWork(u); }},
           {"Send marks for moderation", [&] { submitForModeration(u); }},
           {"Mark change history", [&] { showAudit(u); }},
           {"Change my password", [&] { changePassword(u); }}});
}

void ConsoleApp::headOfDepartmentMenu(User& u) {
  runMenu(u, "Head of Department - " + u.fullName(),
          {{"Department courses and assignments", [&] { listCourses(u, true); }},
           {"Review marks of an assignment", [&] { reviewMarks(u); }},
           {"Approve and release marks", [&] { approveMarks(u); }},
           {"Return marks for correction", [&] { rejectMarks(u); }},
           {"Mark change history", [&] { showAudit(u); }},
           {"Change my password", [&] { changePassword(u); }}});
}

void ConsoleApp::studentMenu(User& u) {
  runMenu(u, "Student - " + u.fullName(),
          {{"My courses", [&] { listCourses(u, false); }},
           {"My assignments", [&] { listStudentAssignments(u); }},
           {"Submit or resubmit an assignment", [&] { submitAssignment(u); }},
           {"My submissions for an assignment", [&] { showHistory(u); }},
           {"My released marks and feedback", [&] { showReleasedMarks(u); }},
           {"Change my password", [&] { changePassword(u); }}});
}

void ConsoleApp::changePassword(const User& u) {
  const std::string oldPw = c_.askPassword("  Current password: ");
  const auto newPw = c_.askNewPassword();
  if (!newPw) return;
  app_.auth().changePassword(u, oldPw, *newPw);
  c_.print("  Password changed.");
}

// -------------------------------------------------------------- administrator

void ConsoleApp::createUser(const User& admin) {
  NewUserRequest req;
  req.fullName = c_.ask("  Full name: ");
  req.username = c_.ask("  Username: ");
  const auto role = askRole(c_);
  if (!role) return;
  req.role = *role;
  if (*role == Role::Lecturer || *role == Role::HeadOfDepartment) {
    listDepartments(admin);
    req.departmentId = c_.askId("  Department id: ");
    if (!req.departmentId) return;
  }
  const auto pw = c_.askNewPassword();
  if (!pw) return;
  req.password = *pw;
  const Id id = app_.users().createUser(admin, req);
  c_.print("  User created with id " + std::to_string(id) + ".");
}

void ConsoleApp::listUsers(const User& admin) {
  std::vector<std::vector<std::string>> rows;
  for (const auto& u : app_.users().listUsers(admin)) {
    rows.push_back({std::to_string(u->userId()), u->username(), u->fullName(), u->getRole(),
                    u->departmentId() ? std::to_string(*u->departmentId()) : "-",
                    yesNo(u->isActive())});
  }
  c_.printTable({"Id", "Username", "Name", "Role", "Dept", "Active"}, rows);
}

void ConsoleApp::updateUser(const User& admin) {
  const auto id = c_.askId("  User id: ");
  if (!id) return;
  const std::unique_ptr<User> target = app_.userRepository().findById(*id);
  if (!target) throw NotFoundError("User not found.");
  std::string name = c_.ask("  Full name [" + target->fullName() + "]: ");
  if (name.empty()) name = target->fullName();
  Role role = target->role();
  if (c_.confirm("  Change the role (currently " + target->getRole() + ")?")) {
    const auto r = askRole(c_);
    if (!r) return;
    role = *r;
  }
  std::optional<Id> dept = target->departmentId();
  if (role == Role::Lecturer || role == Role::HeadOfDepartment) {
    listDepartments(admin);
    const auto d = c_.askId("  Department id" + (dept ? " [" + std::to_string(*dept) + "]" : "") + ": ");
    if (d) dept = d;
  }
  app_.users().updateUser(admin, *id, name, role, dept);
  c_.print("  User updated.");
}

void ConsoleApp::toggleActive(const User& admin) {
  const auto id = c_.askId("  User id: ");
  if (!id) return;
  const std::unique_ptr<User> target = app_.userRepository().findById(*id);
  if (!target) throw NotFoundError("User not found.");
  const bool newState = !target->isActive();
  if (!c_.confirm(std::string("  ") + (newState ? "Activate " : "Deactivate ") + target->username() + "?")) return;
  app_.users().setActive(admin, *id, newState);
  c_.print(newState ? "  Account activated." : "  Account deactivated.");
}

void ConsoleApp::resetPassword(const User& admin) {
  const auto id = c_.askId("  User id: ");
  if (!id) return;
  const auto pw = c_.askNewPassword();
  if (!pw) return;
  app_.users().resetPassword(admin, *id, *pw);
  c_.print("  Password reset.");
}

void ConsoleApp::createDepartment(const User& admin) {
  const Id id = app_.users().createDepartment(admin, c_.ask("  Department name: "));
  c_.print("  Department created with id " + std::to_string(id) + ".");
}

void ConsoleApp::listDepartments(const User& admin) {
  std::vector<std::vector<std::string>> rows;
  for (const Department& d : app_.users().listDepartments(admin)) {
    rows.push_back({std::to_string(d.id), d.name});
  }
  c_.printTable({"Id", "Department"}, rows);
}

void ConsoleApp::adminCreateCourse(const User& admin) {
  c_.print("  Lecturers:");
  std::vector<std::vector<std::string>> rows;
  for (const auto& u : app_.users().listUsers(admin)) {
    if (u->role() == Role::Lecturer && u->isActive()) {
      rows.push_back({std::to_string(u->userId()), u->fullName(),
                      u->departmentId() ? std::to_string(*u->departmentId()) : "-"});
    }
  }
  c_.printTable({"Id", "Name", "Dept"}, rows);
  const std::string code = c_.ask("  Course code: ");
  const std::string name = c_.ask("  Course name: ");
  const auto lecturer = c_.askId("  Lecturer id: ");
  if (!lecturer) return;
  const Course course = app_.courses().createCourse(admin, code, name, lecturer);
  c_.print("  Course created with id " + std::to_string(course.courseId()) + ".");
}

void ConsoleApp::listCourses(const User& user, bool withAssignments) {
  const std::vector<Course> courses = app_.courses().listCourses(user);
  if (!withAssignments) {
    std::vector<std::vector<std::string>> rows;
    for (const Course& c : courses) {
      rows.push_back({std::to_string(c.courseId()), c.courseCode(), c.courseName(),
                      "lecturer " + std::to_string(c.lecturerId())});
    }
    c_.printTable({"Id", "Code", "Name", "Taught by"}, rows);
    return;
  }
  if (courses.empty()) c_.print("  (no courses)");
  for (const Course& course : courses) {
    c_.print("  " + course.courseCode() + " - " + course.courseName() + " (course id " +
             std::to_string(course.courseId()) + ")");
    std::vector<std::vector<std::string>> rows;
    for (const Assignment& a : app_.assignments().listAssignments(user, course.courseId())) {
      rows.push_back({std::to_string(a.assignmentId()), a.title(), toLocalString(a.deadline()),
                      num(a.maxScore())});
    }
    c_.printTable({"Id", "Assignment", "Deadline", "Max mark"}, rows);
  }
}

void ConsoleApp::assignLecturer(const User& admin) {
  const auto course = c_.askId("  Course id: ");
  if (!course) return;
  const auto lecturer = c_.askId("  New lecturer id: ");
  if (!lecturer) return;
  app_.courses().assignLecturer(admin, *course, *lecturer);
  c_.print("  Lecturer assigned.");
}

void ConsoleApp::enrol(const User& admin, bool enrolStudent) {
  const auto course = c_.askId("  Course id: ");
  if (!course) return;
  const auto student = c_.askId("  Student id: ");
  if (!student) return;
  if (enrolStudent) {
    app_.courses().enrolStudent(admin, *course, *student);
    c_.print("  Student enrolled.");
  } else {
    app_.courses().unenrolStudent(admin, *course, *student);
    c_.print("  Student removed from the course.");
  }
}

void ConsoleApp::listEnrolled(const User& user) {
  const auto course = c_.askId("  Course id: ");
  if (!course) return;
  std::vector<std::vector<std::string>> rows;
  for (const UserSummary& s : app_.courses().listEnrolledStudents(user, *course)) {
    rows.push_back({std::to_string(s.id), s.username, s.fullName});
  }
  c_.printTable({"Id", "Username", "Name"}, rows);
}

// ------------------------------------------------------------------- lecturer

void ConsoleApp::lecturerCreateCourse(const User& lecturer) {
  const std::string code = c_.ask("  Course code: ");
  const std::string name = c_.ask("  Course name: ");
  const Course course = app_.courses().createCourse(lecturer, code, name);
  c_.print("  Course created with id " + std::to_string(course.courseId()) + ".");
}

void ConsoleApp::renameCourse(const User& lecturer) {
  const auto id = c_.askId("  Course id: ");
  if (!id) return;
  app_.courses().renameCourse(lecturer, *id, c_.ask("  New course name: "));
  c_.print("  Course renamed.");
}

void ConsoleApp::createAssignment(const User& lecturer) {
  const auto course = c_.askId("  Course id: ");
  if (!course) return;
  const std::string title = c_.ask("  Title: ");
  const std::string description = c_.ask("  Description: ");
  const auto deadline = c_.askDateTime("  Deadline");
  if (!deadline) return;
  const auto max = c_.askNumber("  Maximum mark: ");
  if (!max) return;
  const Assignment a = app_.assignments().createAssignment(lecturer, *course, title, description, *deadline, *max);
  c_.print("  Assignment created with id " + std::to_string(a.assignmentId()) + ".");
}

void ConsoleApp::grantExtension(const User& lecturer) {
  const auto id = c_.askId("  Assignment id: ");
  if (!id) return;
  const Assignment a = app_.assignments().getAssignment(lecturer, *id);
  c_.print("  Current deadline: " + toLocalString(a.deadline()));
  std::vector<std::vector<std::string>> rows;
  for (const UserSummary& s : app_.courses().listEnrolledStudents(lecturer, a.courseId())) {
    rows.push_back({std::to_string(s.id), s.username, s.fullName});
  }
  c_.printTable({"Id", "Username", "Name"}, rows);
  const auto student = c_.askId("  Student id: ");
  if (!student) return;
  const auto deadline = c_.askDateTime("  New deadline for this student");
  if (!deadline) return;
  const std::string reason = c_.ask("  Reason: ");
  app_.assignments().grantExtension(lecturer, *id, *student, *deadline, reason);
  c_.print("  Extension granted to that student only.");
}

void ConsoleApp::printMarkingRows(const std::vector<MarkingRow>& rows) {
  std::vector<std::vector<std::string>> out;
  for (const MarkingRow& r : rows) {
    out.push_back({std::to_string(r.submission.submissionId()), r.studentName,
                   std::to_string(r.submission.attemptNo()), toLocalString(r.submission.submittedAt()),
                   r.lateDays > 0 ? std::to_string(r.lateDays) + " (-" + num(r.penaltyPercent) + "%)" : "-",
                   toString(r.status),
                   r.mark ? num(r.mark->rawScore()) + " -> " + num(r.mark->finalScore()) : "-"});
  }
  c_.printTable({"Sub id", "Student", "Try", "Submitted", "Days late", "Status", "Mark (raw -> final)"}, out);
}

void ConsoleApp::markWork(const User& lecturer) {
  const auto assignmentId = c_.askId("  Assignment id: ");
  if (!assignmentId) return;
  for (;;) {
    const Assignment a = app_.assignments().getAssignment(lecturer, *assignmentId);
    printMarkingRows(app_.marking().listSubmissions(lecturer, *assignmentId));
    c_.print("  Maximum mark: " + num(a.maxScore()) + ". Late penalty is applied automatically.");
    const auto sub = c_.askId("  Submission id to mark (blank to finish): ");
    if (!sub) return;
    guarded([&] {
      for (const MarkingRow& row : app_.marking().listSubmissions(lecturer, *assignmentId)) {
        if (row.submission.submissionId() == *sub && !row.submission.comments().empty()) {
          c_.print("  Student's comments: " + row.submission.comments());
        }
      }
      const auto raw = c_.askNumber("  Mark: ");
      if (!raw) return;
      const std::string feedback = c_.ask("  Feedback: ");
      const std::string reason = c_.ask("  Reason (required when changing an existing mark): ");
      const MarkRecord m = app_.marking().recordMark(lecturer, *sub, *raw, feedback, reason);
      c_.print("  Saved: mark " + num(m.rawScore()) + ", after late penalty " + num(m.finalScore()) + ".");
    });
  }
}

void ConsoleApp::submitForModeration(const User& lecturer) {
  const auto id = c_.askId("  Assignment id: ");
  if (!id) return;
  printMarkingRows(app_.marking().listSubmissions(lecturer, *id));
  if (!c_.confirm("  Send the marks to the Head of Department?")) return;
  const int n = app_.marking().submitForModeration(lecturer, *id);
  c_.print("  " + std::to_string(n) + " mark(s) sent for moderation. Students cannot see them yet.");
}

void ConsoleApp::showAudit(const User& user) {
  const auto id = c_.askId("  Assignment id: ");
  if (!id) return;
  std::vector<std::vector<std::string>> rows;
  for (const AuditRow& r : app_.auditLog().history(user, *id)) {
    rows.push_back({toLocalString(r.entry.changedAt), r.changedByName, r.studentName,
                    r.entry.previousScore ? num(*r.entry.previousScore) : "-", num(r.entry.newScore),
                    r.entry.reason});
  }
  c_.printTable({"When", "Changed by", "Student", "Old", "New", "Reason"}, rows);
}

// ----------------------------------------------------------- head of department

void ConsoleApp::reviewMarks(const User& hod) {
  const auto id = c_.askId("  Assignment id: ");
  if (!id) return;
  const ModerationReview r = app_.moderation().reviewMarks(hod, *id);
  c_.print("  " + r.course.courseCode() + " - " + r.assignment.title() + " (max " +
           num(r.assignment.maxScore()) + ")");
  printMarkingRows(r.rows);
  c_.print("  Awaiting moderation: " + std::to_string(r.awaitingModeration));
  for (const ModerationDecision& d : r.decisions) {
    c_.print("  Earlier decision " + toLocalString(d.decidedAt) + ": " + toString(d.decision) +
             (d.comments.empty() ? "" : " - " + d.comments));
  }
  for (const MarkingRow& row : r.rows) {
    if (row.mark && !row.mark->feedback().empty() && row.mark->state() == ModerationState::UnderModeration) {
      c_.print("  Feedback for " + row.studentName + ": " + row.mark->feedback());
    }
  }
}

void ConsoleApp::approveMarks(const User& hod) {
  const auto id = c_.askId("  Assignment id: ");
  if (!id) return;
  const ModerationReview r = app_.moderation().reviewMarks(hod, *id);
  printMarkingRows(r.rows);
  if (!c_.confirm("  Approve the " + std::to_string(r.awaitingModeration) +
                  " mark(s) awaiting moderation and release them to the students?")) return;
  const int n = app_.moderation().approveMarks(hod, *id, c_.ask("  Comments (optional): "));
  c_.print("  " + std::to_string(n) + " mark(s) approved and released.");
}

void ConsoleApp::rejectMarks(const User& hod) {
  const auto id = c_.askId("  Assignment id: ");
  if (!id) return;
  const std::string reason = c_.ask("  Reason for the lecturer: ");
  const int n = app_.moderation().rejectMarks(hod, *id, reason);
  c_.print("  " + std::to_string(n) + " mark(s) returned to the lecturer.");
}

// -------------------------------------------------------------------- student

void ConsoleApp::listStudentAssignments(const User& student) {
  std::vector<std::vector<std::string>> rows;
  for (const StudentAssignmentRow& r : app_.assignments().listForStudent(student)) {
    rows.push_back({std::to_string(r.assignment.assignmentId()), r.courseCode, r.assignment.title(),
                    toLocalString(r.assignment.deadline()), num(r.assignment.maxScore()), r.statusText});
  }
  c_.printTable({"Id", "Course", "Assignment", "Deadline", "Max", "Status"}, rows);
}

void ConsoleApp::submitAssignment(const User& student) {
  const auto id = c_.askId("  Assignment id: ");
  if (!id) return;
  const Assignment a = app_.assignments().getAssignment(student, *id);
  const Timestamp deadline = app_.submissions().applicableDeadlineFor(student, *id);
  c_.print("  " + a.title() + " - " + a.description());
  c_.print("  Your deadline: " + toLocalString(deadline) + "   Maximum mark: " + num(a.maxScore()));
  const int late = wholeDaysLate(deadline, app_.clock().now());
  if (late > 0) {
    c_.print("  NOTE: the deadline has passed. A late penalty will be applied to your mark.");
  }
  const std::vector<Submission> attempts = app_.submissions().history(student, *id);
  if (!attempts.empty()) {
    c_.print("  You have already submitted (attempt " + std::to_string(attempts.back().attemptNo()) +
             "). A new file replaces it for marking; the old one is kept in your history.");
  }
  const std::string path = c_.ask("  Path of the file to upload: ");
  if (path.empty()) return;
  const std::string comments = c_.ask("  Comments for the lecturer (optional): ");
  if (!c_.confirm(attempts.empty() ? "  Submit this file?" : "  Replace your submission with this file?")) return;
  const SubmissionReceipt r =
      attempts.empty() ? app_.submissions().submit(student, *id, path, comments)
                       : app_.submissions().resubmit(student, attempts.back().submissionId(), path, comments);
  c_.print(std::string("  ") + (r.isResubmission ? "Resubmission" : "Submission") +
           " received (attempt " + std::to_string(r.attemptNo) + ") at " + toLocalString(r.submittedAt) + ".");
  if (r.lateDays > 0) {
    c_.print("  Late by " + std::to_string(r.lateDays) + " day(s): " + num(r.penaltyPercent) +
             "% of the available mark will be deducted" +
             (r.penaltyPercent >= 100.0 ? " (the mark will be zero)." : "."));
  }
}

void ConsoleApp::showHistory(const User& student) {
  const auto id = c_.askId("  Assignment id: ");
  if (!id) return;
  std::vector<std::vector<std::string>> rows;
  for (const Submission& s : app_.submissions().history(student, *id)) {
    rows.push_back({std::to_string(s.attemptNo()), toLocalString(s.submittedAt()), s.getFilePath(),
                    s.comments()});
  }
  c_.printTable({"Attempt", "Submitted", "File", "Comments"}, rows);
  c_.print("  The latest attempt is the one that will be marked.");
}

void ConsoleApp::showReleasedMarks(const User& student) {
  const auto marks = app_.results().releasedMarks(student);
  if (marks.empty()) c_.print("  No marks have been released to you yet.");
  for (const ReleasedMarkView& m : marks) {
    c_.print("  " + m.courseCode + " - " + m.assignmentTitle + ": " + num(m.finalScore) + " / " + num(m.maxScore));
    if (m.penaltyPoints > 0.0) {
      c_.print("    Mark before penalty " + num(m.rawScore) + ", late penalty -" + num(m.penaltyPoints) +
               " (" + std::to_string(m.lateDays) + " day(s) late)");
    }
    c_.print("    Feedback: " + m.feedback);
  }
}

}  // namespace lms
