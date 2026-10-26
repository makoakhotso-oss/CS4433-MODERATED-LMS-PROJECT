#pragma once
// Low-level console input/output helpers. Input is always read as whole lines and
// validated before use, so typing letters where a number is expected never crashes.
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "lms/common/Types.hpp"

namespace lms {

/// Thrown when the input stream ends (Ctrl-D / Ctrl-Z); lets the program exit cleanly.
class InputClosed : public std::exception {
 public:
  const char* what() const noexcept override { return "Input closed"; }
};

class Console {
 public:
  Console(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

  void print(const std::string& text = "") { out_ << text << '\n'; }
  void printError(const std::string& text) { out_ << "  ! " << text << '\n'; }
  void printTable(const std::vector<std::string>& headers,
                  const std::vector<std::vector<std::string>>& rows);

  /// Prompts and returns the trimmed line. Throws InputClosed at end of input.
  std::string ask(const std::string& prompt);
  /// Like ask() but turns terminal echo off when reading from a real terminal.
  std::string askPassword(const std::string& prompt);
  /// Asks twice and returns the password only if both entries match.
  std::optional<std::string> askNewPassword();
  /// Empty answer (or invalid) returns nullopt after telling the user what was wrong.
  std::optional<Id> askId(const std::string& prompt);
  std::optional<double> askNumber(const std::string& prompt);
  /// Menu choice: a whole number from 0 to 999; blank or invalid input returns nullopt.
  std::optional<int> askChoice(const std::string& prompt);
  std::optional<Timestamp> askDateTime(const std::string& prompt);  // local time
  bool confirm(const std::string& prompt);

 private:
  std::istream& in_;
  std::ostream& out_;
};

}  // namespace lms
