# Build guide

## Requirements
| Tool | Version | Notes |
|------|---------|-------|
| C++17 compiler | GCC 9+, Clang 10+, MSVC 2019+ | tested with GCC 13 |
| CMake | 3.16+ | |
| SQLite 3 (development files) | 3.24+ | needs `ON CONFLICT ... DO UPDATE` |
| libsodium (development files) | 1.0.18+ | password hashing (Argon2id) |
| GoogleTest | 1.10+ | optional; downloaded automatically if not installed (needs internet once) |

### Install the libraries
* **Debian / Ubuntu:** `sudo apt install build-essential cmake libsqlite3-dev libsodium-dev libgtest-dev`
* **Fedora:** `sudo dnf install gcc-c++ cmake sqlite-devel libsodium-devel gtest-devel`
* **macOS:** `brew install cmake sqlite libsodium googletest`
* **Windows (Visual Studio 2019+ and vcpkg):**
  `vcpkg install sqlite3 libsodium gtest --triplet x64-windows`, then configure with
  `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`.

## Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
Options: `-DLMS_BUILD_TESTS=OFF` (skip tests), `-DLMS_WARNINGS_AS_ERRORS=ON`.
Produces `build/lms` (the program) and `build/lms_tests`.

## Run the tests
```
cd build && ctest --output-on-failure       # or ./lms_tests
```

## Run the program
```
./build/lms [--db lms.db] [--data lms_data] [--log lms.log] [--seed-demo]
```
* First start with an empty database: the program asks you to create the first administrator.
* `--seed-demo` fills an **empty** database with a demonstration school (accounts are printed;
  passwords are for demonstration only).
* The database file and the `lms_data/` folder (uploaded work) are the only data; back them up
  together. The program needs no internet connection at any time (NFR-07).

## Static analysis, sanitizers and coverage (quality assurance)
```
cppcheck --enable=warning,style,performance,portability --std=c++17 -Iinclude src
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan && ./build-asan/lms_tests
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage -O0"
cmake --build build-cov && ./build-cov/lms_tests && gcov ...   (or gcovr)
```
