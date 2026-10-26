# Requirements coverage (implementation view)

| Req | Implemented in | Verified by (tests/) |
|-----|----------------|----------------------|
| FR-01 accounts | `UserService`, `SQLiteUserRepository` | `UserServiceTest.*`, `AuthTest.ResetPasswordByAdministratorOnly` |
| FR-02 login | `AuthService`, `User::login`, `PasswordHasher` | `AuthTest.*` |
| FR-03 roles | `AccessControl`, `User::permissions()`, `CourseAccess` | `AccessControlTest.*` |
| FR-04 courses | `CourseService` | `CourseTest.*`, `EnrolmentTest.*` |
| FR-05 assignments | `AssignmentService::createAssignment` | `AssignmentTest.*` |
| FR-06 student views | `CourseService::listCourses`, `AssignmentService::listForStudent` | `AccessControlTest.StudentsSeeOnlyTheirOwn...`, `SubmissionTest.StatusShownToStudent` |
| FR-07 submit (UC-01, incl. optional comments) | `SubmissionService::submit`, `FileStorage` | `SubmissionTest.OnTime*`, `...File*`, `...OptionalComments*` |
| FR-08 resubmit | `SubmissionService::resubmit/submit` (attempts) | `SubmissionTest.Resubmission*`, `...ResubmitReplaces...` |
| FR-09 late penalty | `StandardLatePenaltyPolicy` (Strategy), `MarkingService::recordMark` | `LatePenaltyPolicyTest.*`, `MarkingTest.LatePenalty*`, `SubmissionTest.LateSubmissions*` |
| FR-10 extensions | `AssignmentService::grantExtension` | `ExtensionTest.*` |
| FR-11 marks + feedback | `MarkingService` | `MarkingTest.*` |
| FR-12 moderation | `ModerationService` | `ModerationTest.*` |
| FR-13 hidden until approved | `ModerationService`, `IMarkRepository::findReleasedByStudent`, release trigger | `ModerationTest.StudentsCannotSee...`, `...DatabaseRefusesRelease...` |
| FR-14 audit | `AuditLog`, `mark_audit_log` | `AuditTest.*` |
| FR-15 view released | `ResultsService` | `ModerationTest.StudentsCannotSee...`, `...PenaltyExplanation` |
| FR-16 view records | `MarkingService::listSubmissions`, `ModerationService::reviewMarks`, `AuditLog::history` | `AuditTest.HistoryVisible...`, `ModerationTest.Review...` |
| NFR-01 login < 2 s (800 users) | Argon2id interactive, indexes | `PerformanceTest.LoginWithEightHundredStudentsStored` (~55 ms) |
| NFR-02 800 students | schema, repositories | `CapacityTest.*` |
| NFR-03 salted hashes | `PasswordHasher` | `AuthTest.PasswordsAreStoredAsSaltedHashes`, `PasswordHasherTest.*` |
| NFR-04 access control | `AccessControl` | `AccessControlTest.*` |
| NFR-05 100 records | repositories, transactions | `ReliabilityTest.HundredAssessmentRecordsRoundTrip` |
| NFR-06 audit fields | `AuditLog` | `AuditTest.EveryMarkChange...` |
| NFR-07 offline | no network code; local SQLite | (manual: run with network disabled) |
| NFR-08 CMake build | `CMakeLists.txt` | CI / `docs/BUILD_GUIDE.md` |
