// Suzune's Night Festival - story data tests.

#include "Misc/AutomationTest.h"
#include "MatsuriStory.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags StoryTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriStoryChaptersTest, "SuzuneMatsuri.Story.Chapters",
	StoryTestFlags)
bool FMatsuriStoryChaptersTest::RunTest(const FString&)
{
	for (int32 Floor = 0; Floor <= 3; ++Floor)
	{
		for (int32 Hero = 0; Hero < Matsuri::Heroes::Count; ++Hero)
		{
			const FMatsuriStoryChapter Chapter = Matsuri::Story::ChapterFor(Floor, static_cast<EMatsuriHero>(Hero));
			const FString Where = FString::Printf(TEXT("floor %d hero %d"), Floor + 1, Hero + 1);
			TestTrue(Where + TEXT(" is valid"), Chapter.IsValid());
			TestTrue(Where + TEXT(" has multiple lines"), Chapter.Lines.Num() >= 3);
			for (const FMatsuriStoryLine& Line : Chapter.Lines)
			{
				TestFalse(Where + TEXT(" has a speaker"), Line.Speaker.IsEmpty());
				TestFalse(Where + TEXT(" has dialogue"), Line.Text.IsEmpty());
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriStoryEndingTest, "SuzuneMatsuri.Story.GuardianEnding",
	StoryTestFlags)
bool FMatsuriStoryEndingTest::RunTest(const FString&)
{
	TSet<FString> HeroLines;
	for (int32 Hero = 0; Hero < Matsuri::Heroes::Count; ++Hero)
	{
		const FMatsuriStoryChapter Ending = Matsuri::Story::EndingFor(static_cast<EMatsuriHero>(Hero));
		TestTrue(TEXT("ending is valid"), Ending.IsValid());
		TestTrue(TEXT("ending names the barrier repair"), Ending.Objective.ToString().Contains(TEXT("修复")));
		HeroLines.Add(Ending.Lines[1].Text.ToString());
	}
	TestEqual(TEXT("each heroine has a distinct ending line"), HeroLines.Num(), Matsuri::Heroes::Count);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriStoryToggleTest, "SuzuneMatsuri.Story.DefaultEnabled",
	StoryTestFlags)
bool FMatsuriStoryToggleTest::RunTest(const FString&)
{
	TestTrue(TEXT("story is enabled by default"), Matsuri::Story::IsEnabled());
	return true;
}

#endif
