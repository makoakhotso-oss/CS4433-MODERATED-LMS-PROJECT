#include "lms/Application.hpp"

namespace lms {

namespace {
// The database must have its schema before any repository runs a query.
Database& initialised(Database& db) {
  db.initialiseSchema();
  return db;
}
}  // namespace

Application::Application(const AppConfig& config, const IClock& clock)
    : clock_(clock),
      db_(config.databasePath),
      userRepo_(initialised(db_)),
      departmentRepo_(db_),
      courseRepo_(db_),
      enrolmentRepo_(db_),
      assignmentRepo_(db_),
      extensionRepo_(db_),
      submissionRepo_(db_),
      markRepo_(db_),
      moderationRepo_(db_),
      auditRepo_(db_),
      unitOfWork_(db_),
      storage_(config.dataDirectory),
      hasher_(config.hashCost),
      penaltyPolicy_(),
      auditLog_(auditRepo_, assignmentRepo_, courseRepo_),
      auth_(userRepo_, hasher_, clock_),
      userService_(userRepo_, departmentRepo_, hasher_),
      courseService_(courseRepo_, enrolmentRepo_, userRepo_, clock_),
      assignmentService_(assignmentRepo_, courseRepo_, enrolmentRepo_, extensionRepo_,
                         submissionRepo_, markRepo_, userRepo_, clock_),
      submissionService_(submissionRepo_, assignmentRepo_, enrolmentRepo_, extensionRepo_,
                         markRepo_, penaltyPolicy_, storage_, unitOfWork_, clock_,
                         config.submissionOptions),
      markingService_(submissionRepo_, markRepo_, assignmentRepo_, courseRepo_, extensionRepo_,
                      penaltyPolicy_, auditLog_, unitOfWork_, clock_),
      moderationService_(submissionRepo_, markRepo_, assignmentRepo_, courseRepo_, extensionRepo_,
                         moderationRepo_, penaltyPolicy_, unitOfWork_, clock_),
      resultsService_(markRepo_, extensionRepo_) {}

}  // namespace lms
