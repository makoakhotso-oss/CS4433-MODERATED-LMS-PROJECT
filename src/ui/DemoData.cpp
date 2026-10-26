#include "lms/ui/DemoData.hpp"

#include "lms/common/Errors.hpp"

namespace lms {

void seedDemoData(Application& app, Console& console) {
  if (app.userRepository().countAll() != 0) {
    throw BusinessRuleError("Demo data can only be created in an empty system.");
  }
  const std::string adminPw = "Admin2026!";
  const std::string demoPw = "Passw0rd!1";
  app.users().createInitialAdministrator("System Administrator", "admin", adminPw);
  const std::unique_ptr<User> admin = app.auth().login("admin", adminPw);

  const Id dept = app.users().createDepartment(*admin, "Sciences");
  auto add = [&](const char* name, const char* username, Role role, bool dep) {
    NewUserRequest r;
    r.fullName = name;
    r.username = username;
    r.password = demoPw;
    r.role = role;
    if (dep) r.departmentId = dept;
    return app.users().createUser(*admin, r);
  };
  const Id hod = add("Dr Palesa Mohapi", "hod.mohapi", Role::HeadOfDepartment, true);
  const Id mokoena = add("Mr Thabo Mokoena", "lect.mokoena", Role::Lecturer, true);
  const Id molapo = add("Ms Lineo Molapo", "lect.molapo", Role::Lecturer, true);
  (void)hod;
  std::vector<Id> students;
  for (const auto& s : {std::pair<const char*, const char*>{"Neo Khati", "s.khati"},
                        {"Lerato Tau", "s.tau"},
                        {"Tumelo Sello", "s.sello"},
                        {"Mpho Letsie", "s.letsie"},
                        {"Karabo Ntai", "s.ntai"}}) {
    students.push_back(add(s.first, s.second, Role::Student, false));
  }

  const Course math = app.courses().createCourse(*admin, "MATH101", "Mathematics", mokoena);
  const Course cs = app.courses().createCourse(*admin, "CS101", "Computer Studies", molapo);
  for (Id s : students) {
    app.courses().enrolStudent(*admin, math.courseId(), s);
    app.courses().enrolStudent(*admin, cs.courseId(), s);
  }

  const Timestamp due = app.clock().now() + 7 * kSecondsPerDay;
  const std::unique_ptr<User> l1 = app.auth().login("lect.mokoena", demoPw);
  app.assignments().createAssignment(*l1, math.courseId(), "Algebra worksheet 1",
                                     "Solve the ten problems on the worksheet.", due, 50.0);
  const std::unique_ptr<User> l2 = app.auth().login("lect.molapo", demoPw);
  app.assignments().createAssignment(*l2, cs.courseId(), "Spreadsheet project",
                                     "Build the budget spreadsheet described in class.", due, 100.0);

  console.print("Demo data created. Accounts (username / password):");
  console.print("  admin         / " + adminPw + "   (Administrator)");
  console.print("  hod.mohapi    / " + demoPw + "   (Head of Department)");
  console.print("  lect.mokoena  / " + demoPw + "   (Lecturer, MATH101)");
  console.print("  lect.molapo   / " + demoPw + "   (Lecturer, CS101)");
  console.print("  s.khati, s.tau, s.sello, s.letsie, s.ntai / " + demoPw + "   (Students)");
  console.print("These passwords are for demonstration only - change them before real use.");
}

}  // namespace lms
