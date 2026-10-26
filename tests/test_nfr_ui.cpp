#include <chrono>
#include <sstream>

#include "TestSupport.hpp"
#include "lms/ui/ConsoleApp.hpp"

using namespace lms;
using namespace lms::test;

// NFR-01 / NFR-02: with 800 students stored, a login completes within 2 seconds, using the
// production password-hashing cost.
TEST(PerformanceTest, LoginWithEightHundredStudentsStored) {
  World w(PasswordHasher::Cost::Interactive);
  const std::string cheapHash = PasswordHasher(PasswordHasher::Cost::Minimal).hash(kPassword);
  for (int i = 0; i < 799; ++i) {
    NewUser u;
    u.fullName = "Pupil " + std::to_string(i);
    u.username = "pupil" + std::to_string(i);
    u.passwordHash = cheapHash;
    u.role = Role::Student;
    w.app->userRepository().insert(u);
  }
  const Id real = w.add("Real Pupil", "real.pupil", Role::Student, std::nullopt);
  (void)real;
  ASSERT_GE(w.app->userRepository().countAll(), 800);

  const auto start = std::chrono::steady_clock::now();
  const auto user = w.app->auth().login("real.pupil", kPassword);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  EXPECT_TRUE(user->isLoggedIn());
  EXPECT_LT(ms, 2000) << "login took " << ms << " ms";
  std::cout << "[ INFO ] login with 800+ users took " << ms << " ms\n";
}

// NFR-02: 800 student records are stored and every one can be retrieved correctly.
TEST(CapacityTest, EightHundredStudentsAreStoredAndRetrievedUnchanged) {
  World w;
  for (int i = 0; i < 800; ++i) {
    NewUser u;
    u.fullName = "Student Number " + std::to_string(i);
    u.username = "cap" + std::to_string(i);
    u.passwordHash = "hash" + std::to_string(i);
    u.role = Role::Student;
    w.app->userRepository().insert(u);
  }
  for (int i = 0; i < 800; ++i) {
    const auto u = w.app->userRepository().findByUsername("cap" + std::to_string(i));
    ASSERT_TRUE(u) << i;
    EXPECT_EQ(u->fullName(), "Student Number " + std::to_string(i));
    EXPECT_EQ(u->passwordHash(), "hash" + std::to_string(i));
    EXPECT_EQ(u->role(), Role::Student);
  }
  int students = 0;
  for (const auto& u : w.app->userRepository().findAll()) students += u->role() == Role::Student ? 1 : 0;
  EXPECT_EQ(students, 803);  // 800 + the three students of the fixture
}

TEST(PerformanceTest, ClassOfEightHundredIsListedQuickly) {
  World w;
  std::vector<Id> ids;
  for (int i = 0; i < 800; ++i) {
    NewUser u;
    u.fullName = "Pupil " + std::to_string(i);
    u.username = "pupil" + std::to_string(i);
    u.passwordHash = "x";
    u.role = Role::Student;
    ids.push_back(w.app->userRepository().insert(u));
  }
  for (const Id id : ids) w.app->courses().enrolStudent(*w.admin, w.courseId, id);
  Database& db = w.app->database();
  {
    Transaction tx(db);  // direct inserts of 800 submissions, one transaction (fast bulk load)
    SQLiteSubmissionRepository repo(db);
    for (const Id id : ids) {
      Submission sub(kNoId, w.assignmentId, id, 1, day(3), "f.txt");
      repo.save(sub);
    }
    tx.commit();
  }
  const auto start = std::chrono::steady_clock::now();
  const auto rows = w.app->marking().listSubmissions(*w.lecturer, w.assignmentId);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  EXPECT_EQ(rows.size(), 800u);
  EXPECT_LT(ms, 2000);
  std::cout << "[ INFO ] marking list for 800 submissions took " << ms << " ms\n";
}

// ---- console user interface: bad input must never crash the program ----------------------

namespace {
std::string runUi(World& w, const std::vector<std::string>& lines) {
  std::string input;
  for (const auto& l : lines) input += l + "\n";
  std::istringstream in(input);
  std::ostringstream out;
  Console console(in, out);
  ConsoleApp ui(*w.app, console);
  ui.run();
  return out.str();
}
}  // namespace

