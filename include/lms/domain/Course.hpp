#pragma once
#include <string>

#include "lms/common/Types.hpp"

namespace lms {

class Course {
 public:
  Course(Id id, std::string code, std::string name, Id lecturerId, Id departmentId)
      : courseId_(id),
        courseCode_(std::move(code)),
        courseName_(std::move(name)),
        lecturerId_(lecturerId),
        departmentId_(departmentId) {}

  Id courseId() const noexcept { return courseId_; }
  const std::string& courseCode() const noexcept { return courseCode_; }
  const std::string& courseName() const noexcept { return courseName_; }
  Id lecturerId() const noexcept { return lecturerId_; }
  Id departmentId() const noexcept { return departmentId_; }
  bool isTaughtBy(Id userId) const noexcept { return lecturerId_ == userId; }

  void rename(std::string newName) { courseName_ = std::move(newName); }
  void assignLecturer(Id lecturerId) noexcept { lecturerId_ = lecturerId; }

 private:
  Id courseId_;
  std::string courseCode_;
  std::string courseName_;
  Id lecturerId_;
  Id departmentId_;
};

}  // namespace lms
