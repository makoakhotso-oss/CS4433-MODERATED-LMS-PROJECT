#pragma once
#include "lms/Application.hpp"
#include "lms/ui/Console.hpp"

namespace lms {

/// Fills an EMPTY database with a small demonstration school (for the viva and manual
/// testing). Prints the demo accounts. Refuses to run if any user already exists.
void seedDemoData(Application& app, Console& console);

}  // namespace lms
