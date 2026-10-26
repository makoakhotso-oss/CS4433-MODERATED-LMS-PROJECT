#include "TestSupport.hpp"

using namespace lms;
using namespace lms::test;

namespace {
// Both students submit on time; returns their submission ids.
std::pair<Id, Id> bothSubmit(World& w) {
  return {w.submit(*w.a, w.assignmentId).submissionId, w.submit(*w.b, w.assignmentId).submissionId};
}
}  // namespace

// ---- FR-11: marks and feedback; late penalty applied automatically ------------------------

TEST(MarkingTest, OnTimeWorkKeepsTheRawMark) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  const MarkRecord m = w.mark(r.submissionId, 80, "Well done");
  EXPECT_DOUBLE_EQ(m.rawScore(), 80.0);
  EXPECT_DOUBLE_EQ(m.finalScore(), 80.0);
  EXPECT_EQ(m.feedback(), "Well done");
  EXPECT_EQ(m.state(), ModerationState::Marked);
}

TEST(MarkingTest, LatePenaltyIsAppliedFromTheSubmissionTime) {
  struct Case { Timestamp offset; double expectedFinal; };
  const Case cases[] = {{1, 70.0}, {kSecondsPerDay + 1, 60.0}, {3 * kSecondsPerDay, 50.0},
                        {3 * kSecondsPerDay + 1, 0.0}, {9 * kSecondsPerDay, 0.0}};
  for (const Case& c : cases) {
    World w;
    w.clock.set(w.deadline + c.offset);
    const auto r = w.submit(*w.a, w.assignmentId);
    const MarkRecord m = w.mark(r.submissionId, 80);
    EXPECT_DOUBLE_EQ(m.rawScore(), 80.0);
    EXPECT_DOUBLE_EQ(m.finalScore(), c.expectedFinal) << "offset " << c.offset;
  }
}

// The final mark can never go below zero, even if the penalty exceeds the mark that was earned.
TEST(MarkingTest, FinalMarkNeverBelowZero) {
  World w;
  w.clock.set(w.deadline + 3 * kSecondsPerDay);  // 3 days late = 30 marks off 100
  const auto r = w.submit(*w.a, w.assignmentId);
  const MarkRecord m = w.mark(r.submissionId, 20);
  EXPECT_DOUBLE_EQ(m.rawScore(), 20.0);
  EXPECT_DOUBLE_EQ(m.finalScore(), 0.0);
}

TEST(MarkingTest, MarkValidation) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  auto& m = w.app->marking();
  EXPECT_THROW(m.recordMark(*w.lecturer, r.submissionId, 100.5, "f"), ValidationError);
  EXPECT_THROW(m.recordMark(*w.lecturer, r.submissionId, -1, "f"), ValidationError);
  EXPECT_THROW(m.recordMark(*w.lecturer, r.submissionId, std::nan(""), "f"), ValidationError);
  EXPECT_THROW(m.recordMark(*w.lecturer, r.submissionId, 50, "   "), ValidationError);  // feedback is required
  EXPECT_THROW(m.recordMark(*w.lecturer, 9999, 50, "f"), NotFoundError);
  EXPECT_NO_THROW(m.recordMark(*w.lecturer, r.submissionId, 100, "full marks"));  // boundary
}

TEST(MarkingTest, OnlyTheCurrentAttemptCanBeMarked) {
  World w;
  const auto first = w.submit(*w.a, w.assignmentId, "v1.txt");
  w.submit(*w.a, w.assignmentId, "v2.txt");
  EXPECT_THROW(w.mark(first.submissionId, 50), BusinessRuleError);
}

TEST(MarkingTest, OnlyTheCourseLecturerCanMark) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  EXPECT_THROW(w.app->marking().recordMark(*w.lecturer2, r.submissionId, 50, "f"), NotFoundError);
}

// ---- FR-14 / NFR-06: audit log ---------------------------------------------------------------

