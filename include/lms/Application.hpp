#pragma once
// Composition root: builds the objects of every layer and wires the dependencies
// (interfaces are given their SQLite implementations here and nowhere else).
#include <string>

#include "lms/common/Time.hpp"
#include "lms/data/Database.hpp"
#include "lms/data/FileStorage.hpp"
#include "lms/data/SQLiteRepositories.hpp"
#include "lms/security/PasswordHasher.hpp"
#include "lms/services/AssignmentService.hpp"
#include "lms/services/AuditLog.hpp"
#include "lms/services/AuthService.hpp"
#include "lms/services/CourseService.hpp"
#include "lms/services/MarkingService.hpp"
#include "lms/services/ModerationService.hpp"
#include "lms/services/ResultsService.hpp"
#include "lms/services/StandardLatePenaltyPolicy.hpp"
#include "lms/services/SubmissionService.hpp"
#include "lms/services/UserService.hpp"

namespace lms {

struct AppConfig {
  std::string databasePath{"lms.db"};
  std::string dataDirectory{"lms_data"};  // uploaded submission files
  PasswordHasher::Cost hashCost{PasswordHasher::Cost::Interactive};
  SubmissionOptions submissionOptions{};
};

class Application {
 public:
  Application(const AppConfig& config, const IClock& clock);

  const IClock& clock() const noexcept { return clock_; }
  AuthService& auth() noexcept { return auth_; }
  UserService& users() noexcept { return userService_; }
  CourseService& courses() noexcept { return courseService_; }
  AssignmentService& assignments() noexcept { return assignmentService_; }
  SubmissionService& submissions() noexcept { return submissionService_; }
  MarkingService& marking() noexcept { return markingService_; }
  ModerationService& moderation() noexcept { return moderationService_; }
  ResultsService& results() noexcept { return resultsService_; }
  AuditLog& auditLog() noexcept { return auditLog_; }
  IUserRepository& userRepository() noexcept { return userRepo_; }
  Database& database() noexcept { return db_; }

 private:
  const IClock& clock_;
  Database db_;
  SQLiteUserRepository userRepo_;
  SQLiteDepartmentRepository departmentRepo_;
  SQLiteCourseRepository courseRepo_;
  SQLiteEnrolmentRepository enrolmentRepo_;
  SQLiteAssignmentRepository assignmentRepo_;
  SQLiteExtensionRepository extensionRepo_;
  SQLiteSubmissionRepository submissionRepo_;
  SQLiteMarkRepository markRepo_;
  SQLiteModerationRepository moderationRepo_;
  SQLiteAuditRepository auditRepo_;
  SqliteUnitOfWork unitOfWork_;
  FileStorage storage_;
  PasswordHasher hasher_;
  StandardLatePenaltyPolicy penaltyPolicy_;
  AuditLog auditLog_;
  AuthService auth_;
  UserService userService_;
  CourseService courseService_;
  AssignmentService assignmentService_;
  SubmissionService submissionService_;
  MarkingService markingService_;
  ModerationService moderationService_;
  ResultsService resultsService_;
};

}  // namespace lms
