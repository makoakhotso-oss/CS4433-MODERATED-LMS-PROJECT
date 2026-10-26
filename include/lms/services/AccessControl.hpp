#pragma once
// Authentication & Access Control component (Component Diagram): the single place where
// "may this user do this?" is decided. Services call it before doing any work, so the
// rules hold no matter which user interface is in front (FR-03, NFR-04).
#include "lms/domain/User.hpp"

namespace lms {

class AccessControl {
 public:
  /// Throws PermissionDeniedError unless the user is logged in, active and holds `permission`.
  static void require(const User& actor, Permission permission);
  static bool can(const User& actor, Permission permission) noexcept;
};

}  // namespace lms
