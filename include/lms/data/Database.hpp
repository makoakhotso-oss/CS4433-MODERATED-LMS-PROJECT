#pragma once
// Thin RAII wrappers around the SQLite C API. This is the ONLY place where sqlite3_*
// functions are called; the rest of the program uses Database, Statement and Transaction.
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "lms/data/IUnitOfWork.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace lms {

class Database {
 public:
  /// Opens (or creates) the database file; ":memory:" gives a private temporary database.
  /// Enables foreign keys and full synchronous writes (safe against power loss).
  explicit Database(const std::string& path);
  ~Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  /// Creates the schema on first use; rejects databases written by a newer version.
  void initialiseSchema();

  /// Runs fixed SQL text that contains no user input (schema, pragmas, BEGIN/COMMIT).
  void exec(const char* fixedSql);

  sqlite3* handle() const noexcept { return db_; }
  std::int64_t lastInsertRowId() const noexcept;
  int changes() const noexcept;

 private:
  friend class Transaction;
  sqlite3* db_{nullptr};
  int txDepth_{0};
};

/// A prepared statement with bound parameters. SQL text passed in must be constant:
/// all values are bound with the bind*() calls (never concatenated) - this is what
/// prevents SQL injection.
class Statement {
 public:
  Statement(Database& db, const std::string& sql);
  ~Statement();
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  // Parameter indexes are 1-based; column indexes are 0-based (SQLite convention).
  Statement& bindInt(int index, std::int64_t value);
  Statement& bindDouble(int index, double value);
  Statement& bindText(int index, const std::string& value);
  Statement& bindNull(int index);
  Statement& bindOptionalInt(int index, const std::optional<std::int64_t>& value);

  /// Advances to the next row. Returns true if a row is available, false when finished.
  bool step();
  /// Executes a statement that returns no rows (INSERT/UPDATE/DELETE).
  void run();

  std::int64_t columnInt(int col) const;
  double columnDouble(int col) const;
  std::string columnText(int col) const;
  bool columnIsNull(int col) const;
  std::optional<std::int64_t> columnOptionalInt(int col) const;

 private:
  Database& db_;
  sqlite3_stmt* stmt_{nullptr};
};

/// Transaction scope: rolls back automatically unless commit() is called. Nested
/// Transaction objects become SAVEPOINTs, so services can call each other safely.
class Transaction final : public ITransaction {
 public:
  explicit Transaction(Database& db);
  ~Transaction() override;
  void commit() override;

 private:
  Database& db_;
  int depth_;
  bool done_{false};
};

class SqliteUnitOfWork final : public IUnitOfWork {
 public:
  explicit SqliteUnitOfWork(Database& db) : db_(db) {}
  std::unique_ptr<ITransaction> begin() override { return std::make_unique<Transaction>(db_); }

 private:
  Database& db_;
};

}  // namespace lms
