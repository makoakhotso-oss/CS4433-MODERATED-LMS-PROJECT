#include "lms/services/AccessControl.hpp"

#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"

namespace lms {

bool AccessControl::can(const User& actor, Permission permission) noexcept {
  try {
    return actor.isLoggedIn() && actor.isActive() && actor.hasPermission(permission);
  } catch (...) {
    return false;
  }
}

void AccessControl::require(const User& actor, Permission permission) {
  if (!can(actor, permission)) {
    Logger::instance().warn("Access denied: user '" + actor.username() + "' (" + actor.getRole() +
                            ") attempted " + toString(permission));
    throw PermissionDeniedError("You are not allowed to perform this action.");
  }
}

}  // namespace lms
