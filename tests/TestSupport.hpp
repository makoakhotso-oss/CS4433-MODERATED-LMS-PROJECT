#pragma once
// Shared fixture: a complete in-memory system with a small school in it.
#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

#include "lms/Application.hpp"
#include "lms/common/Errors.hpp"
#include "lms/common/Time.hpp"
#include "lms/data/Schema.hpp"

namespace lms::test {

inline const char* const kPassword = "Passw0rd!1";

inline Timestamp day(int d, int hour = 9, int minute = 0) {
  return makeUtc(2026, 11, d, hour, minute, 0);
}

class World {
 public:
  explicit World(PasswordHasher::Cost cost = PasswordHasher::Cost::Minimal,
                 SubmissionOptions options = {})
      : clock(day(2, 8)) {
    static std::atomic<int> counter{0};
    dir = std::filesystem::temp_directory_path() /
          ("lms_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" + std::to_string(counter++));
    std::filesystem::create_directories(dir);
    AppConfig cfg;
    cfg.databasePath = ":memory:";
    cfg.dataDirectory = (dir / "data").string();
    cfg.hashCost = cost;
    cfg.submissionOptions = options;
    app = std::make_unique<Application>(cfg, clock);

    app->users().createInitialAdministrator("Admin One", "admin", kPassword);
    admin = login("admin");
    sciences = app->users().createDepartment(*admin, "Sciences");
    arts = app->users().createDepartment(*admin, "Arts");
    lecturerId = add("Lecturer One", "lect1", Role::Lecturer, sciences);
    lecturer2Id = add("Lecturer Two", "lect2", Role::Lecturer, sciences);
    hodId = add("Head Sciences", "hod1", Role::HeadOfDepartment, sciences);
    hodArtsId = add("Head Arts", "hod2", Role::HeadOfDepartment, arts);
    studentA = add("Student A", "stud.a", Role::Student, std::nullopt);
    studentB = add("Student B", "stud.b", Role::Student, std::nullopt);
    studentC = add("Student C", "stud.c", Role::Student, std::nullopt);
    lecturer = login("lect1");
    lecturer2 = login("lect2");
    hod = login("hod1");
    hodArts = login("hod2");
    a = login("stud.a");
    b = login("stud.b");
    c = login("stud.c");

    courseId = app->courses().createCourse(*lecturer, "CS101", "Computer Studies").courseId();
    app->courses().enrolStudent(*admin, courseId, studentA);
    app->courses().enrolStudent(*admin, courseId, studentB);
    deadline = day(5, 9);  // assignment deadline: 5 Nov 09:00 UTC
    assignmentId = app->assignments()
                       .createAssignment(*lecturer, courseId, "Project", "Build it", deadline, 100.0)
                       .assignmentId();
  }

  ~World() {
    app.reset();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }

  std::unique_ptr<User> login(const std::string& username) {
    return app->auth().login(username, kPassword);
  }

  Id add(const std::string& name, const std::string& username, Role role,
         std::optional<Id> dept) {
    NewUserRequest r;
    r.fullName = name;
    r.username = username;
    r.password = kPassword;
    r.role = role;
    r.departmentId = dept;
    return app->users().createUser(*admin, r);
  }

  std::filesystem::path makeFile(const std::string& name, const std::string& content = "work") {
    const auto path = dir / name;
    std::ofstream(path) << content;
    return path;
  }

  /// Student `s` submits a file at the current clock time.
  SubmissionReceipt submit(const User& s, Id assignment, const std::string& name = "answer.txt") {
    return app->submissions().submit(s, assignment, makeFile(name, "content of " + name));
  }

  /// A fully moderated pipeline helper: mark a submission and send marks for moderation.
  MarkRecord mark(Id submissionId, double score, const std::string& feedback = "Good work") {
    return app->marking().recordMark(*lecturer, submissionId, score, feedback);
  }

  int countFiles() const {
    int n = 0;
    const auto root = dir / "data";
    if (!std::filesystem::exists(root)) return 0;
    for (const auto& e : std::filesystem::recursive_directory_iterator(root)) {
      if (e.is_regular_file()) ++n;
    }
    return n;
  }

  FixedClock clock;
  std::filesystem::path dir;
  std::unique_ptr<Application> app;
  std::unique_ptr<User> admin, lecturer, lecturer2, hod, hodArts, a, b, c;
  Id sciences{}, arts{}, lecturerId{}, lecturer2Id{}, hodId{}, hodArtsId{};
  Id studentA{}, studentB{}, studentC{}, courseId{}, assignmentId{};
  Timestamp deadline{};
};

}  // namespace lms::test
