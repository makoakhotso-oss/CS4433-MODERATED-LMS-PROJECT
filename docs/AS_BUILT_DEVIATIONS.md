# As-built deviations and document contradictions

The submitted documents (Project Management Plan, Stakeholder Analysis, SRS, Use Case Model,
Software Design Document) are a **frozen baseline** and are not edited. This file records, for the
final report (Section 6 and the traceability appendix), every place where the implementation
had to choose between statements that disagree, and every remaining difference from the design.

## Part A - contradictions inside the submitted documents

| # | The documents say... | ...but also | What the code does |
|---|----------------------|-------------|--------------------|
| A1 | **UC-01 exception flow:** after the deadline, with no extension or late period, the system *rejects* the submission | **UC-01 business rule** and **Stakeholder Conflict 2:** after three days "the mark is zero"; **SDD 3.3.4 sequence diagram:** a submission more than 3 days late is *saved* with a zero outcome; Student Representative: late work "should not simply be rejected" | Accepted and saved; the mark is zero. A first submission can be late; a *re*submission after the deadline is refused (UC-01 business rule). One switch, `SubmissionOptions::rejectBeyondLatePeriod`, gives the "reject" reading instead. |
| A2 | **SRS FR-04 and Stakeholder Analysis:** the *Lecturer* creates and manages courses; the Administrator manages users, roles, settings | **Assignment brief (case study):** the *Administrator* manages courses, assigns lecturers and enrols students | Both: a lecturer creates/renames own courses; the administrator creates courses for any lecturer, reassigns lecturers and enrols/removes students (the SRS has no enrolment requirement, so only the administrator enrols). |
| A3 | **SDD Class Diagram:** `Lecturer` and `HeadOfDepartment` have `departmentId`; the case study says the HoD moderates "all courses in their department" | **SDD ERD / SQL schema / data dictionary:** no department anywhere | Added `departments`, `users.department_id`, `courses.department_id`. A HoD can only review/approve/return marks of courses in their own department (change **D-1**). |
| A4 | **UC-02 step 7**, **SDD sequence 3.3.5** (`saveModerationPending`) and the **state machine** ("Under Moderation") all need a stored "sent for moderation" state | **SDD schema:** nothing can store it (`moderation_decisions` only allows *Approved* / *ChangesRequested*, and `moderator_id` is mandatory) | Added `marks.moderation_state` (`Marked`, `UnderModeration`, `ChangesRequested`, `Released`) with a CHECK tying `Released` to `released_at` (**D-2**). |
| A5 | **SDD state machine:** the HoD can *request changes* **and** *reject moderation* (state *Rejected* -> student resubmits); **Class Diagram:** `rejectMarks(assignmentId, reason)` | **UC-03 alternative flow**, **SDD sequence 3.3.5** and the **schema CHECK** know only *Approved* and *ChangesRequested* | One return path: `ModerationService::rejectMarks` records *ChangesRequested* and sends the marks back to the **lecturer** (UC-03). There is no "Rejected -> student resubmits" path because the database cannot record it. |
| A6 | **SDD ERD** shows a table `moderation_history` and an unreadable box at the bottom left | **SDD data dictionary, SQL schema and table-to-class mapping (3.4.6)** do not contain them | Not built. `moderation_decisions` keeps every decision (**D-7**). |
| A7 | **UC-01 step 3:** the student "adds comments if needed" | **SDD `submissions` table** has no comment column | Added `submissions.comments` (optional, up to 1000 characters); shown to the lecturer when marking (**D-8**). |
| A8 | **SDD 3.4.4 constraints:** "a student must not receive marks in a course they are not enrolled in" | The SQL has no rule for it | Trigger `validate_submission_enrolment` (**D-3**), plus the check in `SubmissionService`. |
| A9 | **Stakeholder Resolution 4:** the "most recent valid submission *before the deadline*" is marked | **UC-01 alt. flow 1:** a first submission may be up to three days late | The latest attempt is always the current one; a late first submission is the only attempt, so it is the one marked. |
| A10 | **Class Diagram** `SubmissionStatus` has no *Changes requested* | **State machine** has it | The enum includes `ChangesRequested`. |

## Part B - remaining differences between the code and the submitted design

These are deliberate and small. Each can be defended in the viva.

