#pragma once
// SQLite implementations of the repository interfaces. All SQL lives in src/data/ and uses
// parameterised statements only.
#include "lms/data/Database.hpp"
#include "lms/data/IRepositories.hpp"

namespace lms {

class SQLiteUserRepository final : public IUserRepository {
 public:
  explicit SQLiteUserRepository(Database& db) : db_(db) {}
  std::unique_ptr<User> findById(Id id) const override;
  std::unique_ptr<User> findByUsername(const std::string& username) const override;
  std::vector<std::unique_ptr<User>> findAll() const override;
  Id insert(const NewUser& user) override;
  void updateProfile(Id id, const std::string& fullName, Role role,
                     std::optional<Id> departmentId) override;
  void setActive(Id id, bool active) override;
  void updatePasswordHash(Id id, const std::string& hash) override;
  int countActiveAdministrators() const override;
  int countAll() const override;
  bool hasAcademicRecords(Id id) const override;

 private:
  Database& db_;
};

class SQLiteDepartmentRepository final : public IDepartmentRepository {
 public:
  explicit SQLiteDepartmentRepository(Database& db) : db_(db) {}
  Id insert(const std::string& name) override;
  std::optional<Department> findById(Id id) const override;
  std::optional<Department> findByName(const std::string& name) const override;
  std::vector<Department> findAll() const override;

 private:
  Database& db_;
};

class SQLiteCourseRepository final : public ICourseRepository {
 public:
  explicit SQLiteCourseRepository(Database& db) : db_(db) {}
  Id insert(const NewCourse& course) override;
  std::optional<Course> findById(Id id) const override;
  std::optional<Course> findByCode(const std::string& code) const override;
  std::vector<Course> findAll() const override;
  std::vector<Course> findByLecturer(Id lecturerId) const override;
  std::vector<Course> findByDepartment(Id departmentId) const override;
  std::vector<Course> findByStudent(Id studentId) const override;
  void update(const Course& course) override;

 private:
  Database& db_;
};

class SQLiteEnrolmentRepository final : public IEnrolmentRepository {
 public:
  explicit SQLiteEnrolmentRepository(Database& db) : db_(db) {}
  void enrol(Id studentId, Id courseId, Timestamp when) override;
  void unenrol(Id studentId, Id courseId) override;
  bool isEnrolled(Id studentId, Id courseId) const override;
  std::vector<UserSummary> findStudents(Id courseId) const override;
  bool hasSubmissions(Id studentId, Id courseId) const override;

 private:
  Database& db_;
};

class SQLiteAssignmentRepository final : public IAssignmentRepository {
 public:
  explicit SQLiteAssignmentRepository(Database& db) : db_(db) {}
  Id insert(const NewAssignment& assignment) override;
  std::optional<Assignment> findById(Id id) const override;
  std::vector<Assignment> findByCourse(Id courseId) const override;
  void updateDeadline(Id assignmentId, Timestamp deadline) override;

 private:
  Database& db_;
};

class SQLiteExtensionRepository final : public IExtensionRepository {
 public:
  explicit SQLiteExtensionRepository(Database& db) : db_(db) {}
  void upsert(const Extension& extension) override;
  std::optional<Extension> find(Id assignmentId, Id studentId) const override;
  std::vector<Extension> findByAssignment(Id assignmentId) const override;

 private:
  Database& db_;
};

class SQLiteSubmissionRepository final : public ISubmissionRepository {
 public:
  explicit SQLiteSubmissionRepository(Database& db) : db_(db) {}
  bool save(Submission& submission) override;
  std::optional<Submission> findById(Id id) const override;
  std::vector<Submission> findByAssignment(Id assignmentId) const override;
  std::vector<Submission> findAttempts(Id studentId, Id assignmentId) const override;
  std::optional<Submission> findLatest(Id studentId, Id assignmentId) const override;
  std::vector<SubmissionWithStudent> findCurrentByAssignment(Id assignmentId) const override;
  std::vector<Submission> findCurrentByStudent(Id studentId) const override;
  int nextAttemptNo(Id studentId, Id assignmentId) const override;

 private:
  Database& db_;
};

class SQLiteMarkRepository final : public IMarkRepository {
 public:
  explicit SQLiteMarkRepository(Database& db) : db_(db) {}
  bool save(MarkRecord& mark) override;
  std::optional<MarkRecord> findById(Id markId) const override;
  std::optional<MarkRecord> findBySubmission(Id submissionId) const override;
  std::vector<MarkRecord> findByAssignment(Id assignmentId) const override;
  std::vector<ReleasedMarkRow> findReleasedByStudent(Id studentId) const override;

 private:
  Database& db_;
};

class SQLiteModerationRepository final : public IModerationRepository {
 public:
  explicit SQLiteModerationRepository(Database& db) : db_(db) {}
  Id insert(const ModerationDecision& decision) override;
  std::vector<ModerationDecision> findByAssignment(Id assignmentId) const override;

 private:
  Database& db_;
};

class SQLiteAuditRepository final : public IAuditRepository {
 public:
  explicit SQLiteAuditRepository(Database& db) : db_(db) {}
  Id append(const AuditEntry& entry) override;
  std::vector<AuditRow> findByMark(Id markId) const override;
  std::vector<AuditRow> findByAssignment(Id assignmentId) const override;

 private:
  Database& db_;
};

}  // namespace lms
