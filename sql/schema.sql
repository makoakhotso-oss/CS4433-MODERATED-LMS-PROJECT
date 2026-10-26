-- LMS database schema, version 1 (SQLite).
-- Based on Software Design Document section 3.4.5, with the changes listed in
-- docs/AS_BUILT_DEVIATIONS.md (D-1 .. D-8).
-- Foreign-key enforcement is switched on for every connection by the application.

CREATE TABLE departments (
    department_id INTEGER PRIMARY KEY,
    name          TEXT NOT NULL UNIQUE CHECK (length(trim(name)) > 0)
);

CREATE TABLE users (
    user_id       INTEGER PRIMARY KEY,
    full_name     TEXT NOT NULL CHECK (length(trim(full_name)) > 0),
    username      TEXT NOT NULL UNIQUE CHECK (length(username) BETWEEN 3 AND 32),
    password_hash TEXT NOT NULL,
    role          TEXT NOT NULL CHECK (
                      role IN ('Administrator', 'Lecturer', 'HeadOfDepartment', 'Student')
                  ),
    active        INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    department_id INTEGER,
    FOREIGN KEY (department_id) REFERENCES departments(department_id)
);

CREATE TABLE courses (
    course_id     INTEGER PRIMARY KEY,
    course_code   TEXT NOT NULL UNIQUE,
    course_name   TEXT NOT NULL,
    lecturer_id   INTEGER NOT NULL,
    department_id INTEGER NOT NULL,
    FOREIGN KEY (lecturer_id)   REFERENCES users(user_id),
    FOREIGN KEY (department_id) REFERENCES departments(department_id)
);

CREATE TABLE enrollments (
    student_id  INTEGER NOT NULL,
    course_id   INTEGER NOT NULL,
    enrolled_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (student_id, course_id),
    FOREIGN KEY (student_id) REFERENCES users(user_id),
    FOREIGN KEY (course_id)  REFERENCES courses(course_id)
);

CREATE TABLE assignments (
    assignment_id INTEGER PRIMARY KEY,
    course_id     INTEGER NOT NULL,
    created_by    INTEGER NOT NULL,
    title         TEXT NOT NULL,
    description   TEXT NOT NULL,
    deadline      TEXT NOT NULL,
    max_score     REAL NOT NULL CHECK (max_score > 0),
    FOREIGN KEY (course_id)  REFERENCES courses(course_id),
    FOREIGN KEY (created_by) REFERENCES users(user_id)
);

CREATE TABLE extensions (
    extension_id  INTEGER PRIMARY KEY,
    assignment_id INTEGER NOT NULL,
    student_id    INTEGER NOT NULL,
    approved_by   INTEGER NOT NULL,
    new_deadline  TEXT NOT NULL,
    reason        TEXT NOT NULL,
    FOREIGN KEY (assignment_id) REFERENCES assignments(assignment_id),
    FOREIGN KEY (student_id)    REFERENCES users(user_id),
    FOREIGN KEY (approved_by)   REFERENCES users(user_id),
    UNIQUE (assignment_id, student_id)
);

CREATE TABLE submissions (
    submission_id INTEGER PRIMARY KEY,
    assignment_id INTEGER NOT NULL,
    student_id    INTEGER NOT NULL,
    attempt_no    INTEGER NOT NULL CHECK (attempt_no >= 1),
    submitted_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    file_path     TEXT NOT NULL,
    comments      TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (assignment_id) REFERENCES assignments(assignment_id),
    FOREIGN KEY (student_id)    REFERENCES users(user_id),
    UNIQUE (assignment_id, student_id, attempt_no)
);

CREATE TABLE marks (
    mark_id          INTEGER PRIMARY KEY,
    submission_id    INTEGER NOT NULL UNIQUE,
    raw_score        REAL NOT NULL CHECK (raw_score >= 0),
    final_score      REAL NOT NULL CHECK (final_score >= 0),
    feedback         TEXT NOT NULL DEFAULT '',
    released_at      TEXT,
    moderation_state TEXT NOT NULL DEFAULT 'Marked' CHECK (
                         moderation_state IN ('Marked', 'UnderModeration', 'ChangesRequested', 'Released')
                     ),
    FOREIGN KEY (submission_id) REFERENCES submissions(submission_id),
    CHECK ((moderation_state = 'Released') = (released_at IS NOT NULL))
);