TEST(AuditTest, EveryMarkChangeIsRecordedWithUserTimeOldAndNewValue) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  w.mark(r.submissionId, 70);
  w.clock.advance(600);
  auto& marking = w.app->marking();
  EXPECT_THROW(marking.recordMark(*w.lecturer, r.submissionId, 75, "Good work"), ValidationError);  // reason needed
  EXPECT_THROW(marking.recordMark(*w.lecturer, r.submissionId, 70, "Good work", "same"), BusinessRuleError);
  marking.recordMark(*w.lecturer, r.submissionId, 75, "Good work", "Second marker moderated up");

  const auto rows = w.app->auditLog().history(*w.lecturer, w.assignmentId);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_FALSE(rows[0].entry.previousScore.has_value());
  EXPECT_DOUBLE_EQ(rows[0].entry.newScore, 70.0);
  EXPECT_EQ(rows[0].entry.reason, "Initial mark");
  EXPECT_EQ(rows[0].entry.changedBy, w.lecturerId);
  EXPECT_EQ(rows[0].entry.changedAt, day(2, 8));
  ASSERT_TRUE(rows[1].entry.previousScore.has_value());
  EXPECT_DOUBLE_EQ(*rows[1].entry.previousScore, 70.0);
  EXPECT_DOUBLE_EQ(rows[1].entry.newScore, 75.0);
  EXPECT_EQ(rows[1].entry.reason, "Second marker moderated up");
  EXPECT_EQ(rows[1].entry.changedAt, day(2, 8) + 600);
  EXPECT_EQ(rows[1].changedByName, "Lecturer One");
  EXPECT_EQ(rows[1].studentName, "Student A");
}

TEST(AuditTest, FeedbackOnlyChangeIsAlsoLogged) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  w.mark(r.submissionId, 70, "ok");
  w.app->marking().recordMark(*w.lecturer, r.submissionId, 70, "better feedback", "Clarified comments");
  EXPECT_EQ(w.app->auditLog().history(*w.lecturer, w.assignmentId).size(), 2u);
}

TEST(AuditTest, HistoryVisibleToLecturerAndDepartmentHeadOnly) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  w.mark(r.submissionId, 70);
  EXPECT_EQ(w.app->auditLog().history(*w.hod, w.assignmentId).size(), 1u);
  EXPECT_THROW(w.app->auditLog().history(*w.hodArts, w.assignmentId), NotFoundError);
  EXPECT_THROW(w.app->auditLog().history(*w.lecturer2, w.assignmentId), NotFoundError);
  EXPECT_THROW(w.app->auditLog().history(*w.a, w.assignmentId), PermissionDeniedError);
}

TEST(AuditTest, LogCannotBeAlteredOrDeleted) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  w.mark(r.submissionId, 70);
  Statement upd(w.app->database(), "UPDATE mark_audit_log SET new_score = 99");
  EXPECT_THROW(upd.run(), DatabaseError);
  Statement del(w.app->database(), "DELETE FROM mark_audit_log");
  EXPECT_THROW(del.run(), DatabaseError);
  EXPECT_EQ(w.app->auditLog().history(*w.lecturer, w.assignmentId).size(), 1u);
}

// ---- FR-12 / FR-13 / FR-15: moderation and release --------------------------------------------

TEST(ModerationTest, StudentsCannotSeeMarksUntilTheHodApproves) {
  World w;
  const auto [sa, sb] = bothSubmit(w);
  w.mark(sa, 80, "Fine work A");
  EXPECT_TRUE(w.app->results().releasedMarks(*w.a).empty());       // marked, not yet sent

  w.mark(sb, 60, "Fine work B");
  EXPECT_EQ(w.app->marking().submitForModeration(*w.lecturer, w.assignmentId), 2);
  EXPECT_TRUE(w.app->results().releasedMarks(*w.a).empty());       // under moderation
  EXPECT_TRUE(w.app->results().releasedMarks(*w.b).empty());

  EXPECT_EQ(w.app->moderation().approveMarks(*w.hod, w.assignmentId, "Checked"), 2);
  const auto marksA = w.app->results().releasedMarks(*w.a);
  ASSERT_EQ(marksA.size(), 1u);
  EXPECT_DOUBLE_EQ(marksA[0].finalScore, 80.0);
  EXPECT_EQ(marksA[0].feedback, "Fine work A");
  EXPECT_EQ(marksA[0].assignmentTitle, "Project");
  const auto marksB = w.app->results().releasedMarks(*w.b);
  ASSERT_EQ(marksB.size(), 1u);
  EXPECT_DOUBLE_EQ(marksB[0].finalScore, 60.0);
  EXPECT_TRUE(w.app->results().releasedMarks(*w.c).empty());       // never anyone else's
}

