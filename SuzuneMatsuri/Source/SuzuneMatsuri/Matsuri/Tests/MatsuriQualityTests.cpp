// Suzune's Night Festival - the play-quality caps.
// Run: Automation RunTests SuzuneMatsuri.Quality

#include "Misc/AutomationTest.h"
#include "MatsuriQuality.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	Scalability::FQualityLevels AllAt(int32 Level)
	{
		Scalability::FQualityLevels Levels;
		Levels.SetFromSingleQualityLevel(Level);
		return Levels;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriQualityCapTest, "SuzuneMatsuri.Quality.CapsCinematicForPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriQualityCapTest::RunTest(const FString&)
{
	constexpr int32 Cinematic = 4, Epic = 3, High = 2, Low = 0;

	const Scalability::FQualityLevels FromCinematic = Matsuri::Quality::Capped(AllAt(Cinematic), Epic, High);
	TestEqual(TEXT("shadows come down to Epic"), FromCinematic.ShadowQuality, Epic);
	TestEqual(TEXT("post-processing comes down to Epic"), FromCinematic.PostProcessQuality, Epic);
	TestEqual(TEXT("GI comes down to High"), FromCinematic.GlobalIlluminationQuality, High);
	TestEqual(TEXT("resolution is left alone"), FromCinematic.ResolutionQuality, AllAt(Cinematic).ResolutionQuality);

	TestTrue(TEXT("a low setting is never raised"), Matsuri::Quality::Capped(AllAt(Low), Epic, High) == AllAt(Low));
	TestTrue(TEXT("-1 means no cap"), Matsuri::Quality::Capped(AllAt(Cinematic), -1, -1) == AllAt(Cinematic));
	TestEqual(TEXT("the group cap also bounds GI"), Matsuri::Quality::Capped(AllAt(Cinematic), High - 1, -1).GlobalIlluminationQuality, High - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriRenderHeightTest, "SuzuneMatsuri.Quality.RendersTallWindowsAtAboutNineHundredLines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriRenderHeightTest::RunTest(const FString&)
{
	using Matsuri::Quality::ScreenPercentageFor;
	TestEqual(TEXT("a 1440-line window renders at 62.5%"), ScreenPercentageFor(1440, 900), 62.5f);
	TestEqual(TEXT("a 1080-line window at 83%"), FMath::RoundToInt(ScreenPercentageFor(1080, 900)), 83);
	TestEqual(TEXT("a small window stays at full resolution"), ScreenPercentageFor(849, 900), 100.0f);
	TestEqual(TEXT("never below half: TSR would smear"), ScreenPercentageFor(4320, 900), 50.0f);
	TestEqual(TEXT("0 turns it off"), ScreenPercentageFor(1440, 0), 100.0f);
	TestEqual(TEXT("no viewport: full resolution"), ScreenPercentageFor(0, 900), 100.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
