#include "TestSupport.hpp"
#include "lms/data/FileStorage.hpp"

using namespace lms;
using namespace lms::test;

// ---- FR-07: submit before the deadline ------------------------------------------------

TEST(SubmissionTest, OnTimeSubmissionIsRecorded) {
  World w;
  const SubmissionReceipt r = w.submit(*w.a, w.assignmentId);
  EXPECT_EQ(r.attemptNo, 1);
  EXPECT_FALSE(r.isResubmission);
  EXPECT_EQ(r.lateDays, 0);
  EXPECT_DOUBLE_EQ(r.penaltyPercent, 0.0);
  EXPECT_EQ(r.applicableDeadline, w.deadline);
  EXPECT_EQ(r.submittedAt, w.clock.now());
  const auto history = w.app->submissions().history(*w.a, w.assignmentId);
  ASSERT_EQ(history.size(), 1u);
  EXPECT_EQ(history[0].studentId(), w.studentA);
  EXPECT_TRUE(std::filesystem::exists(w.dir / "data" / history[0].getFilePath()));
}

TEST(SubmissionTest, SubmissionExactlyAtDeadlineIsOnTime) {
  World w;
  w.clock.set(w.deadline);
  EXPECT_EQ(w.submit(*w.a, w.assignmentId).lateDays, 0);
}

TEST(SubmissionTest, StatusShownToStudent) {
  World w;
  EXPECT_EQ(w.app->assignments().listForStudent(*w.a).at(0).statusText, "Not submitted");
  w.submit(*w.a, w.assignmentId);
  EXPECT_EQ(w.app->assignments().listForStudent(*w.a).at(0).statusText, "Submitted (attempt 1)");
  w.clock.set(w.deadline + kSecondsPerDay);
  EXPECT_EQ(w.app->assignments().listForStudent(*w.b).at(0).statusText, "Not submitted (deadline passed)");
}

// ---- FR-08: resubmission --------------------------------------------------------------

TEST(SubmissionTest, ResubmissionBeforeDeadlineKeepsHistoryAndLatestIsCurrent) {
  World w;
  w.submit(*w.a, w.assignmentId, "v1.txt");
  w.clock.advance(3600);
  const SubmissionReceipt r2 = w.submit(*w.a, w.assignmentId, "v2.txt");
  EXPECT_EQ(r2.attemptNo, 2);
  EXPECT_TRUE(r2.isResubmission);
  const auto history = w.app->submissions().history(*w.a, w.assignmentId);
  ASSERT_EQ(history.size(), 2u);  // nothing is overwritten
  EXPECT_EQ(history[0].attemptNo(), 1);
  EXPECT_EQ(history[1].attemptNo(), 2);
  EXPECT_NE(history[0].getFilePath(), history[1].getFilePath());
  EXPECT_EQ(w.countFiles(), 2);
  const auto rows = w.app->marking().listSubmissions(*w.lecturer, w.assignmentId);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].submission.attemptNo(), 2);  // the latest valid submission is the one to mark
}

TEST(SubmissionTest, ResubmissionAfterDeadlineIsRefused) {
  World w;
  w.submit(*w.a, w.assignmentId);
  w.clock.set(w.deadline + 60);
  EXPECT_THROW(w.submit(*w.a, w.assignmentId, "again.txt"), BusinessRuleError);
  EXPECT_EQ(w.app->submissions().history(*w.a, w.assignmentId).size(), 1u);
  EXPECT_EQ(w.countFiles(), 1);
}

TEST(SubmissionTest, LateFirstSubmissionCannotBeReplaced) {
  World w;
  w.clock.set(w.deadline + 3600);
  w.submit(*w.a, w.assignmentId);
  EXPECT_THROW(w.submit(*w.a, w.assignmentId, "again.txt"), BusinessRuleError);
}