TEST(ModerationTest, ReleasedMarkShowsPenaltyExplanation) {
  World w;
  w.clock.set(w.deadline + kSecondsPerDay + 1);
  const auto r = w.submit(*w.a, w.assignmentId);
  w.mark(r.submissionId, 90, "Late but good");
  w.app->marking().submitForModeration(*w.lecturer, w.assignmentId);
  w.app->moderation().approveMarks(*w.hod, w.assignmentId);
  const auto v = w.app->results().releasedMarks(*w.a).at(0);
  EXPECT_DOUBLE_EQ(v.rawScore, 90.0);
  EXPECT_DOUBLE_EQ(v.finalScore, 70.0);
  EXPECT_DOUBLE_EQ(v.penaltyPoints, 20.0);
  EXPECT_EQ(v.lateDays, 2);
}

TEST(ModerationTest, LecturerMustMarkEverythingBeforeSending) {
  World w;
  const auto [sa, sb] = bothSubmit(w);
  (void)sb;
  w.mark(sa, 80);
  EXPECT_THROW(w.app->marking().submitForModeration(*w.lecturer, w.assignmentId), BusinessRuleError);
  EXPECT_THROW(w.app->marking().submitForModeration(*w.lecturer2, w.assignmentId), NotFoundError);
}

TEST(ModerationTest, NothingToModerateOrApproveIsAnError) {
  World w;
  EXPECT_THROW(w.app->marking().submitForModeration(*w.lecturer, w.assignmentId), BusinessRuleError);  // no submissions
  EXPECT_THROW(w.app->moderation().approveMarks(*w.hod, w.assignmentId), BusinessRuleError);
  const auto [sa, sb] = bothSubmit(w);
  w.mark(sa, 80);
  w.mark(sb, 80);
  EXPECT_THROW(w.app->moderation().approveMarks(*w.hod, w.assignmentId), BusinessRuleError);  // not sent yet
  w.app->marking().submitForModeration(*w.lecturer, w.assignmentId);
  EXPECT_THROW(w.app->marking().submitForModeration(*w.lecturer, w.assignmentId), BusinessRuleError);  // already sent
}

TEST(ModerationTest, OnlyTheDepartmentHodMayModerate) {
  World w;
  const auto [sa, sb] = bothSubmit(w);
  w.mark(sa, 80);
  w.mark(sb, 80);
  w.app->marking().submitForModeration(*w.lecturer, w.assignmentId);
  EXPECT_THROW(w.app->moderation().approveMarks(*w.hodArts, w.assignmentId), NotFoundError);
  EXPECT_THROW(w.app->moderation().rejectMarks(*w.hodArts, w.assignmentId, "no"), NotFoundError);
  EXPECT_THROW(w.app->moderation().approveMarks(*w.lecturer, w.assignmentId), PermissionDeniedError);
  EXPECT_TRUE(w.app->results().releasedMarks(*w.a).empty());
}

TEST(ModerationTest, ReviewShowsMarksFeedbackAndHistory) {
  World w;
  const auto [sa, sb] = bothSubmit(w);
  w.mark(sa, 80);
  w.mark(sb, 55);
  w.app->marking().submitForModeration(*w.lecturer, w.assignmentId);
  const ModerationReview r = w.app->moderation().reviewMarks(*w.hod, w.assignmentId);
  EXPECT_EQ(r.rows.size(), 2u);
  EXPECT_EQ(r.awaitingModeration, 2);
  EXPECT_EQ(r.course.courseCode(), "CS101");
  EXPECT_TRUE(r.decisions.empty());
}

