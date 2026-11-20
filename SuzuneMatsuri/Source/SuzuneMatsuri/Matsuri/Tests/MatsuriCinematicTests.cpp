#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "MatsuriCinematicWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriCinematicPathsTest, "SuzuneMatsuri.Cinematic.Paths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriCinematicPathsTest::RunTest(const FString&)
{
	TestEqual(TEXT("first chapter movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::FirstFloor), FString(TEXT("Movies/matsuri_intro_1.mp4")));
	TestEqual(TEXT("second chapter movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::SecondFloor), FString(TEXT("Movies/matsuri_intro_2.mp4")));
	TestEqual(TEXT("third chapter movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::ThirdFloor), FString(TEXT("Movies/matsuri_intro_3.mp4")));
	TestEqual(TEXT("defeat movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::Defeat), FString(TEXT("Movies/matsuri_defeat.mp4")));
	TestEqual(TEXT("second chapter movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::SecondFloor), FString(TEXT("Movies/matsuri_intro_2.mp4")));
	TestEqual(TEXT("third chapter movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::ThirdFloor), FString(TEXT("Movies/matsuri_intro_3.mp4")));
	TestEqual(TEXT("defeat movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::Defeat), FString(TEXT("Movies/matsuri_defeat.mp4")));
	TestEqual(TEXT("victory movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::Victory), FString(TEXT("Movies/matsuri_victory.mp4")));
	TestTrue(TEXT("movies resolve under project Content"), UMatsuriCinematicWidget::AbsoluteMoviePath(EMatsuriCinematic::FirstFloor).StartsWith(FPaths::ProjectContentDir()));
	TestTrue(TEXT("isolated missing test path is not a production mapping"), !UMatsuriCinematicWidget::AbsoluteMoviePath(EMatsuriCinematic::FirstFloor).Contains(TEXT("Saved")));
	return true;
}

#endif
