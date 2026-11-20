// Suzune's Night Festival - yokai tactics (unit tests on the pure decisions).
// Run: Automation RunTests SuzuneMatsuri.Brain

#include "Misc/AutomationTest.h"
#include "MatsuriYokaiBrain.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags MatsuriTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

using namespace Matsuri::YokaiBrain;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriChochinBrainTest, "SuzuneMatsuri.Brain.ChochinStalksFromTheBlindSide", MatsuriTestFlags)
bool FMatsuriChochinBrainTest::RunTest(const FString&)
{
	TestTrue(TEXT("looked at, it only creeps"), ChochinMove(true, 1500.0f) == EChochinMove::Creep && ChochinMove(true, 100.0f) == EChochinMove::Creep);
	TestTrue(TEXT("unwatched and far, it stalks"), ChochinMove(false, 1500.0f) == EChochinMove::Stalk);
	TestTrue(TEXT("unwatched and close, it lunges"), ChochinMove(false, ChochinLungeRange - 1.0f) == EChochinMove::Lunge);
	TestTrue(TEXT("watched, it edges closer, but at a fraction of its stalk"), ChochinSpeed(EChochinMove::Creep) > 0.0f && ChochinSpeed(EChochinMove::Creep) <= ChochinSpeed(EChochinMove::Stalk) * 0.2f);
	TestTrue(TEXT("the lunge is the fastest"), ChochinSpeed(EChochinMove::Lunge) > ChochinSpeed(EChochinMove::Stalk));
	TestTrue(TEXT("stalking outpaces its old open chase (250)"), ChochinSpeed(EChochinMove::Stalk) > 250.0f);

	const FVector Eye(0.0f, 0.0f, 150.0f);
	TestTrue(TEXT("straight ahead is in view"), IsInViewCone(Eye, FVector::ForwardVector, FVector(800.0f, 50.0f, 150.0f)));
	TestFalse(TEXT("behind is not"), IsInViewCone(Eye, FVector::ForwardVector, FVector(-800.0f, 0.0f, 150.0f)));
	TestFalse(TEXT("far off to the side is not"), IsInViewCone(Eye, FVector::ForwardVector, FVector(100.0f, 800.0f, 150.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriKasaBrainTest, "SuzuneMatsuri.Brain.KasaLurksAndPounces", MatsuriTestFlags)
bool FMatsuriKasaBrainTest::RunTest(const FString&)
{
	TestTrue(TEXT("unseen, one corner away: lurk"), KasaMove(false, 700.0f, 1) == EKasaMove::Lurk);
	TestTrue(TEXT("off the maze, unseen nearby: lurk"), KasaMove(false, 700.0f, INDEX_NONE) == EKasaMove::Lurk);
	TestTrue(TEXT("in sight and in reach: pounce"), KasaMove(true, KasaPounceRange - 1.0f, 1) == EKasaMove::Pounce);
	TestTrue(TEXT("in sight but far: close in along the corridors"), KasaMove(true, 1500.0f, 4) == EKasaMove::Path);
	TestTrue(TEXT("unseen and several corners away: close in"), KasaMove(false, 900.0f, 3) == EKasaMove::Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriKappaBrainTest, "SuzuneMatsuri.Brain.KappaFightsFromCover", MatsuriTestFlags)
bool FMatsuriKappaBrainTest::RunTest(const FString&)
{
	TestTrue(TEXT("ready and in sight: throw"), KappaMove(true, true, true) == EKappaMove::Throw);
	TestTrue(TEXT("ready but unseen: peek out"), KappaMove(false, true, true) == EKappaMove::Peek);
	TestTrue(TEXT("reloading in the open: hide"), KappaMove(true, false, true) == EKappaMove::Hide);
	TestTrue(TEXT("reloading in cover: hold"), KappaMove(false, false, true) == EKappaMove::Hold);
	// Chase regression: a kappa corridors away must come closer, not sit in cover.
	TestTrue(TEXT("out of range: approach, whatever else"), KappaMove(false, false, false) == EKappaMove::Approach && KappaMove(true, true, false) == EKappaMove::Approach);

	const FVector From(0.0f, 0.0f, 0.0f);
	TArray<FCoverSpot> Spots = {
		{ FVector(100.0f, 0.0f, 0.0f), true, false },	// near but exposed
		{ FVector(300.0f, 0.0f, 0.0f), true, true },		// hidden
		{ FVector(150.0f, 50.0f, 0.0f), false, true },	// hidden but walled off
		{ FVector(500.0f, 0.0f, 0.0f), true, true } };	// hidden, farther
	TestEqual(TEXT("the nearest reachable hidden spot"), PickCover(Spots, From), 1);
	TestEqual(TEXT("no cover at all"), PickCover({ { FVector(100.0f, 0.0f, 0.0f), true, false } }, From), static_cast<int32>(INDEX_NONE));
	// Peeking from cover: step to the nearest spot the player can see (straight at them may be through the block).
	TestEqual(TEXT("the nearest reachable spot in view"), PickPeek(Spots, From), 0);
	TestEqual(TEXT("nowhere to peek from"), PickPeek({ { FVector(100.0f, 0.0f, 0.0f), true, true } }, From), static_cast<int32>(INDEX_NONE));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
