#pragma once
// Repository interfaces (Repository pattern, Class Diagram section 3.2.3). Services depend
// only on these abstractions (DIP); SQLite implementations live in Sqlite*Repository.hpp.
// The interfaces are kept small and separate (ISP).
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lms/domain/Assignment.hpp"
#include "lms/domain/Course.hpp"
#include "lms/domain/MarkRecord.hpp"
#include "lms/domain/Records.hpp"
#include "lms/domain/Submission.hpp"
#include "lms/domain/User.hpp"

namespace lms {

// ----------------------------------------------------------------------- users
struct NewUser {
  std::string fullName;
  std::string username;  // already normalised (lower case)
  std::string passwordHash;
  Role role{Role::Student};
  std::optional<Id> departmentId;
};

class IUserRepository {
 public:
  virtual ~IUserRepository() = default;
  virtual std::unique_ptr<User> findById(Id id) const = 0;               // nullptr if absent
  virtual std::unique_ptr<User> findByUsername(const std::string& username) const = 0;
  virtual std::vector<std::unique_ptr<User>> findAll() const = 0;
  virtual Id insert(const NewUser& user) = 0;
  virtual void updateProfile(Id id, const std::string& fullName, Role role,
                             std::optional<Id> departmentId) = 0;
  virtual void setActive(Id id, bool active) = 0;
  virtual void updatePasswordHash(Id id, const std::string& hash) = 0;
  virtual int countActiveAdministrators() const = 0;
  virtual int countAll() const = 0;
  /// True if the user owns courses/assignments or has enrolments/submissions.
  virtual bool hasAcademicRecords(Id id) const = 0;
};

class IDepartmentRepository {
 public:
  virtual ~IDepartmentRepository() = default;
  virtual Id insert(const std::string& name) = 0;
  virtual std::optional<Department> findById(Id id) const = 0;
  virtual std::optional<Department> findByName(const std::string& name) const = 0;
  virtual std::vector<Department> findAll() const = 0;
};

// --------------------------------------------------------------------- courses
struct NewCourse {
  std::string code;
  std::string name;
  Id lecturerId{kNoId};
  Id departmentId{kNoId};
};

class ICourseRepository {
 public:
  virtual ~ICourseRepository() = default;
  virtual Id insert(const NewCourse& course) = 0;
  virtual std::optional<Course> findById(Id id) const = 0;
  virtual std::optional<Course> findByCode(const std::string& code) const = 0;
  virtual std::vector<Course> findAll() const = 0;
  virtual std::vector<Course> findByLecturer(Id lecturerId) const = 0;
  virtual std::vector<Course> findByDepartment(Id departmentId) const = 0;
  virtual std::vector<Course> findByStudent(Id studentId) const = 0;
  virtual void update(const Course& course) = 0;  // name and lecturer
};

class IEnrolmentRepository {
 public:
  virtual ~IEnrolmentRepository() = default;
  virtual void enrol(Id studentId, Id courseId, Timestamp when) = 0;
  virtual void unenrol(Id studentId, Id courseId) = 0;
  virtual bool isEnrolled(Id studentId, Id courseId) const = 0;
  virtual std::vector<UserSummary> findStudents(Id courseId) const = 0;
  virtual bool hasSubmissions(Id studentId, Id courseId) const = 0;
};

// ----------------------------------------------------------------- assignments
struct NewAssignment {
  Id courseId{kNoId};
  Id createdBy{kNoId};
  std::string title;
  std::string description;
  Timestamp deadline{0};
  double maxScore{0.0};
};

class IAssignmentRepository {
 public:
  virtual ~IAssignmentRepository() = default;
  virtual Id insert(const NewAssignment& assignment) = 0;
  virtual std::optional<Assignment> findById(Id id) const = 0;
  virtual std::vector<Assignment> findByCourse(Id courseId) const = 0;
  virtual void updateDeadline(Id assignmentId, Timestamp deadline) = 0;
};

class IExtensionRepository {
 public:
  virtual ~IExtensionRepository() = default;
  /// Inserts, or replaces the existing extension of that student for that assignment.
  virtual void upsert(const Extension& extension) = 0;
  virtual std::optional<Extension> find(Id assignmentId, Id studentId) const = 0;
  virtual std::vector<Extension> findByAssignment(Id assignmentId) const = 0;
};

// ----------------------------------------------------------------- submissions
struct SubmissionWithStudent {
  Submission submission;
  std::string studentName;
  std::string studentUsername;
};

class ISubmissionRepository {
 public:
  virtual ~ISubmissionRepository() = default;
  /// Stores a new attempt and sets its id. Returns true on success (failures throw).
  virtual bool save(Submission& submission) = 0;
  virtual std::optional<Submission> findById(Id id) const = 0;
  virtual std::vector<Submission> findByAssignment(Id assignmentId) const = 0;  // all attempts
  virtual std::vector<Submission> findAttempts(Id studentId, Id assignmentId) const = 0;
  virtual std::optional<Submission> findLatest(Id studentId, Id assignmentId) const = 0;
  virtual std::vector<SubmissionWithStudent> findCurrentByAssignment(Id assignmentId) const = 0;
  virtual std::vector<Submission> findCurrentByStudent(Id studentId) const = 0;
  virtual int nextAttemptNo(Id studentId, Id assignmentId) const = 0;
};

// ----------------------------------------------------------------------- marks
struct ReleasedMarkRow {
  MarkRecord mark;
  Submission submission;
  Assignment assignment;
  std::string courseCode;
  std::string courseName;
};

class IMarkRepository {
 public:
  virtual ~IMarkRepository() = default;
  /// Inserts the mark if markId is 0 (and sets its id), otherwise updates it. Returns true on
  /// success (failures throw).
  virtual bool save(MarkRecord& mark) = 0;
  virtual std::optional<MarkRecord> findById(Id markId) const = 0;
  virtual std::optional<MarkRecord> findBySubmission(Id submissionId) const = 0;
  virtual std::vector<MarkRecord> findByAssignment(Id assignmentId) const = 0;
  /// Only released marks of this student - the database query itself excludes the rest.
  virtual std::vector<ReleasedMarkRow> findReleasedByStudent(Id studentId) const = 0;
};

class IModerationRepository {
 public:
  virtual ~IModerationRepository() = default;
  virtual Id insert(const ModerationDecision& decision) = 0;
  virtual std::vector<ModerationDecision> findByAssignment(Id assignmentId) const = 0;
};

struct AuditRow {
  AuditEntry entry;
  std::string changedByName;
  std::string studentName;
  Id assignmentId{kNoId};
};

class IAuditRepository {
 public:
  virtual ~IAuditRepository() = default;
  virtual Id append(const AuditEntry& entry) = 0;
  virtual std::vector<AuditRow> findByMark(Id markId) const = 0;
  virtual std::vector<AuditRow> findByAssignment(Id assignmentId) const = 0;
};

}  // namespace lms
