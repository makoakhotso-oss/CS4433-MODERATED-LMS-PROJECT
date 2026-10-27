#include "lms/ui/ConsoleApp.hpp"

#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"
#include "lms/common/Time.hpp"

namespace lms {


// ------------------------------------------------------------------ framework

void ConsoleApp::guarded(const std::function<void()>& action) {
  try {
    action();
  } catch (const InputClosed&) {
    throw;
  } catch (const LmsError& e) {
    c_.printError(e.what());
  } catch (const std::exception& e) {
    Logger::instance().error(std::string("Unexpected error: ") + e.what());
    c_.printError("An unexpected error occurred. The action was not completed.");
  }
}

void ConsoleApp::runMenu(const User& user, const std::string& title,
                         const std::vector<MenuItem>& items) {
  for (;;) {
    // An administrator may deactivate an account while its owner is logged in; the session
    // ends at the next screen instead of waiting for the next login.
    const std::unique_ptr<User> current = app_.userRepository().findById(user.userId());
    if (!current || !current->isActive()) {
      c_.printError("Your account has been deactivated. You are being logged out.");
      return;
    }
    c_.print();
    c_.print("=== " + title + " ===");
    for (std::size_t i = 0; i < items.size(); ++i) {
      c_.print("  " + std::to_string(i + 1) + ") " + items[i].label);
    }
    c_.print("  0) Log out");
    const auto choice = c_.askChoice("Choose: ");
    if (!choice) continue;
    if (*choice == 0) return;
    if (static_cast<std::size_t>(*choice) > items.size()) {
      c_.printError("That is not a menu option.");
      continue;
    }
    guarded(items[static_cast<std::size_t>(*choice) - 1].action);
  }
}

void ConsoleApp::firstRunSetup() {
  if (app_.userRepository().countAll() != 0) return;
  c_.print("First start: no accounts exist yet. Create the system administrator.");
  for (;;) {
    guarded([&] {
      const std::string name = c_.ask("  Full name: ");
      const std::string username = c_.ask("  Username: ");
      const auto password = c_.askNewPassword();
      if (!password) return;
      app_.users().createInitialAdministrator(name, username, *password);
      c_.print("  Administrator created. You can now log in.");
    });
    if (app_.userRepository().countAll() != 0) return;
  }
}

void ConsoleApp::run() {
  c_.print("Offline Moderated Learning Management System");
  try {
    for (;;) {
      c_.print();
      c_.print("1) Log in    0) Exit");
      const auto choice = c_.askChoice("Choose: ");
      if (!choice) continue;
      if (*choice == 0) {
        c_.print("Goodbye.");
        return;
      }
      if (*choice != 1) {
        c_.printError("That is not a menu option.");
        continue;
      }
      std::unique_ptr<User> user;
      guarded([&] {
        const std::string username = c_.ask("Username: ");
        const std::string password = c_.askPassword("Password: ");
        user = app_.auth().login(username, password);
      });
      if (!user) continue;
      c_.print("Welcome, " + user->fullName() + " (" + user->getRole() + ").");
      switch (user->role()) {
        case Role::Administrator: administratorMenu(*user); break;
        case Role::Lecturer: lecturerMenu(*user); break;
        case Role::HeadOfDepartment: headOfDepartmentMenu(*user); break;
        case Role::Student: studentMenu(*user); break;
      }
      user->logout();
      c_.print("You have been logged out.");
    }
  } catch (const InputClosed&) {
    c_.print();
    c_.print("Goodbye.");
  }
}

}  // namespace lms