TEST(SubmissionTest, MarkedWorkCanBeResubmittedBeforeDeadlineAndHistoryIsKept) {
  World w;
  const auto first = w.submit(*w.a, w.assignmentId, "v1.txt");
  w.mark(first.submissionId, 70);
  w.clock.advance(60);

  const auto second = w.app->submissions().resubmit(*w.a, first.submissionId,
                                                    w.makeFile("again.txt"));
  EXPECT_EQ(second.attemptNo, 2);
  EXPECT_TRUE(second.isResubmission);
  const auto history = w.app->submissions().history(*w.a, w.assignmentId);
  ASSERT_EQ(history.size(), 2u);
  EXPECT_EQ(history[0].submissionId(), first.submissionId);
  EXPECT_EQ(history[1].submissionId(), second.submissionId);
  EXPECT_GT(history[1].submittedAt(), history[0].submittedAt());

  const auto rows = w.app->marking().listSubmissions(*w.lecturer, w.assignmentId);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].submission.submissionId(), second.submissionId);
  EXPECT_FALSE(rows[0].mark.has_value());  // the old mark does not apply to the new attempt
}

TEST(SubmissionTest, ResubmitReplacesTheCurrentSubmission) {
  World w;
  const auto first = w.submit(*w.a, w.assignmentId, "v1.txt");
  const auto second = w.app->submissions().resubmit(*w.a, first.submissionId, w.makeFile("v2.txt"));
  EXPECT_EQ(second.attemptNo, 2);
  EXPECT_TRUE(second.isResubmission);
  EXPECT_EQ(w.app->submissions().history(*w.a, w.assignmentId).size(), 2u);
  // the old attempt is no longer the current one
  EXPECT_THROW(w.app->submissions().resubmit(*w.a, first.submissionId, w.makeFile("v3.txt")), BusinessRuleError);
  // someone else's or a missing submission looks the same: not found
  EXPECT_THROW(w.app->submissions().resubmit(*w.b, second.submissionId, w.makeFile("x.txt")), NotFoundError);
  EXPECT_THROW(w.app->submissions().resubmit(*w.a, 9999, w.makeFile("y.txt")), NotFoundError);
  // after the deadline the same rule as for any resubmission applies
  w.clock.set(w.deadline + 60);
  EXPECT_THROW(w.app->submissions().resubmit(*w.a, second.submissionId, w.makeFile("z.txt")), BusinessRuleError);
}

TEST(SubmissionTest, OptionalCommentsAreSavedAndShownToTheLecturer) {
  World w;
  w.app->submissions().submit(*w.a, w.assignmentId, w.makeFile("a.txt"), "  Please see page 3  ");
  w.submit(*w.b, w.assignmentId);
  const auto rows = w.app->marking().listSubmissions(*w.lecturer, w.assignmentId);
  ASSERT_EQ(rows.size(), 2u);
  for (const auto& row : rows) {
    EXPECT_EQ(row.submission.comments(), row.studentName == "Student A" ? "Please see page 3" : "");
  }
  EXPECT_THROW(w.app->submissions().submit(*w.a, w.assignmentId, w.makeFile("b.txt"), std::string(1001, 'x')), ValidationError);
  EXPECT_EQ(w.app->submissions().history(*w.a, w.assignmentId).size(), 1u);  // refused before saving
}

// ---- FR-09: late submissions and the penalty -------------------------------------------

TEST(SubmissionTest, LateSubmissionsAreAcceptedWithPenaltyPreview) {
  struct Case { Timestamp offset; int days; double percent; };
  const Case cases[] = {{1, 1, 10.0},
                        {kSecondsPerDay, 1, 10.0},
                        {kSecondsPerDay + 1, 2, 20.0},
                        {2 * kSecondsPerDay, 2, 20.0},
                        {3 * kSecondsPerDay, 3, 30.0},
                        {3 * kSecondsPerDay + 1, 4, 100.0},
                        {10 * kSecondsPerDay, 10, 100.0}};
  for (const Case& c : cases) {
    World w;
    w.clock.set(w.deadline + c.offset);
    const SubmissionReceipt r = w.submit(*w.a, w.assignmentId);
    EXPECT_EQ(r.lateDays, c.days) << "offset " << c.offset;
    EXPECT_DOUBLE_EQ(r.penaltyPercent, c.percent) << "offset " << c.offset;
  }
}

TEST(SubmissionTest, RejectModeRefusesSubmissionsBeyondTheLatePeriod) {
  SubmissionOptions strict;
  strict.rejectBeyondLatePeriod = true;
  World w(PasswordHasher::Cost::Minimal, strict);
  w.clock.set(w.deadline + 3 * kSecondsPerDay);
  EXPECT_NO_THROW(w.submit(*w.a, w.assignmentId));              // 3 days late: still allowed
  w.clock.set(w.deadline + 3 * kSecondsPerDay + 1);
  EXPECT_THROW(w.submit(*w.b, w.assignmentId), BusinessRuleError);  // 4 days late: rejected
  EXPECT_EQ(w.countFiles(), 1);
}

