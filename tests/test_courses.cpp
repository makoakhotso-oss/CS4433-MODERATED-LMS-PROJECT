#include "TestSupport.hpp"

using namespace lms;
using namespace lms::test;

TEST(CourseTest, LecturerCreatesAndManagesOwnCourse) {
  World w;
  const Course c = w.app->courses().createCourse(*w.lecturer, " math 101 ", "Mathematics");
  EXPECT_EQ(c.courseCode(), "MATH 101");  // normalised
  EXPECT_EQ(c.lecturerId(), w.lecturerId);
  EXPECT_EQ(c.departmentId(), w.sciences);
  w.app->courses().renameCourse(*w.lecturer, c.courseId(), "Pure Mathematics");
  EXPECT_EQ(w.app->courses().getCourse(*w.lecturer, c.courseId()).courseName(), "Pure Mathematics");
  EXPECT_EQ(w.app->courses().listCourses(*w.lecturer).size(), 2u);  // CS101 + MATH 101
  EXPECT_EQ(w.app->courses().listCourses(*w.hod).size(), 2u);
}

TEST(CourseTest, CourseValidation) {
  World w;
  EXPECT_THROW(w.app->courses().createCourse(*w.lecturer, "X", "Too short code"), ValidationError);
  EXPECT_THROW(w.app->courses().createCourse(*w.lecturer, "BAD;CODE", "Name"), ValidationError);
  EXPECT_THROW(w.app->courses().createCourse(*w.lecturer, "OK101", "   "), ValidationError);
  EXPECT_THROW(w.app->courses().createCourse(*w.lecturer, "CS101", "Duplicate code"), BusinessRuleError);
  EXPECT_THROW(w.app->courses().renameCourse(*w.lecturer, 9999, "Name"), NotFoundError);
}

TEST(CourseTest, AdministratorCreatesCoursesForLecturersAndReassigns) {
  World w;
  EXPECT_THROW(w.app->courses().createCourse(*w.admin, "ENG101", "English"), ValidationError);       // lecturer needed
  EXPECT_THROW(w.app->courses().createCourse(*w.admin, "ENG101", "English", w.studentA), NotFoundError);  // not a lecturer
  const Course c = w.app->courses().createCourse(*w.admin, "ENG101", "English", w.lecturer2Id);
  EXPECT_EQ(c.lecturerId(), w.lecturer2Id);
  w.app->courses().assignLecturer(*w.admin, c.courseId(), w.lecturerId);
  EXPECT_EQ(w.app->courses().getCourse(*w.admin, c.courseId()).lecturerId(), w.lecturerId);
  EXPECT_EQ(w.app->courses().listCourses(*w.admin).size(), 2u);
  EXPECT_THROW(w.app->courses().assignLecturer(*w.admin, c.courseId(), w.hodId), NotFoundError);
  // a lecturer of another department cannot be assigned
  const Id artsLecturer = w.add("Arts Lecturer", "lect.arts", Role::Lecturer, w.arts);
  EXPECT_THROW(w.app->courses().assignLecturer(*w.admin, c.courseId(), artsLecturer), BusinessRuleError);
  // a deactivated lecturer cannot be given a course
  w.app->users().setActive(*w.admin, w.lecturer2Id, false);
  EXPECT_THROW(w.app->courses().createCourse(*w.admin, "ENG102", "English 2", w.lecturer2Id), BusinessRuleError);
  EXPECT_THROW(w.app->courses().assignLecturer(*w.admin, c.courseId(), w.lecturer2Id), BusinessRuleError);
}

TEST(EnrolmentTest, EnrolRules) {
  World w;
  auto& svc = w.app->courses();
  EXPECT_THROW(svc.enrolStudent(*w.admin, w.courseId, w.studentA), BusinessRuleError);  // already enrolled
  EXPECT_THROW(svc.enrolStudent(*w.admin, w.courseId, w.lecturerId), NotFoundError);    // not a student
  EXPECT_THROW(svc.enrolStudent(*w.admin, 9999, w.studentC), NotFoundError);
  w.app->users().setActive(*w.admin, w.studentC, false);
  EXPECT_THROW(svc.enrolStudent(*w.admin, w.courseId, w.studentC), BusinessRuleError);  // deactivated
  w.app->users().setActive(*w.admin, w.studentC, true);
  svc.enrolStudent(*w.admin, w.courseId, w.studentC);
  const auto students = svc.listEnrolledStudents(*w.lecturer, w.courseId);
  EXPECT_EQ(students.size(), 3u);
  EXPECT_THROW(svc.listEnrolledStudents(*w.a, w.courseId), PermissionDeniedError);
  EXPECT_THROW(svc.listEnrolledStudents(*w.lecturer2, w.courseId), NotFoundError);
  EXPECT_EQ(svc.listEnrolledStudents(*w.hod, w.courseId).size(), 3u);
}

TEST(EnrolmentTest, UnenrolRules) {
  World w;
  auto& svc = w.app->courses();
  EXPECT_THROW(svc.unenrolStudent(*w.admin, w.courseId, w.studentC), NotFoundError);  // not enrolled
  w.submit(*w.a, w.assignmentId);
  EXPECT_THROW(svc.unenrolStudent(*w.admin, w.courseId, w.studentA), BusinessRuleError);  // has submissions
  EXPECT_NO_THROW(svc.unenrolStudent(*w.admin, w.courseId, w.studentB));
  EXPECT_TRUE(svc.listCourses(*w.b).empty());
  EXPECT_THROW(w.submit(*w.b, w.assignmentId), NotFoundError);  // no longer enrolled
}

TEST(DepartmentTest, CreateAndList) {
  World w;
  EXPECT_THROW(w.app->users().createDepartment(*w.admin, "Sciences"), BusinessRuleError);
  EXPECT_THROW(w.app->users().createDepartment(*w.admin, "   "), ValidationError);
  const auto all = w.app->users().listDepartments(*w.admin);
  ASSERT_EQ(all.size(), 2u);
  EXPECT_EQ(all[0].name, "Arts");  // sorted by name
}
