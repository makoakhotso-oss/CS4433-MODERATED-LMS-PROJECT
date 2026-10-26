#include "lms/common/Logger.hpp"

#include "lms/common/Time.hpp"

namespace lms {

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::open(const std::string& path) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_.is_open()) file_.close();
  file_.open(path, std::ios::app);
}

void Logger::write(const char* level, const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!file_.is_open()) return;
  file_ << toDbString(SystemClock().now()) << " UTC [" << level << "] " << message << '\n';
  file_.flush();
}

}  // namespace lms
