#include <cmath>

#include "TestSupport.hpp"
#include "lms/common/Validation.hpp"
#include "lms/services/StandardLatePenaltyPolicy.hpp"

using namespace lms;
using namespace lms::test;

TEST(TimeTest, DbStringRoundTrip) {
  const Timestamp t = makeUtc(2026, 2, 28, 23, 59, 59);
  EXPECT_EQ(toDbString(t), "2026-02-28 23:59:59");
  EXPECT_EQ(fromDbString("2026-02-28 23:59:59"), t);
  EXPECT_EQ(toDbString(t + 1), "2026-03-01 00:00:00");
  EXPECT_EQ(fromDbString("2028-02-29 12:00:00") - fromDbString("2028-02-28 12:00:00"), kSecondsPerDay);
  EXPECT_EQ(toDbString(0), "1970-01-01 00:00:00");
}

TEST(TimeTest, RejectsMalformedDates) {
  EXPECT_THROW(fromDbString("2026-13-01 00:00:00"), ValidationError);
  EXPECT_THROW(fromDbString("2026-02-30 00:00:00"), ValidationError);
  EXPECT_THROW(fromDbString("garbage"), ValidationError);
  EXPECT_THROW(fromDbString("2026-02-03 10:00:00 extra"), ValidationError);
}

TEST(TimeTest, LocalInputParsing) {
  EXPECT_TRUE(parseLocalDateTime("2026-11-30 17:00").has_value());
  EXPECT_FALSE(parseLocalDateTime("2026-11-31 17:00").has_value());
  EXPECT_FALSE(parseLocalDateTime("2026-11-30").has_value());
  EXPECT_FALSE(parseLocalDateTime("30/11/2026 17:00").has_value());
  EXPECT_FALSE(parseLocalDateTime("2026-11-30 25:00").has_value());
  const auto t = parseLocalDateTime("2026-11-30 17:00");
  EXPECT_EQ(toLocalString(*t), "2026-11-30 17:00");
}

// Boundary value analysis for the late-day calculation (FR-09).
TEST(TimeTest, WholeDaysLateBoundaries) {
  const Timestamp d = day(5, 9);
  EXPECT_EQ(wholeDaysLate(d, d - 1000), 0);
  EXPECT_EQ(wholeDaysLate(d, d), 0);  // exactly on the deadline is on time
  EXPECT_EQ(wholeDaysLate(d, d + 1), 1);
  EXPECT_EQ(wholeDaysLate(d, d + kSecondsPerDay), 1);
  EXPECT_EQ(wholeDaysLate(d, d + kSecondsPerDay + 1), 2);
  EXPECT_EQ(wholeDaysLate(d, d + 3 * kSecondsPerDay), 3);
  EXPECT_EQ(wholeDaysLate(d, d + 3 * kSecondsPerDay + 1), 4);
}

TEST(ValidationTest, Username) {
  EXPECT_EQ(validation::normaliseUsername("  Alice.B-1 "), "alice.b-1");
  EXPECT_THROW(validation::normaliseUsername("ab"), ValidationError);
  EXPECT_THROW(validation::normaliseUsername("has space"), ValidationError);
  EXPECT_THROW(validation::normaliseUsername("x'; DROP TABLE users;--"), ValidationError);
  EXPECT_THROW(validation::normaliseUsername(std::string(33, 'a')), ValidationError);
  EXPECT_NO_THROW(validation::normaliseUsername(std::string(32, 'a')));
}

TEST(ValidationTest, PasswordStrength) {
  EXPECT_NO_THROW(validation::requirePasswordStrength("abcdefg1"));
  EXPECT_THROW(validation::requirePasswordStrength("abcdef1"), ValidationError);   // 7 chars
  EXPECT_THROW(validation::requirePasswordStrength("abcdefgh"), ValidationError);  // no digit
  EXPECT_THROW(validation::requirePasswordStrength("12345678"), ValidationError);  // no letter
  EXPECT_THROW(validation::requirePasswordStrength(std::string(129, 'a') + "1"), ValidationError);
}

