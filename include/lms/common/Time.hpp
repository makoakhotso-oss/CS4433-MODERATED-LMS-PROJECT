#pragma once
// Time helpers. All times are stored in the database as UTC text
// ("YYYY-MM-DD HH:MM:SS", the same format as SQLite's CURRENT_TIMESTAMP) and
// shown to users in the computer's local time zone.
#include <optional>
#include <string>

#include "lms/common/Types.hpp"

namespace lms {

/// Abstraction of "now" so that deadline logic can be tested deterministically.
class IClock {
 public:
  virtual ~IClock() = default;
  virtual Timestamp now() const = 0;
};

class SystemClock final : public IClock {
 public:
  Timestamp now() const override;
};

/// Clock whose time is set by hand (used by the automated tests).
class FixedClock final : public IClock {
 public:
  explicit FixedClock(Timestamp t = 0) : t_(t) {}
  Timestamp now() const override { return t_; }
  void set(Timestamp t) { t_ = t; }
  void advance(Timestamp seconds) { t_ += seconds; }

 private:
  Timestamp t_;
};

std::string toDbString(Timestamp ts);                    // UTC "YYYY-MM-DD HH:MM:SS"
Timestamp fromDbString(const std::string& text);         // throws ValidationError if malformed
std::string toLocalString(Timestamp ts);                 // local "YYYY-MM-DD HH:MM"
std::optional<Timestamp> parseLocalDateTime(const std::string& text);  // local "YYYY-MM-DD HH:MM"
Timestamp makeUtc(int year, int month, int day, int hour = 0, int minute = 0, int second = 0);

/// Whole days late, rounded up: 0 if on time (or exactly at the deadline),
/// 1 for anything up to 24 hours late, 2 for up to 48 hours, and so on.
int wholeDaysLate(Timestamp deadline, Timestamp submittedAt);

}  // namespace lms
