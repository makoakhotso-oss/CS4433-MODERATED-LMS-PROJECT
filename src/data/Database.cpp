#include "lms/data/Database.hpp"

#include <sqlite3.h>

#include "lms/common/Errors.hpp"
#include "lms/data/Schema.hpp"

namespace lms {

namespace {

[[noreturn]] void throwSqlite(sqlite3* db, int rc, const std::string& context) {
  const bool constraint = (rc & 0xFF) == SQLITE_CONSTRAINT;
  std::string message = context + ": " + (db != nullptr ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
  throw DatabaseError(message, constraint);
}

}  // namespace

Database::Database(const std::string& path) {
  const int rc = sqlite3_open_v2(path.c_str(), &db_,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                                 nullptr);
  if (rc != SQLITE_OK) {
    const std::string msg = db_ != nullptr ? sqlite3_errmsg(db_) : sqlite3_errstr(rc);
    if (db_ != nullptr) sqlite3_close(db_);
    db_ = nullptr;
    throw DatabaseError("Cannot open database '" + path + "': " + msg);
  }
  sqlite3_extended_result_codes(db_, 1);
  sqlite3_busy_timeout(db_, 5000);
  try {
    exec("PRAGMA foreign_keys = ON");
    exec("PRAGMA synchronous = FULL");
    Statement check(*this, "PRAGMA foreign_keys");
    if (!check.step() || check.columnInt(0) != 1) {
      throw DatabaseError("This SQLite build does not support foreign keys.");
    }
  } catch (...) {
    sqlite3_close(db_);
    db_ = nullptr;
    throw;
  }
}

Database::~Database() {
  if (db_ != nullptr) sqlite3_close(db_);
}

void Database::exec(const char* fixedSql) {
  char* err = nullptr;
  const int rc = sqlite3_exec(db_, fixedSql, nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    const std::string msg = err != nullptr ? err : sqlite3_errstr(rc);
    sqlite3_free(err);
    throw DatabaseError("Database error: " + msg, (rc & 0xFF) == SQLITE_CONSTRAINT);
  }
}

std::int64_t Database::lastInsertRowId() const noexcept { return sqlite3_last_insert_rowid(db_); }
int Database::changes() const noexcept { return sqlite3_changes(db_); }

void Database::initialiseSchema() {
  int version = 0;
  {
    Statement st(*this, "PRAGMA user_version");
    if (st.step()) version = static_cast<int>(st.columnInt(0));
  }
  if (version > kSchemaVersion) {
    throw DatabaseError("The database was created by a newer version of the LMS.");
  }
  if (version == kSchemaVersion) return;
  Transaction tx(*this);
  exec(schemaSql());
  exec(("PRAGMA user_version = " + std::to_string(kSchemaVersion)).c_str());
  tx.commit();
}

// ------------------------------------------------------------------ Statement

Statement::Statement(Database& db, const std::string& sql) : db_(db) {
  const int rc = sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &stmt_, nullptr);
  if (rc != SQLITE_OK) throwSqlite(db.handle(), rc, "Cannot prepare statement");
}

Statement::~Statement() {
  if (stmt_ != nullptr) sqlite3_finalize(stmt_);
}

Statement& Statement::bindInt(int index, std::int64_t value) {
  const int rc = sqlite3_bind_int64(stmt_, index, value);
  if (rc != SQLITE_OK) throwSqlite(db_.handle(), rc, "Cannot bind integer");
  return *this;
}

Statement& Statement::bindDouble(int index, double value) {
  const int rc = sqlite3_bind_double(stmt_, index, value);
  if (rc != SQLITE_OK) throwSqlite(db_.handle(), rc, "Cannot bind number");
  return *this;
}

Statement& Statement::bindText(int index, const std::string& value) {
  const int rc = sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()),
                                   SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) throwSqlite(db_.handle(), rc, "Cannot bind text");
  return *this;
}

Statement& Statement::bindNull(int index) {
  const int rc = sqlite3_bind_null(stmt_, index);
  if (rc != SQLITE_OK) throwSqlite(db_.handle(), rc, "Cannot bind null");
  return *this;
}

Statement& Statement::bindOptionalInt(int index, const std::optional<std::int64_t>& value) {
  return value ? bindInt(index, *value) : bindNull(index);
}

bool Statement::step() {
  const int rc = sqlite3_step(stmt_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  throwSqlite(db_.handle(), rc, "Database operation failed");
}

void Statement::run() {
  while (step()) {
  }
}

std::int64_t Statement::columnInt(int col) const { return sqlite3_column_int64(stmt_, col); }
double Statement::columnDouble(int col) const { return sqlite3_column_double(stmt_, col); }

std::string Statement::columnText(int col) const {
  const unsigned char* text = sqlite3_column_text(stmt_, col);
  if (text == nullptr) return {};
  return std::string(reinterpret_cast<const char*>(text),
                     static_cast<std::size_t>(sqlite3_column_bytes(stmt_, col)));
}

bool Statement::columnIsNull(int col) const { return sqlite3_column_type(stmt_, col) == SQLITE_NULL; }

std::optional<std::int64_t> Statement::columnOptionalInt(int col) const {
  if (columnIsNull(col)) return std::nullopt;
  return columnInt(col);
}

// ---------------------------------------------------------------- Transaction

Transaction::Transaction(Database& db) : db_(db), depth_(db.txDepth_) {
  if (depth_ == 0) {
    db_.exec("BEGIN IMMEDIATE");
  } else {
    db_.exec(("SAVEPOINT lms_sp" + std::to_string(depth_)).c_str());
  }
  ++db_.txDepth_;
}

void Transaction::commit() {
  if (done_) return;
  if (depth_ == 0) {
    db_.exec("COMMIT");
  } else {
    db_.exec(("RELEASE lms_sp" + std::to_string(depth_)).c_str());
  }
  --db_.txDepth_;
  done_ = true;
}

Transaction::~Transaction() {
  if (done_) return;
  try {
    if (depth_ == 0) {
      db_.exec("ROLLBACK");
    } else {
      db_.exec(("ROLLBACK TO lms_sp" + std::to_string(depth_)).c_str());
      db_.exec(("RELEASE lms_sp" + std::to_string(depth_)).c_str());
    }
  } catch (...) {
    // A destructor must not throw; SQLite has already discarded the transaction on error.
  }
  --db_.txDepth_;
}

}  // namespace lms