TEST(ValidationTest, ScoreRangeAndRounding) {
  EXPECT_DOUBLE_EQ(validation::requireScore(0.0, 100, "Mark"), 0.0);
  EXPECT_DOUBLE_EQ(validation::requireScore(100.0, 100, "Mark"), 100.0);
  EXPECT_DOUBLE_EQ(validation::requireScore(67.456, 100, "Mark"), 67.46);
  EXPECT_THROW(validation::requireScore(-0.01, 100, "Mark"), ValidationError);
  EXPECT_THROW(validation::requireScore(100.01, 100, "Mark"), ValidationError);
  EXPECT_THROW(validation::requireScore(std::nan(""), 100, "Mark"), ValidationError);
  EXPECT_THROW(validation::requireScore(INFINITY, 100, "Mark"), ValidationError);
}

TEST(ValidationTest, TextRules) {
  EXPECT_EQ(validation::requireText("  hi  ", "Field", 10), "hi");
  EXPECT_THROW(validation::requireText("   ", "Field", 10), ValidationError);
  EXPECT_NO_THROW(validation::requireText("   ", "Field", 10, true));
  EXPECT_THROW(validation::requireText(std::string(11, 'x'), "Field", 10), ValidationError);
  EXPECT_THROW(validation::requireText(std::string("a\x01" "b"), "Field", 10), ValidationError);
}

TEST(PasswordHasherTest, SaltedAndVerifiable) {
  PasswordHasher h(PasswordHasher::Cost::Minimal);
  const std::string h1 = h.hash("Secret123");
  const std::string h2 = h.hash("Secret123");
  EXPECT_NE(h1, h2);  // a fresh random salt each time
  EXPECT_EQ(h1.rfind("$argon2id$", 0), 0u);
  EXPECT_EQ(h1.find("Secret123"), std::string::npos);
  EXPECT_TRUE(PasswordHasher::verify(h1, "Secret123"));
  EXPECT_TRUE(PasswordHasher::verify(h2, "Secret123"));
  EXPECT_FALSE(PasswordHasher::verify(h1, "secret123"));
  EXPECT_FALSE(PasswordHasher::verify("not a hash", "Secret123"));
  EXPECT_FALSE(PasswordHasher::verify("", ""));
}

TEST(PasswordHasherTest, DetectsWeakerParameters) {
  PasswordHasher weak(PasswordHasher::Cost::Minimal);
  PasswordHasher strong(PasswordHasher::Cost::Interactive);
  EXPECT_TRUE(strong.needsRehash(weak.hash("Secret123")));
  EXPECT_FALSE(strong.needsRehash(strong.hash("Secret123")));
}

// Equivalence classes / boundaries for the late-penalty strategy (FR-09).
// calculatePenalty(lateDays, mark): `mark` is the available mark; the result is the deduction.
TEST(LatePenaltyPolicyTest, TenPercentPerDayUpToThree) {
  StandardLatePenaltyPolicy p;
  EXPECT_DOUBLE_EQ(p.calculatePenalty(0, 100), 0.0);
  EXPECT_DOUBLE_EQ(p.calculatePenalty(1, 100), 10.0);
  EXPECT_DOUBLE_EQ(p.calculatePenalty(2, 100), 20.0);
  EXPECT_DOUBLE_EQ(p.calculatePenalty(3, 100), 30.0);
}

TEST(LatePenaltyPolicyTest, ZeroOutcomeAfterThreeDays) {
  StandardLatePenaltyPolicy p;
  EXPECT_DOUBLE_EQ(p.calculatePenalty(4, 100), 100.0);  // the whole available mark is lost
  EXPECT_DOUBLE_EQ(p.calculatePenalty(30, 50), 50.0);
}

TEST(LatePenaltyPolicyTest, PenaltyIsBasedOnTheAvailableMarkAndRounded) {
  StandardLatePenaltyPolicy p;
  EXPECT_DOUBLE_EQ(p.calculatePenalty(1, 50), 5.0);       // 10% of the available 50
  EXPECT_DOUBLE_EQ(p.calculatePenalty(3, 33.3), 9.99);    // rounded to 2 decimal places
  EXPECT_DOUBLE_EQ(p.calculatePenalty(-2, 100), 0.0);
}

TEST(LatePenaltyPolicyTest, ConfigurableStrategy) {
  StandardLatePenaltyPolicy p(0.25, 2);
  EXPECT_DOUBLE_EQ(p.calculatePenalty(2, 100), 50.0);
  EXPECT_DOUBLE_EQ(p.calculatePenalty(3, 100), 100.0);
}
