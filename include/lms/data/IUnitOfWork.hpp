#pragma once
// Lets services group several repository calls into one atomic unit without depending
// on SQLite (Dependency Inversion).
#include <memory>

namespace lms {

class ITransaction {
 public:
  virtual ~ITransaction() = default;  // rolls back if commit() was not called
  virtual void commit() = 0;
};

class IUnitOfWork {
 public:
  virtual ~IUnitOfWork() = default;
  virtual std::unique_ptr<ITransaction> begin() = 0;
};

}  // namespace lms
