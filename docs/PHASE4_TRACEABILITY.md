# Phase 4 implementation traceability

This document maps the submitted Phase 2 SRS and Phase 3 design to the implementation. The
original submitted documents are retained in `reference/`; this mapping does not replace them.

## Functional requirements

| SRS requirement | Implementation | Verification |
|---|---|---|
| FR-01 Administrator manages accounts | `UserService`, `SQLiteUserRepository` | `UserServiceTest.*`, `AuthTest.ResetPasswordByAdministratorOnly` |
| FR-02 Username/password login | `AuthService`, `PasswordHasher` | `AuthTest.*`, `PasswordHasherTest.*` |
| FR-03 Four role permissions | Abstract `User`, four derived roles, `AccessControl`, `CourseAccess` | `AccessControlTest.*`, `UiTest.StudentCannotReachStaffMenuItems` |
| FR-04 Lecturer manages courses | `CourseService` and course repositories | `CourseTest.*` |
| FR-05 Lecturer creates assignments and deadlines | `AssignmentService` | `AssignmentTest.*` |
| FR-06 Student sees own courses and assignments | `CourseService`, `AssignmentService` | `AccessControlTest.StudentsSeeOnlyTheirOwnCoursesAndAssignments`, `SubmissionTest.StatusShownToStudent` |
| FR-07 Student submits work | `SubmissionService`, `FileStorage` | `SubmissionTest.OnTimeSubmissionIsRecorded`, `SubmissionTest.FileProblemsAreReportedAndLeaveNothingBehind` |
| FR-08 Student resubmits before applicable deadline; latest attempt is current | `SubmissionService`, `SQLiteSubmissionRepository` | `SubmissionTest.ResubmissionBeforeDeadlineKeepsHistoryAndLatestIsCurrent`, `SubmissionTest.MarkedWorkCanBeResubmittedBeforeDeadlineAndHistoryIsKept` |
| FR-09 Late submissions and 10% daily penalty up to three days | `ILatePenaltyPolicy`, `StandardLatePenaltyPolicy`, `MarkingService` | `LatePenaltyPolicyTest.*`, `MarkingTest.LatePenaltyIsAppliedFromTheSubmissionTime`, `SubmissionTest.LateSubmissionsAreAcceptedWithPenaltyPreview` |
| FR-10 Individual extensions | `AssignmentService`, extension repository | `ExtensionTest.*` |
| FR-11 Mark and feedback | `MarkingService`, mark repository | `MarkingTest.*` |
| FR-12 HoD moderation | `ModerationService` | `ModerationTest.*` |
| FR-13 Hide marks until approval/release | `ModerationService`, released-mark query, database trigger | `ModerationTest.StudentsCannotSeeMarksUntilTheHodApproves`, `ModerationTest.DatabaseRefusesReleaseWithoutApproval` |
| FR-14 Audit mark changes (user and time; Phase 4 also requires old/new/reason) | `AuditLog`, `mark_audit_log` | `AuditTest.EveryMarkChangeIsRecordedWithUserTimeOldAndNewValue`, `AuditTest.FeedbackOnlyChangeIsAlsoLogged`, `AuditTest.LogCannotBeAlteredOrDeleted` |
| FR-15 Student views released marks and feedback | `ResultsService` | `ModerationTest.ReleasedMarkShowsPenaltyExplanation` |
| FR-16 Authorised users view relevant records | `MarkingService`, `ModerationService`, `AuditLog` | `AuditTest.HistoryVisibleToLecturerAndDepartmentHeadOnly`, `ModerationTest.ReviewShowsMarksFeedbackAndHistory` |

## Non-functional requirements

| SRS requirement | Implementation / evidence |
|---|---|
| NFR-01 Login under 2 seconds with 800 students | Argon2id interactive parameters and indexed lookups; `PerformanceTest.LoginWithEightHundredStudentsStored` |
| NFR-02 Store at least 800 students | SQLite repositories; `CapacityTest.EightHundredStudentsAreStoredAndRetrievedUnchanged` |
| NFR-03 Salted password hashes | libsodium Argon2id; `AuthTest.PasswordsAreStoredAsSaltedHashes`, `PasswordHasherTest.*` |
| NFR-04 Role access restrictions | Service-layer permission and ownership checks; `AccessControlTest.*` |
| NFR-05 Save/retrieve 100 assessment records | SQLite persistence and transactions; `ReliabilityTest.HundredAssessmentRecordsRoundTrip` |
| NFR-06 Complete mark audit | Audit entries include actor, timestamp, old/new values and reason; `AuditTest.*` |
| NFR-07 Offline operation | Local console application, SQLite database and local file storage; no runtime network dependency |
| NFR-08 CMake maintainability | CMake build configuration and `docs/BUILD_GUIDE.md` |

## Class-diagram design patterns

- **Strategy:** `ILatePenaltyPolicy` and `StandardLatePenaltyPolicy` isolate the late-penalty rule from submission and marking services.
- **Repository:** interfaces in `include/lms/data/IRepositories.hpp` separate services from SQLite persistence; SQLite implementations are under `src/data/`.
- **Data Access Object / data-access layer:** SQLite statements and database operations are confined to `src/data/`.

The submitted class diagram identifies Strategy and Repository as its named patterns. `UserFactory` is an additional implementation helper for reconstructing the correct derived `User` type from stored role data; it is not presented as a required pattern in the submitted diagram.

## Important implementation interpretation

The SRS and use-case text contain a tension around submissions more than three days late: the UC-01 exception flow says to reject, while its business rule, stakeholder resolution and Phase 3 submit sequence describe saving the work with a zero outcome. The implementation follows the business-rule/sequence interpretation for a first submission. A resubmission after the applicable deadline is refused. This decision is documented in `AS_BUILT_DEVIATIONS.md`.

The system supports resubmission before the applicable deadline even if an earlier attempt was marked or released. The earlier attempt and its audit history remain stored; the new attempt becomes current and must be marked and moderated separately. This implements the SRS rule that a student may resubmit before the deadline and preserves the submission history.
