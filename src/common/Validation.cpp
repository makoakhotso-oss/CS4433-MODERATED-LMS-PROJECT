#include "lms/common/Validation.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#include "lms/common/Errors.hpp"

namespace lms::validation {

namespace {
bool isControl(unsigned char c) { return c < 0x20 && c != '\n' && c != '\t'; }
}  // namespace

std::string trim(const std::string& s) {
  std::size_t b = 0;
  std::size_t e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string requireText(const std::string& value, const std::string& field, std::size_t maxLen,
                        bool allowEmpty) {
  std::string t = trim(value);
  if (t.empty() && !allowEmpty) throw ValidationError(field + " must not be empty.");
  if (t.size() > maxLen) {
    throw ValidationError(field + " must not be longer than " + std::to_string(maxLen) +
                          " characters.");
  }
  if (std::any_of(t.begin(), t.end(), [](char c) { return isControl(static_cast<unsigned char>(c)); })) {
    throw ValidationError(field + " contains invalid characters.");
  }
  return t;
}

std::string normaliseUsername(const std::string& username) {
  const std::string u = toLower(trim(username));
  if (u.size() < 3 || u.size() > 32) {
    throw ValidationError("Username must be 3 to 32 characters long.");
  }
  for (char c : u) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (uc > 127 || !(std::isalnum(uc) || c == '.' || c == '_' || c == '-')) {
      throw ValidationError("Username may only contain letters, digits, '.', '_' and '-'.");
    }
  }
  return u;
}

std::string normaliseCourseCode(const std::string& code) {
  std::string c = trim(code);
  std::transform(c.begin(), c.end(), c.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  if (c.size() < 2 || c.size() > 20) {
    throw ValidationError("Course code must be 2 to 20 characters long.");
  }
  for (char ch : c) {
    const unsigned char uc = static_cast<unsigned char>(ch);
    if (uc > 127 || !(std::isalnum(uc) || ch == ' ' || ch == '-')) {
      throw ValidationError("Course code may only contain letters, digits, spaces and '-'.");
    }
  }
  return c;
}

void requirePasswordStrength(const std::string& password) {
  if (password.size() < 8) throw ValidationError("Password must be at least 8 characters long.");
  if (password.size() > 128) throw ValidationError("Password must not be longer than 128 characters.");
  const bool letter = std::any_of(password.begin(), password.end(),
                                  [](unsigned char c) { return std::isalpha(c) != 0; });
  const bool digit = std::any_of(password.begin(), password.end(),
                                 [](unsigned char c) { return std::isdigit(c) != 0; });
  if (!letter || !digit) {
    throw ValidationError("Password must contain at least one letter and one digit.");
  }
}

double roundMark(double value) { return std::round(value * 100.0) / 100.0; }

double requireScore(double value, double maxScore, const std::string& field) {
  if (!std::isfinite(value)) throw ValidationError(field + " must be a number.");
  if (value < 0.0 || value > maxScore) {
    std::ostringstream os;
    os << field << " must be between 0 and " << maxScore << ".";
    throw ValidationError(os.str());
  }
  return roundMark(value);
}

}  // namespace lms::validation