CREATE TABLE moderation_decisions (
    decision_id   INTEGER PRIMARY KEY,
    assignment_id INTEGER NOT NULL,
    moderator_id  INTEGER NOT NULL,
    decision      TEXT NOT NULL CHECK (decision IN ('Approved', 'ChangesRequested')),
    comments      TEXT NOT NULL DEFAULT '',
    decided_at    TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (assignment_id) REFERENCES assignments(assignment_id),
    FOREIGN KEY (moderator_id)  REFERENCES users(user_id)
);

CREATE TABLE mark_audit_log (
    audit_id       INTEGER PRIMARY KEY,
    mark_id        INTEGER NOT NULL,
    changed_by     INTEGER NOT NULL,
    previous_score REAL,
    new_score      REAL NOT NULL CHECK (new_score >= 0),
    changed_at     TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    reason         TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (mark_id)    REFERENCES marks(mark_id),
    FOREIGN KEY (changed_by) REFERENCES users(user_id)
);

-- Indexes for the common look-ups (login, course lists, marking lists).
CREATE INDEX idx_courses_lecturer      ON courses(lecturer_id);
CREATE INDEX idx_courses_department    ON courses(department_id);
CREATE INDEX idx_enrollments_course    ON enrollments(course_id);
CREATE INDEX idx_assignments_course    ON assignments(course_id);
CREATE INDEX idx_submissions_student   ON submissions(student_id);
CREATE INDEX idx_moderation_assignment ON moderation_decisions(assignment_id);
CREATE INDEX idx_audit_mark            ON mark_audit_log(mark_id);

-- Scores must not exceed the assignment maximum.
CREATE TRIGGER validate_mark_insert
BEFORE INSERT ON marks
FOR EACH ROW
WHEN NEW.raw_score > (
        SELECT a.max_score FROM submissions s
        JOIN assignments a ON a.assignment_id = s.assignment_id
        WHERE s.submission_id = NEW.submission_id)
  OR NEW.final_score > (
        SELECT a.max_score FROM submissions s
        JOIN assignments a ON a.assignment_id = s.assignment_id
        WHERE s.submission_id = NEW.submission_id)
BEGIN
    SELECT RAISE(ABORT, 'Mark exceeds assignment maximum');
END;

CREATE TRIGGER validate_mark_update
BEFORE UPDATE OF raw_score, final_score ON marks
FOR EACH ROW
WHEN NEW.raw_score > (
        SELECT a.max_score FROM submissions s
        JOIN assignments a ON a.assignment_id = s.assignment_id
        WHERE s.submission_id = NEW.submission_id)
  OR NEW.final_score > (
        SELECT a.max_score FROM submissions s
        JOIN assignments a ON a.assignment_id = s.assignment_id
        WHERE s.submission_id = NEW.submission_id)
BEGIN
    SELECT RAISE(ABORT, 'Mark exceeds assignment maximum');
END;

-- A mark cannot be released unless the latest moderation decision for its
-- assignment is an approval (D-5: "latest" is decided by decision_id, so two
-- decisions made in the same second cannot be confused).
CREATE TRIGGER prevent_unapproved_mark_release
BEFORE UPDATE OF released_at ON marks
FOR EACH ROW
WHEN NEW.released_at IS NOT NULL
 AND NOT EXISTS (
        SELECT 1
        FROM submissions s
        JOIN moderation_decisions d ON d.assignment_id = s.assignment_id
        WHERE s.submission_id = NEW.submission_id
          AND d.decision = 'Approved'
          AND d.decided_at <= NEW.released_at
          AND d.decision_id = (
                SELECT MAX(d2.decision_id) FROM moderation_decisions d2
                WHERE d2.assignment_id = s.assignment_id))
BEGIN
    SELECT RAISE(ABORT, 'Marks cannot be released before moderation approval');
END;

-- A student can only submit to an assignment of a course they are enrolled in.
CREATE TRIGGER validate_submission_enrolment
BEFORE INSERT ON submissions
FOR EACH ROW
WHEN NOT EXISTS (
        SELECT 1 FROM assignments a
        JOIN enrollments e ON e.course_id = a.course_id
        WHERE a.assignment_id = NEW.assignment_id
          AND e.student_id = NEW.student_id)
BEGIN
    SELECT RAISE(ABORT, 'Student is not enrolled in the course of this assignment');
END;

-- The audit log is append-only (D-6).
CREATE TRIGGER mark_audit_log_no_update
BEFORE UPDATE ON mark_audit_log
BEGIN
    SELECT RAISE(ABORT, 'The audit log is append-only');
END;

CREATE TRIGGER mark_audit_log_no_delete
BEFORE DELETE ON mark_audit_log
BEGIN
    SELECT RAISE(ABORT, 'The audit log is append-only');
END;
