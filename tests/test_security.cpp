#include "TestSupport.hpp"

using namespace lms;
using namespace lms::test;

TEST(AuthTest, CorrectCredentialsLogIn) {
  World w;
  const auto u = w.app->auth().login("  STUD.A ", kPassword);  // case and spaces are normalised
  EXPECT_TRUE(u->isLoggedIn());
  EXPECT_EQ(u->role(), Role::Student);
  EXPECT_EQ(u->getRole(), "Student");
}

TEST(AuthTest, WrongPasswordUnknownUserAndInactiveAreIndistinguishable) {
  World w;
  const std::string expected = "Invalid username or password.";
  auto message = [&](const std::string& user, const std::string& pw) {
    try {
      w.app->auth().login(user, pw);
    } catch (const AuthenticationError& e) {
      return std::string(e.what());
    }
    return std::string("NO ERROR");
  };
  EXPECT_EQ(message("stud.a", "wrongPass1"), expected);
  EXPECT_EQ(message("nobody", kPassword), expected);
  w.app->users().setActive(*w.admin, w.studentC, false);
  EXPECT_EQ(message("stud.c", kPassword), expected);
  EXPECT_EQ(message("", ""), expected);
}

TEST(AuthTest, SqlInjectionAttemptsFail) {
  World w;
  EXPECT_THROW(w.app->auth().login("admin' --", "x"), AuthenticationError);
  EXPECT_THROW(w.app->auth().login("' OR '1'='1", "' OR '1'='1"), AuthenticationError);
  EXPECT_NO_THROW(w.login("admin"));  // the database is untouched
}

TEST(AuthTest, TemporaryLockoutAfterRepeatedFailures) {
  World w;
  for (int i = 0; i < 5; ++i) EXPECT_THROW(w.app->auth().login("stud.a", "badPass12"), AuthenticationError);
  try {
    w.app->auth().login("stud.a", kPassword);  // even the right password is refused while locked
    FAIL() << "expected lockout";
  } catch (const AuthenticationError& e) {
    EXPECT_NE(std::string(e.what()).find("Too many failed attempts"), std::string::npos);
  }
  w.clock.advance(301);
  EXPECT_NO_THROW(w.app->auth().login("stud.a", kPassword));
}

// NFR-03: the database holds salted hashes, never the password.
TEST(AuthTest, PasswordsAreStoredAsSaltedHashes) {
  World w;
  Statement st(w.app->database(), "SELECT username, password_hash FROM users");
  int rows = 0;
  while (st.step()) {
    ++rows;
    const std::string hash = st.columnText(1);
    EXPECT_EQ(hash.rfind("$argon2id$", 0), 0u) << st.columnText(0);
    EXPECT_EQ(hash.find(kPassword), std::string::npos);
  }
  EXPECT_GE(rows, 8);
  // Two users with the same password have different hashes (different salts).
  EXPECT_NE(w.app->userRepository().findByUsername("stud.a")->passwordHash(),
            w.app->userRepository().findByUsername("stud.b")->passwordHash());
}

TEST(AuthTest, ChangePassword) {
  World w;
  EXPECT_THROW(w.app->auth().changePassword(*w.a, "wrong", "NewPass123"), AuthenticationError);
  EXPECT_THROW(w.app->auth().changePassword(*w.a, kPassword, "short1"), ValidationError);
  w.app->auth().changePassword(*w.a, kPassword, "NewPass123");
  EXPECT_THROW(w.login("stud.a"), AuthenticationError);
  EXPECT_NO_THROW(w.app->auth().login("stud.a", "NewPass123"));
}

TEST(AuthTest, ResetPasswordByAdministratorOnly) {
  World w;
  EXPECT_THROW(w.app->users().resetPassword(*w.lecturer, w.studentA, "Another123"), PermissionDeniedError);
  w.app->users().resetPassword(*w.admin, w.studentA, "Another123");
  EXPECT_NO_THROW(w.app->auth().login("stud.a", "Another123"));
}

