#include "lms/ui/Console.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>

#include "lms/common/Time.hpp"
#include "lms/common/Validation.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace lms {

namespace {

std::string clip(const std::string& s, std::size_t width) {
  if (s.size() <= width) return s;
  return s.substr(0, width - 3) + "...";
}

// RAII helper that switches terminal echo off and restores it afterwards.
class EchoOff {
 public:
  EchoOff() {
#ifdef _WIN32
    handle_ = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(handle_, &mode)) {
      saved_ = mode;
      active_ = SetConsoleMode(handle_, mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT)) != 0;
    }
#else
    if (isatty(STDIN_FILENO) != 0 && tcgetattr(STDIN_FILENO, &saved_) == 0) {
      termios t = saved_;
      t.c_lflag &= ~static_cast<tcflag_t>(ECHO);
      active_ = tcsetattr(STDIN_FILENO, TCSANOW, &t) == 0;
    }
#endif
  }
  ~EchoOff() {
    if (!active_) return;
#ifdef _WIN32
    SetConsoleMode(handle_, saved_);
#else
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
#endif
  }
  bool active() const { return active_; }
  EchoOff(const EchoOff&) = delete;
  EchoOff& operator=(const EchoOff&) = delete;

 private:
  bool active_{false};
#ifdef _WIN32
  HANDLE handle_{nullptr};
  DWORD saved_{0};
#else
  termios saved_{};
#endif
};

}  // namespace

void Console::printTable(const std::vector<std::string>& headers,
                         const std::vector<std::vector<std::string>>& rows) {
  constexpr std::size_t kMaxWidth = 38;
  std::vector<std::size_t> widths(headers.size());
  for (std::size_t c = 0; c < headers.size(); ++c) widths[c] = headers[c].size();
  for (const auto& row : rows) {
    for (std::size_t c = 0; c < headers.size() && c < row.size(); ++c) {
      widths[c] = std::max(widths[c], std::min(row[c].size(), kMaxWidth));
    }
  }
  auto line = [&](const std::vector<std::string>& cells) {
    out_ << "  ";
    for (std::size_t c = 0; c < headers.size(); ++c) {
      const std::string text = c < cells.size() ? clip(cells[c], kMaxWidth) : "";
      out_ << std::left << std::setw(static_cast<int>(widths[c])) << text << "  ";
    }
    out_ << '\n';
  };
  line(headers);
  out_ << "  ";
  for (std::size_t c = 0; c < headers.size(); ++c) out_ << std::string(widths[c], '-') << "  ";
  out_ << '\n';
  for (const auto& row : rows) line(row);
  if (rows.empty()) out_ << "  (none)\n";
}

std::string Console::ask(const std::string& prompt) {
  out_ << prompt << std::flush;
  std::string line;
  if (!std::getline(in_, line)) throw InputClosed();
  return validation::trim(line);
}

std::string Console::askPassword(const std::string& prompt) {
  out_ << prompt << std::flush;
  std::string line;
  bool ok = false;
  if (&in_ == &std::cin) {
    EchoOff guard;
    ok = static_cast<bool>(std::getline(in_, line));
    if (guard.active()) out_ << '\n';
  } else {
    ok = static_cast<bool>(std::getline(in_, line));
  }
  if (!ok) throw InputClosed();
  if (!line.empty() && line.back() == '\r') line.pop_back();
  return line;  // not trimmed: spaces may be part of a password
}

std::optional<std::string> Console::askNewPassword() {
  const std::string first = askPassword("  New password (min 8 characters, letters and digits): ");
  const std::string second = askPassword("  Repeat the new password: ");
  if (first != second) {
    printError("The passwords do not match.");
    return std::nullopt;
  }
  return first;
}

std::optional<Id> Console::askId(const std::string& prompt) {
  const std::string text = ask(prompt);
  if (text.empty()) return std::nullopt;
  char* end = nullptr;
  const long long v = std::strtoll(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0' || v <= 0) {
    printError("Please enter a positive whole number.");
    return std::nullopt;
  }
  return static_cast<Id>(v);
}

std::optional<int> Console::askChoice(const std::string& prompt) {
  const std::string text = ask(prompt);
  if (text.empty()) return std::nullopt;
  char* end = nullptr;
  const long v = std::strtol(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0' || v < 0 || v > 999) {
    printError("Please enter a menu number.");
    return std::nullopt;
  }
  return static_cast<int>(v);
}

std::optional<double> Console::askNumber(const std::string& prompt) {
  const std::string text = ask(prompt);
  if (text.empty()) return std::nullopt;
  char* end = nullptr;
  const double v = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(v)) {
    printError("Please enter a number.");
    return std::nullopt;
  }
  return v;
}

std::optional<Timestamp> Console::askDateTime(const std::string& prompt) {
  const std::string text = ask(prompt + " (YYYY-MM-DD HH:MM): ");
  if (text.empty()) return std::nullopt;
  const std::optional<Timestamp> t = parseLocalDateTime(text);
  if (!t) printError("Please use the format YYYY-MM-DD HH:MM, for example 2026-11-30 17:00.");
  return t;
}

bool Console::confirm(const std::string& prompt) {
  const std::string a = validation::toLower(ask(prompt + " [y/N]: "));
  return a == "y" || a == "yes";
}

}  // namespace lms
