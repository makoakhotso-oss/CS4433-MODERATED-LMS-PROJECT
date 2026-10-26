# Coding standard

1. **Language:** C++17, no compiler extensions. Builds without warnings under
   `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (MSVC `/W4`).
2. **Naming:** types `PascalCase`; functions and variables `camelCase`; data members end in `_`
   (except plain structs); constants `kPascalCase`; namespaces lower-case (`lms`). Interfaces start
   with `I`. One class per header, header name = class name.
3. **Layering rule:** `ui` may call `services`; `services` may call `data` *interfaces*; only
   `src/data` includes `<sqlite3.h>`. No SQL outside `src/data`, no business rules in `ui`.
4. **Memory and ownership:** RAII everywhere; no `new`/`delete`; `std::unique_ptr` for owned
   polymorphic objects; references for non-owning dependencies injected through constructors.
5. **Errors:** throw exceptions derived from `lms::LmsError` (never return error codes, never
   `exit()`); destructors never throw. The UI catches `LmsError` and prints the message; any other
   exception is logged and reported generically. The program must not crash on bad input.
6. **Input validation:** every external value is validated at the service boundary
   (`common/Validation.hpp`) and again by database constraints.
7. **SQL:** only prepared statements with `?` placeholders and `Statement::bind*`; SQL text is a
   compile-time constant. String concatenation of user values into SQL is forbidden.
8. **Security:** passwords only via `PasswordHasher`; never logged, never stored, never compared
   with `==`. Every service operation starts with `AccessControl::require(...)`.
9. **Time:** all times are UTC `Timestamp` values internally; `IClock` is injected so rules can be
   tested deterministically.
10. **Style:** 2-space indent, 100-column limit, braces on the same line (`.clang-format`
    provided). Comments explain *why* (rules, requirements ids), not what.
11. **Reviews and checks before merging:** build with no warnings, all tests pass, `cppcheck`
    clean, ASan/UBSan run clean.
