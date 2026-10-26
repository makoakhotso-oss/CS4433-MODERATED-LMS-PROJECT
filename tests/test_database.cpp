#include "TestSupport.hpp"

using namespace lms;
using namespace lms::test;

namespace {
int count(Database& db, const char* sql) {
  Statement st(db, sql);
  return st.step() ? static_cast<int>(st.columnInt(0)) : -1;
}
}  // namespace

TEST(DatabaseTest, SchemaIsCreatedOnceAndVersioned) {
  Database db(":memory:");
  db.initialiseSchema();
  db.initialiseSchema();  // idempotent
  EXPECT_EQ(count(db, "PRAGMA user_version"), kSchemaVersion);
  EXPECT_EQ(count(db, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'users'"), 1);
  EXPECT_EQ(count(db, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'trigger'"), 6);
}

TEST(DatabaseTest, NewerDatabaseVersionIsRejected) {
  Database db(":memory:");
  db.exec("PRAGMA user_version = 99");
  EXPECT_THROW(db.initialiseSchema(), DatabaseError);
}

TEST(DatabaseTest, CannotOpenImpossiblePath) {
  EXPECT_THROW(Database("/nonexistent_folder_xyz/sub/lms.db"), DatabaseError);
}

TEST(DatabaseTest, DataSurvivesRestart) {
  const auto dir = std::filesystem::temp_directory_path() / "lms_persist_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  AppConfig cfg;
  cfg.databasePath = (dir / "lms.db").string();
  cfg.dataDirectory = (dir / "data").string();
  cfg.hashCost = PasswordHasher::Cost::Minimal;
  FixedClock clock(day(2));
  {
    Application app(cfg, clock);
    app.users().createInitialAdministrator("Admin", "admin", kPassword);
  }
  {
    Application app(cfg, clock);  // new process, same file
    EXPECT_EQ(app.userRepository().countAll(), 1);
    EXPECT_NO_THROW(app.auth().login("admin", kPassword));
  }
  std::filesystem::remove_all(dir);
}

TEST(DatabaseTest, ForeignKeysAreEnforced) {
  World w;
  Statement st(w.app->database(), "INSERT INTO enrollments (student_id, course_id) VALUES (?, ?)");
  st.bindInt(1, 99999).bindInt(2, 99999);
  try {
    st.run();
    FAIL() << "expected a constraint violation";
  } catch (const DatabaseError& e) {
    EXPECT_TRUE(e.isConstraintViolation());
  }
}

TEST(DatabaseTest, MarkAboveAssignmentMaximumIsRefused) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  Statement st(w.app->database(),
               "INSERT INTO marks (submission_id, raw_score, final_score) VALUES (?, 101, 50)");
  st.bindInt(1, r.submissionId);
  EXPECT_THROW(st.run(), DatabaseError);
  const MarkRecord m = w.mark(r.submissionId, 50);
  Statement up(w.app->database(), "UPDATE marks SET final_score = 100.5 WHERE mark_id = ?");
  up.bindInt(1, m.markId());
  EXPECT_THROW(up.run(), DatabaseError);
}

TEST(DatabaseTest, SubmissionRequiresEnrolment) {
  World w;
  Statement st(w.app->database(),
               "INSERT INTO submissions (assignment_id, student_id, attempt_no, submitted_at, file_path) "
               "VALUES (?, ?, 1, '2026-11-02 08:00:00', 'x.txt')");
  st.bindInt(1, w.assignmentId).bindInt(2, w.studentC);
  EXPECT_THROW(st.run(), DatabaseError);
}

TEST(DatabaseTest, AttemptNumbersAreUniquePerStudentAndAssignment) {
  World w;
  w.submit(*w.a, w.assignmentId);
  Statement st(w.app->database(),
               "INSERT INTO submissions (assignment_id, student_id, attempt_no, submitted_at, file_path) "
               "VALUES (?, ?, 1, '2026-11-02 08:00:00', 'dup.txt')");
  st.bindInt(1, w.assignmentId).bindInt(2, w.studentA);
  EXPECT_THROW(st.run(), DatabaseError);
}

TEST(DatabaseTest, UsernamesAndCourseCodesAreUnique) {
  World w;
  EXPECT_THROW(w.app->courses().createCourse(*w.lecturer, "cs101", "Duplicate"), BusinessRuleError);
  NewUser u;
  u.fullName = "Dup";
  u.username = "stud.a";
  u.passwordHash = "x";
  EXPECT_THROW(w.app->userRepository().insert(u), DatabaseError);
}

TEST(DatabaseTest, TransactionRollsBackUnlessCommitted) {
  Database db(":memory:");
  db.initialiseSchema();
  {
    Transaction tx(db);
    Statement st(db, "INSERT INTO departments (name) VALUES ('Temp')");
    st.run();
    // no commit
  }
  EXPECT_EQ(count(db, "SELECT COUNT(*) FROM departments"), 0);
  {
    Transaction tx(db);
    Statement st(db, "INSERT INTO departments (name) VALUES ('Kept')");
    st.run();
    tx.commit();
  }
  EXPECT_EQ(count(db, "SELECT COUNT(*) FROM departments"), 1);
}

TEST(DatabaseTest, NestedTransactionsUseSavepoints) {
  Database db(":memory:");
  db.initialiseSchema();
  {
    Transaction outer(db);
    Statement a(db, "INSERT INTO departments (name) VALUES ('Outer')");
    a.run();
    {
      Transaction inner(db);
      Statement b(db, "INSERT INTO departments (name) VALUES ('Inner')");
      b.run();
      // inner abandoned -> only the inner insert is undone
    }
    outer.commit();
  }
  EXPECT_EQ(count(db, "SELECT COUNT(*) FROM departments"), 1);
  Statement st(db, "SELECT name FROM departments");
  ASSERT_TRUE(st.step());
  EXPECT_EQ(st.columnText(0), "Outer");
}

TEST(DatabaseTest, FailedMarkSaveLeavesNoPartialData) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  // A mark above the maximum is rejected by validation before anything is written.
  EXPECT_THROW(w.app->marking().recordMark(*w.lecturer, r.submissionId, 500, "f"), ValidationError);
  EXPECT_EQ(count(w.app->database(), "SELECT COUNT(*) FROM marks"), 0);
  EXPECT_EQ(count(w.app->database(), "SELECT COUNT(*) FROM mark_audit_log"), 0);
}

// Hostile text is stored and returned as plain data; it never becomes SQL.
TEST(SqlInjectionTest, QuotesAndSqlKeywordsAreTreatedAsData) {
  World w;
  const std::string evil = "x'); DROP TABLE users; --";
  const Assignment a = w.app->assignments().createAssignment(*w.lecturer, w.courseId, evil, "d'; DELETE FROM marks; --", day(20), 10);
  EXPECT_EQ(w.app->assignments().getAssignment(*w.lecturer, a.assignmentId()).title(), evil);
  EXPECT_NO_THROW(w.app->courses().renameCourse(*w.lecturer, w.courseId, "Robert'); DROP TABLE courses;--"));
  EXPECT_EQ(w.app->courses().getCourse(*w.lecturer, w.courseId).courseName(), "Robert'); DROP TABLE courses;--");
  EXPECT_GE(count(w.app->database(), "SELECT COUNT(*) FROM users"), 8);
  const auto r = w.submit(*w.a, w.assignmentId);
  EXPECT_NO_THROW(w.app->marking().recordMark(*w.lecturer, r.submissionId, 5, "'; UPDATE marks SET raw_score = 100; --"));
  EXPECT_DOUBLE_EQ(w.app->marking().listSubmissions(*w.lecturer, w.assignmentId).at(0).mark->rawScore(), 5.0);
}

TEST(ConcurrencyTest, SecondWriterWaitsOrFailsCleanly) {
  const auto dir = std::filesystem::temp_directory_path() / "lms_lock_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  Database one((dir / "l.db").string());
  one.initialiseSchema();
  Database two((dir / "l.db").string());
  Transaction tx(one);  // holds the write lock
  Statement st(two, "INSERT INTO departments (name) VALUES ('Blocked')");
  // two.busy_timeout is 5 s; shorten the wait for this test.
  two.exec("PRAGMA busy_timeout = 100");
  EXPECT_THROW(st.run(), DatabaseError);  // reported as an error, no crash or corruption
  tx.commit();
  std::filesystem::remove_all(dir);
}