| # | Design | Implementation | Why |
|---|--------|----------------|-----|
| B1 | Role classes list operations (`Administrator.createUser`, `Lecturer.recordMark`, `Student.submitAssignment`, ...) | The role classes carry the role's **permissions** (polymorphic `role()`, `permissions()`); the operations are services (`UserService`, `MarkingService`, `SubmissionService`, ...) that call `AccessControl` first | SDD 3.1 puts rules and permissions in the Application Logic Layer; keeping `domain` free of persistence preserves the layering. |
| B2 | `Course.addStudent/removeStudent/addAssignment/getAssignments` | `CourseService` and `AssignmentService` | Same reason; entities stay plain data + invariants. |
| B3 | Class `AuditLog` with `recordChange(...)` | `AuditLog` is the service (`recordChange`, `history`); a row is the `AuditEntry` record | The log needs a repository and permission-checked reads. |
| B4 | Repositories: `ISubmissionRepository`, `IMarkRepository` only | Also `IUserRepository`, `IDepartmentRepository`, `ICourseRepository`, `IEnrolmentRepository`, `IAssignmentRepository`, `IExtensionRepository`, `IModerationRepository`, `IAuditRepository`; extra finders on the two designed interfaces | Every table needs a data-access class (Repository pattern, ISP). Signatures `save(x): bool`, `findById`, `findByAssignment` are exactly as designed. |
| B5 | `ILatePenaltyPolicy.calculatePenalty(lateDays, mark)` | **Same signature.** `mark` is the available mark; the result is the deduction; a deduction equal to `mark` is the zero outcome | (Conformed.) |
| B6 | `SubmissionService.submit/resubmit/calculatePenalty(lateDays)` | **Same names.** `resubmit(submissionId, file)` replaces the current attempt | (Conformed.) |
| B7 | `MarkingService.recordMark(submissionId, score, feedback): bool` | `recordMark(...)` also takes a `reason` and returns the `MarkRecord` | The audit log needs the reason (NFR-06 / FR-14). |
| B8 | HoD `approveRelease` / state *Approved* then *Released* | One action: approving also releases (UC-03 steps 4-7; sequence 3.3.5 calls approve then release in the same branch) | Matches UC-03. |
| B9 | Trigger `prevent_unapproved_mark_release` uses `MAX(decided_at)` | Uses `MAX(decision_id)` (**D-5**) | Two decisions in the same second could be confused. |
| B10 | - | Extra database hardening: indexes (**D-4**) and append-only audit log triggers (**D-6**) | NFR-01 performance; NFR-06 integrity. |
| B11 | - | Supporting classes not in the diagram: `AccessControl`, `AuthService`, `UserService`, `CourseService`, `CourseAccess`, `AssignmentService`, `MarkingRowBuilder`, `ResultsService`, `FileStorage`, `IUnitOfWork`, `UserFactory`, `Application` (composition root), `Console`/`ConsoleApp` | Needed to build a working program from the design. |

Left on purpose for Phase 8 (CR1): class-wide extensions and showing extensions on the student's
assignment list. Hooks: `Assignment::setDeadline`, `IAssignmentRepository::updateDeadline`,
`IExtensionRepository::findByAssignment`, `AssignmentService::listForStudent`.

## Part C - how to present this without editing submitted work

1. **Do not edit the submitted files.** Put this table (Parts A and B) in the final report,
   Section 6 "Implementation - key design decisions", under the heading *Design deviations and
   resolved contradictions*, and reference it from the traceability matrix appendix.
2. **Your own Project Management Plan already allows it.** Its risk register lists "Design and
   code may not match" with the mitigation "check the implementation against the UML diagrams and
   update the design when necessary". The assignment also requires the risk register to be updated
   at each gate and the design documents to be updated in Phase 8. In the updated risk register,
   record this risk as having occurred, with the cause (A3-A7: the submitted design was internally
   inconsistent) and the action (this change log).
3. **Suggested wording for the report:**
   > While implementing the design, ten points were found where the submitted documents
   > disagreed with each other (for example, the Use Case Model rejects very late submissions
   > while the sequence diagram and the stakeholder resolution record them with a zero mark, and
   > the Class Diagram gives users a department that the ERD does not store). The baseline
   > documents were left unchanged. Each conflict was resolved in the way that satisfied the
   > requirement or use case first, then the sequence/class diagrams, and the schema was extended
   > minimally (departments, moderation state, optional submission comments, and integrity
   > triggers). All changes are listed in the as-built deviation log (Table X) and are traced to
   > the requirements they serve.
4. **Optional:** send your supervisor one short message before the next gate review listing A1-A7,
   so the as-built log is already agreed.