// NFR-04 / FR-03: every role is refused the functions that belong to other roles.
TEST(AccessControlTest, StudentIsRefusedStaffFunctions) {
  World w;
  EXPECT_THROW(w.app->users().listUsers(*w.a), PermissionDeniedError);
  EXPECT_THROW(w.app->courses().createCourse(*w.a, "HACK1", "Hack"), PermissionDeniedError);
  EXPECT_THROW(w.app->assignments().createAssignment(*w.a, w.courseId, "t", "d", day(20), 10), PermissionDeniedError);
  EXPECT_THROW(w.app->assignments().grantExtension(*w.a, w.assignmentId, w.studentA, day(9), "why"), PermissionDeniedError);
  EXPECT_THROW(w.app->marking().recordMark(*w.a, 1, 50, "f"), PermissionDeniedError);
  EXPECT_THROW(w.app->marking().listSubmissions(*w.a, w.assignmentId), PermissionDeniedError);
  EXPECT_THROW(w.app->moderation().approveMarks(*w.a, w.assignmentId), PermissionDeniedError);
  EXPECT_THROW(w.app->auditLog().history(*w.a, w.assignmentId), PermissionDeniedError);
  EXPECT_THROW(w.app->courses().enrolStudent(*w.a, w.courseId, w.studentC), PermissionDeniedError);
}

TEST(AccessControlTest, LecturerIsRefusedAdminAndHodFunctions) {
  World w;
  EXPECT_THROW(w.app->users().listUsers(*w.lecturer), PermissionDeniedError);
  EXPECT_THROW(w.app->users().createDepartment(*w.lecturer, "X"), PermissionDeniedError);
  EXPECT_THROW(w.app->courses().enrolStudent(*w.lecturer, w.courseId, w.studentC), PermissionDeniedError);
  EXPECT_THROW(w.app->courses().assignLecturer(*w.lecturer, w.courseId, w.lecturer2Id), PermissionDeniedError);
  EXPECT_THROW(w.app->moderation().reviewMarks(*w.lecturer, w.assignmentId), PermissionDeniedError);
  EXPECT_THROW(w.app->moderation().approveMarks(*w.lecturer, w.assignmentId), PermissionDeniedError);
  EXPECT_THROW(w.app->submissions().submit(*w.lecturer, w.assignmentId, w.makeFile("x.txt")), PermissionDeniedError);
  EXPECT_THROW(w.app->results().releasedMarks(*w.lecturer), PermissionDeniedError);
}

TEST(AccessControlTest, HodIsRefusedLecturerAndStudentFunctions) {
  World w;
  EXPECT_THROW(w.app->users().createUser(*w.hod, NewUserRequest{"X Y", "xyz", "Passw0rd!1", Role::Student, std::nullopt}), PermissionDeniedError);
  EXPECT_THROW(w.app->marking().recordMark(*w.hod, 1, 10, "f"), PermissionDeniedError);
  EXPECT_THROW(w.app->marking().submitForModeration(*w.hod, w.assignmentId), PermissionDeniedError);
  EXPECT_THROW(w.app->assignments().createAssignment(*w.hod, w.courseId, "t", "d", day(20), 10), PermissionDeniedError);
  EXPECT_THROW(w.app->submissions().submit(*w.hod, w.assignmentId, w.makeFile("x.txt")), PermissionDeniedError);
}

TEST(AccessControlTest, AdministratorCannotSeeOrChangeAssessment) {
  World w;
  EXPECT_THROW(w.app->marking().listSubmissions(*w.admin, w.assignmentId), PermissionDeniedError);
  EXPECT_THROW(w.app->moderation().reviewMarks(*w.admin, w.assignmentId), PermissionDeniedError);
  EXPECT_THROW(w.app->submissions().submit(*w.admin, w.assignmentId, w.makeFile("x.txt")), PermissionDeniedError);
  EXPECT_THROW(w.app->auditLog().history(*w.admin, w.assignmentId), PermissionDeniedError);
}

TEST(AccessControlTest, LoggedOutUserIsRefusedEverything) {
  World w;
  w.a->logout();
  EXPECT_THROW(w.app->assignments().listForStudent(*w.a), PermissionDeniedError);
  EXPECT_THROW(w.app->results().releasedMarks(*w.a), PermissionDeniedError);
  EXPECT_THROW(w.app->courses().getCourse(*w.a, w.courseId), PermissionDeniedError);
}

