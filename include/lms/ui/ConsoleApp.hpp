#pragma once
// Presentation layer: the text-menu user interface for the four roles. It only calls the
// services; it contains no business rules and no SQL.
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lms/Application.hpp"
#include "lms/ui/Console.hpp"

namespace lms {

class ConsoleApp {
 public:
  ConsoleApp(Application& app, Console& console) : app_(app), c_(console) {}

  /// First-run screen: asks for the first administrator when the system has no users.
  void firstRunSetup();
  /// Login loop; returns when the user chooses Exit or input ends.
  void run();

 private:
  struct MenuItem {
    std::string label;
    std::function<void()> action;
  };

  void runMenu(const User& user, const std::string& title, const std::vector<MenuItem>& items);
  void guarded(const std::function<void()>& action);

  void administratorMenu(User& user);
  void lecturerMenu(User& user);
  void headOfDepartmentMenu(User& user);
  void studentMenu(User& user);
  void changePassword(const User& user);

  // Administrator
  void createUser(const User& admin);
  void listUsers(const User& admin);
  void updateUser(const User& admin);
  void toggleActive(const User& admin);
  void resetPassword(const User& admin);
  void createDepartment(const User& admin);
  void listDepartments(const User& admin);
  void adminCreateCourse(const User& admin);
  void listCourses(const User& user, bool withAssignments);
  void assignLecturer(const User& admin);
  void enrol(const User& admin, bool enrolStudent);
  void listEnrolled(const User& user);

  // Lecturer
  void lecturerCreateCourse(const User& lecturer);
  void renameCourse(const User& lecturer);
  void createAssignment(const User& lecturer);
  void grantExtension(const User& lecturer);
  void markWork(const User& lecturer);
  void submitForModeration(const User& lecturer);
  void showAudit(const User& user);

  // Head of department
  void reviewMarks(const User& hod);
  void approveMarks(const User& hod);
  void rejectMarks(const User& hod);

  // Student
  void listStudentAssignments(const User& student);
  void submitAssignment(const User& student);
  void showHistory(const User& student);
  void showReleasedMarks(const User& student);

  void printMarkingRows(const std::vector<MarkingRow>& rows);

  Application& app_;
  Console& c_;
};

}  // namespace lms