// ---- FR-10: individual extensions --------------------------------------------------------

TEST(ExtensionTest, ExtensionAppliesOnlyToTheSelectedStudent) {
  World w;
  const Timestamp extended = w.deadline + 3 * kSecondsPerDay;
  const Extension e = w.app->assignments().grantExtension(*w.lecturer, w.assignmentId, w.studentA, extended, "Illness");
  EXPECT_EQ(e.newDeadline, extended);
  EXPECT_EQ(e.approvedBy, w.lecturerId);

  w.clock.set(w.deadline + 2 * kSecondsPerDay);
  const SubmissionReceipt ra = w.submit(*w.a, w.assignmentId);
  EXPECT_EQ(ra.lateDays, 0);
  EXPECT_TRUE(ra.extensionApplied);
  EXPECT_EQ(ra.applicableDeadline, extended);
  const SubmissionReceipt rb = w.submit(*w.b, w.assignmentId);
  EXPECT_EQ(rb.lateDays, 2);
  EXPECT_FALSE(rb.extensionApplied);
  EXPECT_EQ(rb.applicableDeadline, w.deadline);
}

TEST(ExtensionTest, PenaltyIsMeasuredFromTheExtendedDeadline) {
  World w;
  w.app->assignments().grantExtension(*w.lecturer, w.assignmentId, w.studentA, w.deadline + 2 * kSecondsPerDay, "Family emergency");
  w.clock.set(w.deadline + 3 * kSecondsPerDay);  // one day after the extended deadline
  const auto r = w.submit(*w.a, w.assignmentId);
  EXPECT_EQ(r.lateDays, 1);
  EXPECT_EQ(w.mark(r.submissionId, 80).finalScore(), 70.0);
}

TEST(ExtensionTest, ExtensionAlsoAllowsResubmissionUntilTheNewDeadline) {
  World w;
  w.submit(*w.a, w.assignmentId, "v1.txt");
  w.app->assignments().grantExtension(*w.lecturer, w.assignmentId, w.studentA, w.deadline + 2 * kSecondsPerDay, "More time");
  w.clock.set(w.deadline + kSecondsPerDay);
  EXPECT_NO_THROW(w.submit(*w.a, w.assignmentId, "v2.txt"));
  EXPECT_EQ(w.app->submissions().history(*w.a, w.assignmentId).size(), 2u);
}

TEST(ExtensionTest, Validation) {
  World w;
  auto& svc = w.app->assignments();
  EXPECT_THROW(svc.grantExtension(*w.lecturer, w.assignmentId, w.studentA, w.deadline, "same"), ValidationError);
  EXPECT_THROW(svc.grantExtension(*w.lecturer, w.assignmentId, w.studentA, w.deadline - 1, "earlier"), ValidationError);
  EXPECT_THROW(svc.grantExtension(*w.lecturer, w.assignmentId, w.studentA, w.deadline + 100, "  "), ValidationError);
  EXPECT_THROW(svc.grantExtension(*w.lecturer, w.assignmentId, w.studentC, w.deadline + 100, "not enrolled"), NotFoundError);
  EXPECT_THROW(svc.grantExtension(*w.lecturer, 9999, w.studentA, w.deadline + 100, "x"), NotFoundError);
}

TEST(ExtensionTest, SecondExtensionReplacesTheFirst) {
  World w;
  auto& svc = w.app->assignments();
  svc.grantExtension(*w.lecturer, w.assignmentId, w.studentA, w.deadline + kSecondsPerDay, "first");
  svc.grantExtension(*w.lecturer, w.assignmentId, w.studentA, w.deadline + 4 * kSecondsPerDay, "second");
  EXPECT_EQ(w.app->submissions().applicableDeadlineFor(*w.a, w.assignmentId), w.deadline + 4 * kSecondsPerDay);
}

