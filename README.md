# Moderated Offline Learning Management System

Offline C++17 / SQLite console application for a secondary school (CS4433 Software
Engineering project). Four roles - Administrator, Lecturer, Head of Department (HoD),
Student - with assignment submission, late penalties, individual extensions, marking,
HoD moderation, release of marks and a tamper-evident mark audit log.

```
include/lms/   headers        src/   implementation        tests/  GoogleTest suite (104 tests, as verified in the submitted build log)
sql/schema.sql database schema (single source of truth, embedded at build time)
docs/          build guide, coding standard, security, as-built deviations, requirements coverage
```

Layers (Software Design Document 3.1): **ui** (console menus) -> **services** (rules,
permissions, late penalty strategy, workflow) -> **data** (repository interfaces + SQLite
implementations, the only code that touches SQLite).

Quick start: see [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md). The submitted Phase 2 requirements and Phase 3 diagrams are retained in `reference/` for traceability.

```
cmake -S . -B build && cmake --build build
./build/lms_tests            # run the automated tests
./build/lms --seed-demo      # empty database filled with demo school; log in as admin
```

---

## Final Project Submission

**Project:** Offline Moderated Learning Management System  
**Module:** CS4433 – Software Engineering

### Final Report
- [Final Consolidated Report (PDF)](docs/FINAL_REPORT.pdf)

### Original Diagrams

**Phase 1: Project Planning**
- [Work Breakdown Structure](images/wbs.png)
- [Gantt Chart](images/gantt_chart.png)

**Phase 2: Requirements Engineering**
- [Use Case Diagram](images/use_case.png)

**Phase 3: System Design**
- [Component Diagram](images/component_diagram.png)
- [Class Diagram](images/class_diagram.png)
- [State Machine Diagram](images/state_diagram.png)
- [Activity Diagram](images/activity_diagram.png)
- [Submit Assignment Sequence Diagram](images/sequence_diagram_submit_assignment.png)
- [Moderate and Release Marks Sequence Diagram](images/sequence_diagrams_moderate_and_release_marks.png)
- [Entity Relationship Diagram](images/erd.png)

**Phase 7: Deployment**
- [UML Deployment Diagram](images/uml_deployment_diagrams.png)

### Implementation and Supporting Documentation
- [Build and Installation Guide](docs/BUILD_GUIDE.md)
- [Requirements Traceability](docs/PHASE4_TRACEABILITY.md)
- [Requirements Coverage](docs/REQUIREMENTS_COVERAGE.md)
- [As-Built Design Deviations](docs/AS_BUILT_DEVIATIONS.md)
- [Security Documentation](docs/SECURITY.md)
- [Earlier Assignment Documents](reference/)
- [Source Code](src/)
- [Automated Tests](tests/)
- [Database Schema](sql/)
- [CI Test Runs](../../actions)

### Video Presentation
15-minute project presentation: **Link to be added after recording.**

