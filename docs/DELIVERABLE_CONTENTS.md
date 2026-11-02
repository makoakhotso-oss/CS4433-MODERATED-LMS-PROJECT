# Phase 4 source deliverable contents

- `src/` and `include/`: C++17 implementation, organised into UI, services, domain, data,
  security and common modules.
- `sql/schema.sql`: SQLite schema, constraints, indexes and integrity triggers. CMake embeds this
  schema into the application at build time.
- `tests/`: automated GoogleTest test cases.
- `CMakeLists.txt`, `cmake/` and `scripts/verify.sh`: build and verification support.
- `docs/`: build guide, coding standard, security notes, requirements coverage and as-built
  decisions.
- `reference/`: copies of the submitted assignment brief, Phase 2 SRS/use-case/stakeholder
  documents and editable Phase 3 diagrams plus the Software Design Document. These are included
  as traceability references; they are not silently changed by the implementation package.

Machine-specific build directories, local databases, uploaded student files, runtime logs and Git
metadata are intentionally excluded. They are generated locally and should not be distributed as
part of the source deliverable.
