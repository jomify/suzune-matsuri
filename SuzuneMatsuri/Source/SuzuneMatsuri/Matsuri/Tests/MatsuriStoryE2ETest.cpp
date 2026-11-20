// Rendered end-to-end coverage for the video-only story flow.

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "GameFramework/PlayerController.h"
#include "MatsuriBossGameMode.h"
#include "MatsuriCinematicWidget.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriFlow.h"
#include "MatsuriGameOverWidget.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriStoryWidget.h"
#include "MatsuriVictoryWidget.h"

using namespace MatsuriTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriStoryE2ETest, "SuzuneMatsuri.E2E.Story",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriStoryE2ETest::RunTest(const FString&)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("Video story flow is covered by the rendered test run."));
		return true;
	}

	struct FState { bool bBossVideoSkipped = false; int32 OpenFrame = -1; };
	TSharedRef<FState> State = MakeShared<FState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;

	Steps.Add({ TEXT("first-floor video"), [this, State](UWorld* World, int32 Frame)
	{
		if (!IsMap(World, Matsuri::Floors::MapFor(0)) || !Hero(World) || !FindWidget<UMatsuriHUDWidget>(World)) return EStep::Wait;
		if (Frame == 0) Run(World)->UseRecordSlot(TestRecordSlot);
		UMatsuriCinematicWidget* Movie = FindWidget<UMatsuriCinematicWidget>(World);
		TestFalse(TEXT("first floor has no text story card"), FindWidget<UMatsuriStoryWidget>(World) != nullptr);
		if (!Movie || !Movie->HasMediaOpened()) return EStep::Continue;
		if (State->OpenFrame < 0) State->OpenFrame = Frame;
		TestTrue(TEXT("first floor video pauses play"), World->IsPaused());
		if (!SettleAndShoot(Frame - State->OpenFrame, TEXT("story_video_opening"), 20)) return EStep::Continue;
		Movie->Finish();
		TestFalse(TEXT("first floor video resumes play"), World->IsPaused());
		return EStep::Done;
	} });

	Steps.Add({ TEXT("boss opens without text"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			UseTestBossBrain();
			Matsuri::Flow::StartAtFloor(World, Run(World)->GetRunState().Hero, Matsuri::Floors::BossFloor);
			return EStep::Continue;
		}
		if (!IsMap(World, Matsuri::Floors::MapFor(Matsuri::Floors::BossFloor)) || !Hero(World)) return EStep::Continue;
		TestFalse(TEXT("boss entrance has no text story card"), FindWidget<UMatsuriStoryWidget>(World) != nullptr);
		TestFalse(TEXT("boss opens without a story pause"), World->IsPaused());
		State->bBossVideoSkipped = true;
		return EStep::Done;
	} });

	Steps.Add({ TEXT("victory video and result"), [this, State](UWorld* World, int32 Frame)
	{
		AMatsuriBossGameMode* Mode = World ? World->GetAuthGameMode<AMatsuriBossGameMode>() : nullptr;
		if (!Mode || !State->bBossVideoSkipped) return EStep::Wait;
		if (Frame == 0) { Mode->DevClearFloor(); return EStep::Continue; }
		TestFalse(TEXT("victory has no text story card"), FindWidget<UMatsuriStoryWidget>(World) != nullptr);
		UMatsuriCinematicWidget* Movie = FindWidget<UMatsuriCinematicWidget>(World);
		if (!Movie || !Movie->HasMediaOpened()) return EStep::Continue;
		TestTrue(TEXT("victory video pauses result"), World->IsPaused());
		Movie->Finish();
		TestNotNull(TEXT("victory result follows the video"), FindWidget<UMatsuriVictoryWidget>(World));
		TestTrue(TEXT("victory result is paused"), World->IsPaused());
		return EStep::Done;
	} });

	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.Story"), true);
	SetCVar(TEXT("matsuri.Replay"), false);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Sakura));
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps), true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([]
	{
		SetCVar(TEXT("matsuri.Threats"), true);
		SetCVar(TEXT("matsuri.Story"), true);
		SetCVar(TEXT("matsuri.Replay"), true);
		RestoreBossBrain();
	}, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriDefeatCinematicE2ETest, "SuzuneMatsuri.E2E.DefeatCinematic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriDefeatCinematicE2ETest::RunTest(const FString&)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("Defeat cinematic is covered by the rendered test run."));
		return true;
	}

	struct FState { bool bKilled = false; bool bDefeatSkipped = false; };
	TSharedRef<FState> State = MakeShared<FState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("skip first-floor video"), [this, State](UWorld* World, int32 Frame)
	{
		if (!IsMap(World, Matsuri::Floors::MapFor(0)) || !Hero(World) || !FindWidget<UMatsuriHUDWidget>(World)) return EStep::Wait;
		if (Frame == 0) Run(World)->UseRecordSlot(TestRecordSlot);
		UMatsuriCinematicWidget* Movie = FindWidget<UMatsuriCinematicWidget>(World);
		if (!Movie || !Movie->HasMediaOpened()) return EStep::Continue;
		Movie->Finish();
		TestFalse(TEXT("first-floor video resumes play"), World->IsPaused());
		return EStep::Done;
	} });

	Steps.Add({ TEXT("trigger defeat cinematic"), [this, State](UWorld* World, int32 Frame)
	{
		AMatsuriFloorGameMode* Mode = World ? World->GetAuthGameMode<AMatsuriFloorGameMode>() : nullptr;
		if (!Mode || !State->bKilled)
		{
			if (Mode && Frame == 0 && !State->bKilled)
			{
				State->bKilled = true;
				TestTrue(TEXT("fatal damage is accepted"), Mode->HurtPlayer(nullptr, 10000.0f));
			}
			return EStep::Continue;
		}
		TestFalse(TEXT("defeat uses no text story card"), FindWidget<UMatsuriStoryWidget>(World) != nullptr);
		UMatsuriCinematicWidget* Movie = FindWidget<UMatsuriCinematicWidget>(World);
		if (!Movie || !Movie->HasMediaOpened()) return EStep::Continue;
		TestTrue(TEXT("defeat video pauses the world"), World->IsPaused());
		Movie->Finish();
		State->bDefeatSkipped = true;
		return EStep::Done;
	} });

	Steps.Add({ TEXT("game over follows defeat video"), [this, State](UWorld* World, int32)
	{
		if (!State->bDefeatSkipped) return EStep::Wait;
		UMatsuriGameOverWidget* GameOver = FindWidget<UMatsuriGameOverWidget>(World);
		if (!GameOver) return EStep::Wait;
		TestEqual(TEXT("fatal run ends once"), Run(World)->GetLastRunEnd(), EMatsuriRunEnd::OutOfHealth);
		TestTrue(TEXT("game over remains paused"), World->IsPaused());
		TestFalse(TEXT("no text story card follows defeat"), FindWidget<UMatsuriStoryWidget>(World) != nullptr);
		return EStep::Done;
	} });

	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.Story"), true);
	SetCVar(TEXT("matsuri.Replay"), false);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps), true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([]
	{
		SetCVar(TEXT("matsuri.Threats"), true);
		SetCVar(TEXT("matsuri.Story"), true);
		SetCVar(TEXT("matsuri.Replay"), true);
	}, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