TEST(ModerationTest, ChangesRequestedThenCorrectedThenApproved) {
  World w;
  const auto [sa, sb] = bothSubmit(w);
  w.mark(sa, 80);
  w.mark(sb, 60);
  w.app->marking().submitForModeration(*w.lecturer, w.assignmentId);

  EXPECT_THROW(w.app->moderation().rejectMarks(*w.hod, w.assignmentId, "  "), ValidationError);
  EXPECT_EQ(w.app->moderation().rejectMarks(*w.hod, w.assignmentId, "B looks too low"), 2);
  EXPECT_TRUE(w.app->results().releasedMarks(*w.b).empty());        // still hidden
  EXPECT_THROW(w.app->moderation().approveMarks(*w.hod, w.assignmentId), BusinessRuleError);

  auto rows = w.app->marking().listSubmissions(*w.lecturer, w.assignmentId);
  for (const auto& row : rows) EXPECT_EQ(row.status, SubmissionStatus::ChangesRequested);

  w.app->marking().recordMark(*w.lecturer, sb, 68, "Re-marked", "HoD asked for review");
  EXPECT_EQ(w.app->marking().submitForModeration(*w.lecturer, w.assignmentId), 2);
  EXPECT_EQ(w.app->moderation().approveMarks(*w.hod, w.assignmentId), 2);
  EXPECT_DOUBLE_EQ(w.app->results().releasedMarks(*w.b).at(0).finalScore, 68.0);

  const auto decisions = w.app->moderation().reviewMarks(*w.hod, w.assignmentId).decisions;
  ASSERT_EQ(decisions.size(), 2u);
  EXPECT_EQ(decisions[0].decision, DecisionType::ChangesRequested);
  EXPECT_EQ(decisions[0].comments, "B looks too low");
  EXPECT_EQ(decisions[1].decision, DecisionType::Approved);
  EXPECT_EQ(decisions[1].moderatorId, w.hodId);
}

TEST(ModerationTest, ResubmittingAfterReleaseHidesTheOldAttemptMark) {
  World w;
  const auto first = w.submit(*w.a, w.assignmentId, "v1.txt");
  w.mark(first.submissionId, 80);
  w.app->marking().submitForModeration(*w.lecturer, w.assignmentId);
  w.app->moderation().approveMarks(*w.hod, w.assignmentId);
  ASSERT_EQ(w.app->results().releasedMarks(*w.a).size(), 1u);

  w.clock.advance(60);
  const auto second = w.app->submissions().resubmit(*w.a, first.submissionId,
                                                    w.makeFile("v2.txt"));
  EXPECT_EQ(second.attemptNo, 2);
  EXPECT_TRUE(w.app->results().releasedMarks(*w.a).empty());
  const auto history = w.app->submissions().history(*w.a, w.assignmentId);
  ASSERT_EQ(history.size(), 2u);
  EXPECT_EQ(history[0].submissionId(), first.submissionId);
  EXPECT_EQ(history[1].submissionId(), second.submissionId);
}

TEST(ModerationTest, CorrectingAReleasedMarkWithdrawsItUntilModeratedAgain) {
  World w;
  const auto [sa, sb] = bothSubmit(w);
  w.mark(sa, 80);
  w.mark(sb, 60);
  w.app->marking().submitForModeration(*w.lecturer, w.assignmentId);
  w.app->moderation().approveMarks(*w.hod, w.assignmentId);
  ASSERT_EQ(w.app->results().releasedMarks(*w.a).size(), 1u);

  w.app->marking().recordMark(*w.lecturer, sa, 85, "Re-marked", "Arithmetic error found");
  EXPECT_TRUE(w.app->results().releasedMarks(*w.a).empty());           // withdrawn
  EXPECT_EQ(w.app->results().releasedMarks(*w.b).size(), 1u);          // others unaffected
  EXPECT_EQ(w.app->auditLog().history(*w.lecturer, w.assignmentId).size(), 3u);

  EXPECT_EQ(w.app->marking().submitForModeration(*w.lecturer, w.assignmentId), 1);
  EXPECT_EQ(w.app->moderation().approveMarks(*w.hod, w.assignmentId), 1);
  EXPECT_DOUBLE_EQ(w.app->results().releasedMarks(*w.a).at(0).finalScore, 85.0);
}