TEST(ExtensionTest, NotPossibleOnceWorkIsMarked) {
  World w;
  w.clock.set(w.deadline + 3600);
  const auto r = w.submit(*w.a, w.assignmentId);
  w.mark(r.submissionId, 70);
  EXPECT_THROW(w.app->assignments().grantExtension(*w.lecturer, w.assignmentId, w.studentA, w.deadline + kSecondsPerDay, "late"), BusinessRuleError);
}

TEST(AssignmentTest, CreationRules) {
  World w;
  auto& svc = w.app->assignments();
  EXPECT_THROW(svc.createAssignment(*w.lecturer, w.courseId, "", "d", day(20), 10), ValidationError);
  EXPECT_THROW(svc.createAssignment(*w.lecturer, w.courseId, "t", "d", day(20), 0), ValidationError);
  EXPECT_THROW(svc.createAssignment(*w.lecturer, w.courseId, "t", "d", day(20), -5), ValidationError);
  EXPECT_THROW(svc.createAssignment(*w.lecturer, w.courseId, "t", "d", w.clock.now(), 10), ValidationError);  // not in the future
  EXPECT_THROW(svc.createAssignment(*w.lecturer, 9999, "t", "d", day(20), 10), NotFoundError);
  const Assignment a = svc.createAssignment(*w.lecturer, w.courseId, "Quiz", "desc", day(20), 25.5);
  EXPECT_EQ(svc.getAssignment(*w.lecturer, a.assignmentId()).title(), "Quiz");
}

TEST(AssignmentTest, DomainDeadlineCanOnlyMoveLater) {
  Assignment a(1, 1, 1, "t", "d", day(5), 10);
  EXPECT_THROW(a.setDeadline(day(4)), BusinessRuleError);
  EXPECT_THROW(a.setDeadline(day(5)), BusinessRuleError);
  a.setDeadline(day(6));
  EXPECT_EQ(a.deadline(), day(6));
  EXPECT_TRUE(a.isOpen(day(6)));
  EXPECT_FALSE(a.isOpen(day(6) + 1));
}

// ---- access and file handling --------------------------------------------------------------

TEST(SubmissionTest, StudentMustBeEnrolled) {
  World w;
  EXPECT_THROW(w.submit(*w.c, w.assignmentId), NotFoundError);
  EXPECT_THROW(w.submit(*w.a, 9999), NotFoundError);
  EXPECT_EQ(w.countFiles(), 0);
}

TEST(SubmissionTest, FileProblemsAreReportedAndLeaveNothingBehind) {
  World w;
  EXPECT_THROW(w.app->submissions().submit(*w.a, w.assignmentId, w.dir / "missing.txt"), StorageError);
  EXPECT_THROW(w.app->submissions().submit(*w.a, w.assignmentId, w.makeFile("empty.txt", "")), StorageError);
  EXPECT_THROW(w.app->submissions().submit(*w.a, w.assignmentId, w.makeFile("virus.exe", "MZ")), StorageError);
  EXPECT_THROW(w.app->submissions().submit(*w.a, w.assignmentId, w.dir), StorageError);  // a folder
  EXPECT_EQ(w.countFiles(), 0);
  EXPECT_TRUE(w.app->submissions().history(*w.a, w.assignmentId).empty());
}

TEST(SubmissionTest, StoredFileNamesAreSanitised) {
  World w;
  w.submit(*w.a, w.assignmentId, "my answer (final) ;rm -rf.txt");
  const auto h = w.app->submissions().history(*w.a, w.assignmentId);
  const std::string stored = h.at(0).getFilePath();
  EXPECT_EQ(stored.find(' '), std::string::npos);
  EXPECT_EQ(stored.find('('), std::string::npos);
  EXPECT_EQ(stored.find(';'), std::string::npos);
  EXPECT_EQ(stored.find(".."), std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(w.dir / "data" / stored));
}

TEST(FileStorageTest, EnforcesSizeLimit) {
  const auto dir = std::filesystem::temp_directory_path() / "lms_fs_test";
  std::filesystem::create_directories(dir);
  std::ofstream(dir / "big.txt") << "12345678901";  // 11 bytes
  FileStorage tiny(dir / "store", 10);
  EXPECT_THROW(tiny.store(1, 1, 1, dir / "big.txt"), StorageError);
  FileStorage ok(dir / "store", 11);
  EXPECT_NO_THROW(ok.store(1, 1, 1, dir / "big.txt"));
  std::filesystem::remove_all(dir);
}
