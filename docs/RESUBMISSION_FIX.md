# Resubmission-after-marking fix

The submission workflow now permits a student to submit a new attempt while the applicable deadline has not passed, even if an earlier attempt has already been marked or released. The earlier attempt and its mark/audit history remain stored. The new attempt becomes the current attempt and must be marked and moderated separately.

A previously released mark for an older attempt is hidden from the student's released-results view once a newer attempt exists. The old mark and audit entries are retained in the database for history.

Resubmission after the applicable deadline remains blocked. The change does not alter extension or late-penalty calculations.

## Verify after rebuilding

From the project root:

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The tests added for this fix cover resubmission after marking, retention of both attempts, selection of the new attempt for marking, and hiding a released mark attached to an older attempt.
