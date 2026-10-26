#include "lms/common/Time.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

#include "lms/common/Errors.hpp"

namespace lms {

namespace {

// Civil-calendar conversions (Howard Hinnant's public-domain algorithms). They avoid
// timegm()/_mkgmtime() so the code behaves identically on Windows and Linux.
std::int64_t daysFromCivil(int y, int m, int d) noexcept {
  y -= m <= 2 ? 1 : 0;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<std::int64_t>(era) * 146097 + doe - 719468;
}

void civilFromDays(std::int64_t z, int& y, int& m, int& d) noexcept {
  z += 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int doe = static_cast<int>(z - era * 146097);
  const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = yoe + static_cast<int>(era) * 400 + (m <= 2 ? 1 : 0);
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (m == 2 && isLeap(y)) ? 29 : kDays[m - 1];
}

bool validFields(int y, int mo, int d, int h, int mi, int s) {
  if (y < 2000 || y > 2100 || mo < 1 || mo > 12) return false;
  if (d < 1 || d > daysInMonth(y, mo)) return false;
  return h >= 0 && h <= 23 && mi >= 0 && mi <= 59 && s >= 0 && s <= 59;
}

std::tm localTm(Timestamp ts) {
  std::tm out{};
  const std::time_t t = static_cast<std::time_t>(ts);
#ifdef _WIN32
  localtime_s(&out, &t);
#else
  localtime_r(&t, &out);
#endif
  return out;
}

}  // namespace

Timestamp SystemClock::now() const {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

Timestamp makeUtc(int year, int month, int day, int hour, int minute, int second) {
  if (!validFields(year, month, day, hour, minute, second)) {
    throw ValidationError("Invalid date/time.");
  }
  return daysFromCivil(year, month, day) * kSecondsPerDay + hour * 3600 + minute * 60 + second;
}

std::string toDbString(Timestamp ts) {
  std::int64_t days = ts / kSecondsPerDay;
  std::int64_t rem = ts % kSecondsPerDay;
  if (rem < 0) {
    rem += kSecondsPerDay;
    --days;
  }
  int y = 0, m = 0, d = 0;
  civilFromDays(days, y, m, d);
  char buf[64];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d", y, m, d,
                static_cast<int>(rem / 3600), static_cast<int>((rem % 3600) / 60),
                static_cast<int>(rem % 60));
  return buf;
}

Timestamp fromDbString(const std::string& text) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  int consumed = 0;
  const int n = std::sscanf(text.c_str(), "%4d-%2d-%2d %2d:%2d:%2d%n", &y, &mo, &d, &h, &mi, &s,
                            &consumed);
  if (n != 6 || consumed != static_cast<int>(text.size()) || !validFields(y, mo, d, h, mi, s)) {
    throw ValidationError("Malformed date/time value: '" + text + "'");
  }
  return makeUtc(y, mo, d, h, mi, s);
}

std::string toLocalString(Timestamp ts) {
  const std::tm t = localTm(ts);
  char buf[64];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d", t.tm_year + 1900, t.tm_mon + 1,
                t.tm_mday, t.tm_hour, t.tm_min);
  return buf;
}

std::optional<Timestamp> parseLocalDateTime(const std::string& text) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  int consumed = 0;
  const int n = std::sscanf(text.c_str(), "%4d-%2d-%2d %2d:%2d%n", &y, &mo, &d, &h, &mi, &consumed);
  if (n != 5 || consumed != static_cast<int>(text.size()) || !validFields(y, mo, d, h, mi, 0)) {
    return std::nullopt;
  }
  std::tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_isdst = -1;
  const std::time_t result = std::mktime(&t);
  if (result == static_cast<std::time_t>(-1)) return std::nullopt;
  // Reject local times that do not exist (e.g. inside a daylight-saving gap).
  const std::tm back = localTm(static_cast<Timestamp>(result));
  if (back.tm_hour != h || back.tm_min != mi || back.tm_mday != d) return std::nullopt;
  return static_cast<Timestamp>(result);
}

int wholeDaysLate(Timestamp deadline, Timestamp submittedAt) {
  const Timestamp diff = submittedAt - deadline;
  if (diff <= 0) return 0;
  return static_cast<int>((diff + kSecondsPerDay - 1) / kSecondsPerDay);
}

}  // namespace lms