TEST(UiTest, StudentSessionSurvivesGarbageInput) {
  World w;
  const std::string out = runUi(w, {"abc", "7", "1", "stud.a", kPassword, "xyz", "-4", "99", "3", "notanumber",
                                    "3", "9999", "2", "0", "0"});
  EXPECT_NE(out.find("Welcome, Student A"), std::string::npos);
  EXPECT_NE(out.find("That is not a menu option."), std::string::npos);
  EXPECT_NE(out.find("Please enter a positive whole number."), std::string::npos);
  EXPECT_NE(out.find("Assignment not found."), std::string::npos);  // error shown, no crash
  EXPECT_NE(out.find("Project"), std::string::npos);                // assignment list printed
  EXPECT_NE(out.find("Goodbye."), std::string::npos);
}

TEST(UiTest, WrongPasswordShowsGenericMessageAndInputEndStopsCleanly) {
  World w;
  const std::string out = runUi(w, {"1", "stud.a", "nope1234", "1", "stud.a"});  // input ends mid-login
  EXPECT_NE(out.find("Invalid username or password."), std::string::npos);
  EXPECT_NE(out.find("Goodbye."), std::string::npos);
}

TEST(UiTest, StudentCannotReachStaffMenuItems) {
  World w;
  const std::string out = runUi(w, {"1", "stud.a", kPassword, "0", "0"});
  EXPECT_EQ(out.find("Create user"), std::string::npos);
  EXPECT_EQ(out.find("Approve and release marks"), std::string::npos);
  EXPECT_NE(out.find("Submit or resubmit an assignment"), std::string::npos);
}

TEST(UiTest, FullWorkflowThroughTheMenus) {
  World w;
  const auto file = w.makeFile("essay.txt", "my essay");
  // student submits
  std::string out = runUi(w, {"1", "stud.a", kPassword, "3", std::to_string(w.assignmentId), file.string(), "Please check page 2", "y", "0", "0"});
  EXPECT_NE(out.find("Submission received (attempt 1)"), std::string::npos);
  const Id sub = w.app->submissions().history(*w.a, w.assignmentId).at(0).submissionId();
  EXPECT_EQ(w.app->submissions().history(*w.a, w.assignmentId).at(0).comments(), "Please check page 2");
  // lecturer marks via menu and sends for moderation
  out = runUi(w, {"1", "lect1", kPassword, "7", std::to_string(w.assignmentId), std::to_string(sub), "82", "Solid essay", "", "",
                  "8", std::to_string(w.assignmentId), "y", "0", "0"});
  EXPECT_NE(out.find("Student's comments: Please check page 2"), std::string::npos);
  EXPECT_NE(out.find("Saved: mark 82.00"), std::string::npos);
  EXPECT_NE(out.find("1 mark(s) sent for moderation"), std::string::npos);
  EXPECT_TRUE(w.app->results().releasedMarks(*w.a).empty());
  // HoD approves
  out = runUi(w, {"1", "hod1", kPassword, "3", std::to_string(w.assignmentId), "y", "Fine", "0", "0"});
  EXPECT_NE(out.find("1 mark(s) approved and released."), std::string::npos);
  // student sees the mark and feedback
  out = runUi(w, {"1", "stud.a", kPassword, "5", "0", "0"});
  EXPECT_NE(out.find("82.00 / 100.00"), std::string::npos);
  EXPECT_NE(out.find("Solid essay"), std::string::npos);
}

TEST(UiTest, FirstRunSetupCreatesTheAdministrator) {
  const auto dir = std::filesystem::temp_directory_path() / "lms_ui_first_run";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  AppConfig cfg;
  cfg.databasePath = ":memory:";
  cfg.dataDirectory = (dir / "d").string();
  cfg.hashCost = PasswordHasher::Cost::Minimal;
  FixedClock clock(day(2));
  Application app(cfg, clock);
  std::istringstream in("Head Teacher\nboss\nAbcdefg1\nDifferent2\nHead Teacher\nboss\nAbcdefg1\nAbcdefg1\n");
  std::ostringstream out;
  Console console(in, out);
  ConsoleApp ui(app, console);
  ui.firstRunSetup();
  EXPECT_EQ(app.userRepository().countAll(), 1);  // first attempt: passwords differ; second succeeds
  EXPECT_NE(out.str().find("The passwords do not match."), std::string::npos);
  EXPECT_NO_THROW(app.auth().login("boss", "Abcdefg1"));
  std::filesystem::remove_all(dir);
}
