// Learning Management System - console application entry point.
#include <cstring>
#include <iostream>
#include <string>

#include "lms/Application.hpp"
#include "lms/common/Errors.hpp"
#include "lms/common/Logger.hpp"
#include "lms/ui/ConsoleApp.hpp"
#include "lms/ui/DemoData.hpp"

namespace {

void printHelp() {
  std::cout << "Learning Management System v1.0\n"
               "Usage: lms [options]\n"
               "  --db <file>      database file          (default: lms.db)\n"
               "  --data <folder>  uploaded work folder   (default: lms_data)\n"
               "  --log <file>     log file               (default: lms.log)\n"
               "  --seed-demo      fill an EMPTY database with demonstration data\n"
               "  --help           show this text\n";
}

}  // namespace

int main(int argc, char** argv) {
  lms::AppConfig config;
  std::string logPath = "lms.log";
  bool seedDemo = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto value = [&](std::string& target) {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << arg << "\n";
        return false;
      }
      target = argv[++i];
      return true;
    };
    if (arg == "--help" || arg == "-h") {
      printHelp();
      return 0;
    } else if (arg == "--db") {
      if (!value(config.databasePath)) return 2;
    } else if (arg == "--data") {
      if (!value(config.dataDirectory)) return 2;
    } else if (arg == "--log") {
      if (!value(logPath)) return 2;
    } else if (arg == "--seed-demo") {
      seedDemo = true;
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      printHelp();
      return 2;
    }
  }

  try {
    lms::Logger::instance().open(logPath);
    lms::SystemClock clock;
    lms::Application app(config, clock);
    lms::Console console(std::cin, std::cout);
    lms::ConsoleApp ui(app, console);
    try {
      if (seedDemo) lms::seedDemoData(app, console);
      ui.firstRunSetup();
      ui.run();
    } catch (const lms::InputClosed&) {
      console.print();
    }
    return 0;
  } catch (const lms::LmsError& e) {
    std::cerr << "Error: " << e.what() << "\n";
    lms::Logger::instance().error(std::string("Fatal: ") + e.what());
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Unexpected error: " << e.what() << "\n";
    lms::Logger::instance().error(std::string("Fatal: ") + e.what());
    return 1;
  }
}
