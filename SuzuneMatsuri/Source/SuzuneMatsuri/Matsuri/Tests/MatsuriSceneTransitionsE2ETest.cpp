// Suzune's Night Festival - scene changes go through a loading screen and a title card (PIE).
// Playtest: "make the transitions between scenes good, add a loading screen". Title -> 1F -> 2F:
// leaving fades into a loading screen naming the destination (the old level stays a moment); arriving opens
// on the floor's title card, which fades into play by itself.
// Run: Automation RunTests SuzuneMatsuri.E2E.SceneTransitions

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "MatsuriFloors.h"
#include "MatsuriFlow.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriLoadingWidget.h"
#include "MatsuriMainMenuWidget.h"

using namespace MatsuriTest;

namespace
{
	constexpr double CardGoneWithin = 2.0;	// hold + fade, with room to spare

	UMatsuriLoadingWidget* FindLoading(UWorld* World, bool bArriving)
	{
		for (TObjectIterator<UMatsuriLoadingWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport() && It->IsArriving() == bArriving)
			{
				return *It;
			}
		}
		return nullptr;
	}

	struct FTransitionState
	{
		double ArrivedAt = 0.0;
	};

	/** Waits for FloorIndex, checks its title card, and lets it fade. */
	FStepRunner::FStep ArriveOnFloor(FAutomationTestBase& Test, TSharedRef<FTransitionState> State, int32 FloorIndex, const TCHAR* Shot)
	{
		return [&Test, State, FloorIndex, Shot](UWorld* World, int32 Frame)
		{
			if (!IsMap(World, Matsuri::Floors::MapFor(FloorIndex)) || !Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
			{
				return EStep::Wait;
			}
			UMatsuriLoadingWidget* Card = FindLoading(World, true);
			if (Frame == 0)
			{
				State->ArrivedAt = World->GetTimeSeconds();
				const FText Expected = Matsuri::Floors::ThemeFor(FloorIndex).Title;
				Test.TestNotNull(TEXT("the floor opens on its title card"), Card);
				if (Card)
				{
					Test.TestEqual(TEXT("the card names the floor"), Card->GetHeadline().ToString(), Expected.ToString());
					Test.TestTrue(TEXT("it starts by covering the scene"), Card->GetCoverage() >= 0.9f);
				}
				return EStep::Continue;
			}
			if (Frame == 3 && Shot && FApp::CanEverRender())
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/MatsuriShots") / (FString(Shot) + TEXT(".png")), true, false);
			}
			if (Card && World->GetTimeSeconds() - State->ArrivedAt < CardGoneWithin)
			{
				return EStep::Continue;
			}
			Test.TestNull(FString::Printf(TEXT("the card faded into play within %.1f s"), CardGoneWithin), Card);
			return EStep::Done;
		};
	}

	/** After a Flow call: the scene being left fades into a loading screen naming the destination. */
	FStepRunner::FStep LeaveFor(FAutomationTestBase& Test, int32 FloorIndex, TFunction<void(UWorld*)> Go)
	{
		return [&Test, FloorIndex, Go](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				Go(World);
				return EStep::Continue;
			}
			UMatsuriLoadingWidget* Loading = FindLoading(World, false);
			Test.TestNotNull(TEXT("leaving fades into a loading screen"), Loading);
			if (Loading)
			{
				Test.TestTrue(TEXT("it names where we are going"), Loading->GetHeadline().ToString().Contains(Matsuri::Floors::ThemeFor(FloorIndex).Title.ToString()));
			}
			return EStep::Done;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriSceneTransitionsE2ETest, "SuzuneMatsuri.E2E.SceneTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriSceneTransitionsE2ETest::RunTest(const FString&)
{
	TSharedRef<FTransitionState> State = MakeShared<FTransitionState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("title fades in"), [this](UWorld* World, int32 Frame)
	{
		if (!IsMap(World, Matsuri::Maps::Title) || !FindWidget<UMatsuriMainMenuWidget>(World))
		{
			return EStep::Wait;
		}
		Run(World)->UseRecordSlot(TestRecordSlot);
		TestNotNull(TEXT("the title fades in from the loading screen"), FindLoading(World, true));
		return EStep::Done;
	} });
	Steps.Add({ TEXT("title -> 1F"), LeaveFor(*this, 0, [](UWorld* World) { Matsuri::Flow::StartNewRun(World, EMatsuriHero::Mio); }) });
	Steps.Add({ TEXT("1F title card"), ArriveOnFloor(*this, State, 0, TEXT("60_floor_card")) });
	Steps.Add({ TEXT("1F -> 2F"), LeaveFor(*this, 1, [](UWorld* World)
	{
		Run(World)->SetFloorTargetForTesting(0);	// the floor counts as cleared
		Matsuri::Flow::ContinueWithCharm(World, static_cast<EMatsuriOmamori>(0));
	}) });
	Steps.Add({ TEXT("2F title card"), ArriveOnFloor(*this, State, 1, nullptr) });

	SetCVar(TEXT("matsuri.Threats"), false);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Maps::Title));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
