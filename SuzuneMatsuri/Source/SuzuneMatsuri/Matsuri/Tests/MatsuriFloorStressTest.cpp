// Suzune's Night Festival - title -> floor 1 again and again, with fresh layouts, rendering a few seconds of
// each, to shake out rendering faults that only some layouts or transitions trigger (not in the SuzuneMatsuri suite).
// Run: Automation RunTests MatsuriArt.FloorStress   (-windowed -ResX=1280 -ResY=720; matsuri.StressLoads N)

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "HAL/IConsoleManager.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriFloors.h"
#include "MatsuriFlow.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriMainMenuWidget.h"

using namespace MatsuriTest;

namespace
{
	TAutoConsoleVariable<int32> CVarStressLoads(
		TEXT("matsuri.StressLoads"), 20,
		TEXT("How many times MatsuriArt.FloorStress loads floor 1."));

	constexpr int32 FramesPerLoad = 180;	// a few seconds of rendering: shadows, Lumen and Nanite settle in
	constexpr int32 TitleFrames = 30;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriFloorStressTest, "MatsuriArt.FloorStress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriFloorStressTest::RunTest(const FString&)
{
	const int32 Loads = FMath::Max(1, CVarStressLoads.GetValueOnGameThread());
	TSharedRef<TWeakObjectPtr<UWorld>> Leaving = MakeShared<TWeakObjectPtr<UWorld>>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	for (int32 Load = 0; Load < Loads; ++Load)
	{
		Steps.Add({ FString::Printf(TEXT("title %d"), Load + 1), [this, Load, Leaving](UWorld* World, int32 Frame)
		{
			// As a player starts a run: from the title screen (every crash seen came on that path).
			if (!IsMap(World, Matsuri::Maps::Title) || !FindWidget<UMatsuriMainMenuWidget>(World))
			{
				if (Frame == 0 && Load > 0)
				{
					Matsuri::Flow::ReturnToTitle(World);
				}
				return Frame == 0 ? EStep::Continue : EStep::Wait;
			}
			if (Load == 0)
			{
				Run(World)->UseRecordSlot(TestRecordSlot);
			}
			if (Frame < TitleFrames)
			{
				return EStep::Continue;
			}
			*Leaving = World;
			Matsuri::Flow::StartAtFloor(World, EMatsuriHero::Mio, 0);	// matsuri.Seed 0: a new layout each time
			return EStep::Done;
		} });
		Steps.Add({ FString::Printf(TEXT("render %d"), Load + 1), [Load, Leaving](UWorld* World, int32 Frame)
		{
			// Wait for the new 1F, not the one being left.
			if (World == Leaving->Get() || !IsMap(World, Matsuri::Floors::MapFor(0)) || !Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
			{
				return EStep::Wait;
			}
			if (Frame == 0)
			{
				UE_LOG(LogTemp, Log, TEXT("FloorStress: load %d on screen"), Load + 1);
			}
			return DoneIf(Frame >= FramesPerLoad);
		} });
	}

	SetCVar(TEXT("matsuri.Story"), false);
	SetCVar(TEXT("matsuri.God"), true);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Maps::Title));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.Story"), true);
		SetCVar(TEXT("matsuri.God"), false);
	}, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
