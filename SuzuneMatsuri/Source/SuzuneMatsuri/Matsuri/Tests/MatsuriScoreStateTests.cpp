// Suzune's Night Festival - score state unit tests.
// Run: Automation RunTests SuzuneMatsuri.Score

#include "Misc/AutomationTest.h"
#include "MatsuriScoreState.h"
#include "MatsuriTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags MatsuriTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriPointsTest, "SuzuneMatsuri.Score.PointsPerCollectible", MatsuriTestFlags)
bool FMatsuriPointsTest::RunTest(const FString&)
{
	TestEqual(TEXT("koban is +1"), Matsuri::PointsFor(EMatsuriCollectible::Koban), 1);
	TestEqual(TEXT("ginka is +2"), Matsuri::PointsFor(EMatsuriCollectible::Ginka), 2);
	TestEqual(TEXT("magatama is +5"), Matsuri::PointsFor(EMatsuriCollectible::Magatama), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriScoreAccumulatesTest, "SuzuneMatsuri.Score.AccumulatesRunAndFloor", MatsuriTestFlags)
bool FMatsuriScoreAccumulatesTest::RunTest(const FString&)
{
	FMatsuriScoreState State;
	State.BeginFloor(10);

	State.AddPoints(1);
	State.AddPoints(5);

	TestEqual(TEXT("floor score"), State.FloorScore, 6);
	TestEqual(TEXT("run score"), State.RunScore, 6);
	TestFalse(TEXT("not cleared below target"), State.bFloorCleared);
	TestEqual(TEXT("progress"), State.GetFloorProgress(), 0.6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriScoreClearsOnceTest, "SuzuneMatsuri.Score.ReportsClearExactlyOnce", MatsuriTestFlags)
bool FMatsuriScoreClearsOnceTest::RunTest(const FString&)
{
	FMatsuriScoreState State;
	State.BeginFloor(3);

	const bool bFirst = State.AddPoints(2);
	const bool bCrossing = State.AddPoints(2);
	const bool bAfter = State.AddPoints(5);

	TestFalse(TEXT("below target does not clear"), bFirst);
	TestTrue(TEXT("crossing target clears"), bCrossing);
	TestFalse(TEXT("already cleared does not report again"), bAfter);
	TestTrue(TEXT("cleared flag set"), State.bFloorCleared);
	TestEqual(TEXT("progress is clamped"), State.GetFloorProgress(), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriScoreExactTargetTest, "SuzuneMatsuri.Score.ExactTargetClears", MatsuriTestFlags)
bool FMatsuriScoreExactTargetTest::RunTest(const FString&)
{
	FMatsuriScoreState State;
	State.BeginFloor(5);

	TestTrue(TEXT("score == target counts as >= target"), State.AddPoints(5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriScoreIgnoresInvalidTest, "SuzuneMatsuri.Score.IgnoresNonPositivePoints", MatsuriTestFlags)
bool FMatsuriScoreIgnoresInvalidTest::RunTest(const FString&)
{
	FMatsuriScoreState State;
	State.BeginFloor(1);

	TestFalse(TEXT("zero ignored"), State.AddPoints(0));
	TestFalse(TEXT("negative ignored"), State.AddPoints(-4));
	TestEqual(TEXT("score unchanged"), State.RunScore, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriScoreFloorResetTest, "SuzuneMatsuri.Score.NewFloorKeepsRunScore", MatsuriTestFlags)
bool FMatsuriScoreFloorResetTest::RunTest(const FString&)
{
	FMatsuriScoreState State;
	State.BeginFloor(2);
	State.AddPoints(4);

	State.BeginFloor(0);

	TestEqual(TEXT("run score kept"), State.RunScore, 4);
	TestEqual(TEXT("floor score reset"), State.FloorScore, 0);
	TestFalse(TEXT("cleared flag reset"), State.bFloorCleared);
	TestEqual(TEXT("target clamped to 1"), State.FloorTarget, 1);

	State.ResetRun();
	TestEqual(TEXT("run reset"), State.RunScore, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