TEST(ModerationTest, LateStragglerDoesNotBlockMarksAlreadyReleased) {
  World w;
  const auto sa = w.submit(*w.a, w.assignmentId).submissionId;
  w.mark(sa, 80);
  w.app->marking().submitForModeration(*w.lecturer, w.assignmentId);
  w.app->moderation().approveMarks(*w.hod, w.assignmentId);

  w.clock.set(w.deadline + 3600);
  const auto sb = w.submit(*w.b, w.assignmentId).submissionId;  // B submits late
  EXPECT_THROW(w.app->marking().submitForModeration(*w.lecturer, w.assignmentId), BusinessRuleError);  // B unmarked
  w.mark(sb, 70);
  EXPECT_EQ(w.app->marking().submitForModeration(*w.lecturer, w.assignmentId), 1);
  EXPECT_EQ(w.app->moderation().approveMarks(*w.hod, w.assignmentId), 1);
  EXPECT_DOUBLE_EQ(w.app->results().releasedMarks(*w.b).at(0).finalScore, 60.0);
  EXPECT_DOUBLE_EQ(w.app->results().releasedMarks(*w.a).at(0).finalScore, 80.0);
}

// The database is a second line of defence: it refuses an early release even if the
// application had a bug.
TEST(ModerationTest, DatabaseRefusesReleaseWithoutApproval) {
  World w;
  const auto r = w.submit(*w.a, w.assignmentId);
  const MarkRecord m = w.mark(r.submissionId, 80);
  Statement st(w.app->database(),
               "UPDATE marks SET moderation_state = 'Released', released_at = '2026-11-02 08:00:00' "
               "WHERE mark_id = ?");
  st.bindInt(1, m.markId());
  EXPECT_THROW(st.run(), DatabaseError);
  EXPECT_TRUE(w.app->results().releasedMarks(*w.a).empty());

  // ...and also while the latest decision is "changes requested".
  w.app->marking().submitForModeration(*w.lecturer, w.assignmentId);
  w.app->moderation().rejectMarks(*w.hod, w.assignmentId, "check again");
  Statement st2(w.app->database(),
                "UPDATE marks SET moderation_state = 'Released', released_at = '2026-11-02 08:00:00' "
                "WHERE mark_id = ?");
  st2.bindInt(1, m.markId());
  EXPECT_THROW(st2.run(), DatabaseError);
}

// ---- NFR-05: 100 consecutive assessment records are stored and read back unchanged ----------

TEST(ReliabilityTest, HundredAssessmentRecordsRoundTrip) {
  World w;
  std::vector<Id> students;
  for (int i = 0; i < 100; ++i) {
    const Id id = w.add("Pupil " + std::to_string(i), "pupil" + std::to_string(i), Role::Student, std::nullopt);
    w.app->courses().enrolStudent(*w.admin, w.courseId, id);
    students.push_back(id);
  }
  std::map<Id, double> expected;  // submission id -> score
  for (int i = 0; i < 100; ++i) {
    const auto s = w.login("pupil" + std::to_string(i));
    const auto receipt = w.submit(*s, w.assignmentId);
    const double score = (i * 37) % 101 + (i % 4) * 0.25;  // 0..100 in quarter steps
    const double bounded = std::min(score, 100.0);
    w.mark(receipt.submissionId, bounded, "Feedback " + std::to_string(i));
    expected[receipt.submissionId] = bounded;
  }
  const auto rows = w.app->marking().listSubmissions(*w.lecturer, w.assignmentId);
  ASSERT_EQ(rows.size(), 100u);
  for (const MarkingRow& row : rows) {
    ASSERT_TRUE(row.mark.has_value());
    EXPECT_DOUBLE_EQ(row.mark->rawScore(), expected.at(row.submission.submissionId()));
    EXPECT_DOUBLE_EQ(row.mark->finalScore(), expected.at(row.submission.submissionId()));
    EXPECT_FALSE(row.mark->feedback().empty());
  }
}
