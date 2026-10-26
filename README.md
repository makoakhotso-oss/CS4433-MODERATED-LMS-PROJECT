# Moderated Learning Management System (LMS) v1.0

Offline C++17 / SQLite console application for a secondary school (CS4433 Software
Engineering project). Four roles - Administrator, Lecturer, Head of Department (HoD),
Student - with assignment submission, late penalties, individual extensions, marking,
HoD moderation, release of marks and a tamper-evident mark audit log.

```
include/lms/   headers        src/   implementation        tests/  GoogleTest suite (103 tests)
sql/schema.sql database schema (single source of truth, embedded at build time)
docs/          build guide, coding standard, security, as-built deviations, requirements coverage
```

Layers (Software Design Document 3.1): **ui** (console menus) -> **services** (rules,
permissions, late penalty strategy, workflow) -> **data** (repository interfaces + SQLite
implementations, the only code that touches SQLite).

Quick start: see [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md).

```
cmake -S . -B build && cmake --build build
./build/lms_tests            # run the automated tests
./build/lms --seed-demo      # empty database filled with demo school; log in as admin
```
