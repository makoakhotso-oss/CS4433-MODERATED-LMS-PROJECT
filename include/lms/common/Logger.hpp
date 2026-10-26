#pragma once
// Minimal file logger for security events (logins, releases) and unexpected errors.
// It never records passwords. If no file is opened, messages are discarded.
#include <fstream>
#include <mutex>
#include <string>

namespace lms {

class Logger {
 public:
  static Logger& instance();
  void open(const std::string& path);
  void info(const std::string& message) { write("INFO", message); }
  void warn(const std::string& message) { write("WARN", message); }
  void error(const std::string& message) { write("ERROR", message); }

 private:
  void write(const char* level, const std::string& message);
  std::mutex mutex_;
  std::ofstream file_;
};

}  // namespace lms
