#pragma once
// Input validation. Every value that comes from a person is checked here (or in a
// domain constructor) before it is used, and a ValidationError explains what is wrong.
#include <string>

namespace lms::validation {

std::string trim(const std::string& s);
std::string toLower(std::string s);

/// Trims, then requires 1..maxLen characters (or 0..maxLen if allowEmpty). No control characters.
std::string requireText(const std::string& value, const std::string& field, std::size_t maxLen,
                        bool allowEmpty = false);

/// Lower-cases and checks: 3-32 characters from [a-z0-9._-].
std::string normaliseUsername(const std::string& username);

/// Upper-cases and checks: 2-20 characters from [A-Z0-9 -].
std::string normaliseCourseCode(const std::string& code);

/// 8-128 characters with at least one letter and one digit.
void requirePasswordStrength(const std::string& password);

/// Rounds to 2 decimal places (so 0.1 * 3 * 100 is 30.00, not 30.000000000000004).
double roundMark(double value);

/// Finite number between 0 and maxScore (inclusive); returned rounded to 2 decimal places.
double requireScore(double value, double maxScore, const std::string& field);

}  // namespace lms::validation