TEST(AccessControlTest, OwnershipIsEnforcedBetweenLecturers) {
  World w;
  EXPECT_THROW(w.app->assignments().createAssignment(*w.lecturer2, w.courseId, "t", "d", day(20), 10), PermissionDeniedError);
  EXPECT_THROW(w.app->assignments().grantExtension(*w.lecturer2, w.assignmentId, w.studentA, day(9), "why"), PermissionDeniedError);
  EXPECT_THROW(w.app->courses().renameCourse(*w.lecturer2, w.courseId, "Stolen"), PermissionDeniedError);
  EXPECT_THROW(w.app->marking().listSubmissions(*w.lecturer2, w.assignmentId), NotFoundError);
  EXPECT_THROW(w.app->courses().getCourse(*w.lecturer2, w.courseId), NotFoundError);
}

TEST(AccessControlTest, StudentsSeeOnlyTheirOwnCoursesAndAssignments) {
  World w;
  EXPECT_EQ(w.app->courses().listCourses(*w.a).size(), 1u);
  EXPECT_TRUE(w.app->courses().listCourses(*w.c).empty());  // C is not enrolled anywhere
  EXPECT_THROW(w.app->assignments().listAssignments(*w.c, w.courseId), NotFoundError);
  EXPECT_THROW(w.app->assignments().getAssignment(*w.c, w.assignmentId), NotFoundError);
  EXPECT_TRUE(w.app->assignments().listForStudent(*w.c).empty());
  EXPECT_EQ(w.app->assignments().listForStudent(*w.a).size(), 1u);
}

TEST(AccessControlTest, HodIsLimitedToTheirDepartment) {
  World w;
  EXPECT_EQ(w.app->courses().listCourses(*w.hod).size(), 1u);
  EXPECT_TRUE(w.app->courses().listCourses(*w.hodArts).empty());
  EXPECT_THROW(w.app->moderation().reviewMarks(*w.hodArts, w.assignmentId), NotFoundError);
}

TEST(UserServiceTest, AccountManagement) {
  World w;
  EXPECT_THROW(w.add("Dup", "stud.a", Role::Student, std::nullopt), BusinessRuleError);
  EXPECT_THROW(w.add("No Dept", "lect9", Role::Lecturer, std::nullopt), ValidationError);
  EXPECT_THROW(w.add("Bad Dept", "lect8", Role::Lecturer, 9999), NotFoundError);
  EXPECT_THROW(w.add("", "someone", Role::Student, std::nullopt), ValidationError);
  EXPECT_THROW(w.app->users().createInitialAdministrator("X", "second", kPassword), BusinessRuleError);

  w.app->users().setActive(*w.admin, w.studentA, false);
  EXPECT_THROW(w.login("stud.a"), AuthenticationError);
  w.app->users().setActive(*w.admin, w.studentA, true);
  EXPECT_NO_THROW(w.login("stud.a"));

  EXPECT_THROW(w.app->users().setActive(*w.admin, w.admin->userId(), false), BusinessRuleError);
  // a role cannot change once the person has academic records
  EXPECT_THROW(w.app->users().updateUser(*w.admin, w.studentA, "Student A", Role::Lecturer, w.sciences), BusinessRuleError);
  EXPECT_NO_THROW(w.app->users().updateUser(*w.admin, w.studentC, "Student Cee", Role::Student, std::nullopt));
  EXPECT_EQ(w.app->userRepository().findById(w.studentC)->fullName(), "Student Cee");
}

TEST(UserServiceTest, LastAdministratorIsProtected) {
  World w;
  const Id second = w.add("Admin Two", "admin2", Role::Administrator, std::nullopt);
  const auto admin2 = w.login("admin2");
  w.app->users().setActive(*admin2, w.admin->userId(), false);  // allowed: admin2 remains
  // admin2 is now the only active administrator and cannot be deactivated or demoted
  EXPECT_THROW(w.app->users().setActive(*admin2, second, false), BusinessRuleError);
  EXPECT_THROW(w.app->users().updateUser(*admin2, second, "Admin Two", Role::Student, std::nullopt), BusinessRuleError);
}
