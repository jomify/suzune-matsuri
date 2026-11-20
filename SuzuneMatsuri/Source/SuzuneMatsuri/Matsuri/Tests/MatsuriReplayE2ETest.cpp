// Suzune's Night Festival - PIE death replay integration test.

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "MatsuriFloorGameMode.h"
#include "MatsuriBossGameMode.h"
#include "MatsuriBoss.h"
#include "MatsuriGameOverWidget.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriReplay.h"
#include "MatsuriFloors.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationEditorCommon.h"
#include "Misc/App.h"

using namespace MatsuriTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriReplayE2ETest, "SuzuneMatsuri.E2E.Replay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriReplayE2ETest::RunTest(const FString&)
{
	TSharedRef<int32> ReplayFrame = MakeShared<int32>(-1);
	TSharedRef<float> ScoreBefore = MakeShared<float>(0.0f);
	TSharedRef<bool> Triggered = MakeShared<bool>(false);
	TArray<TPair<FString, FStepRunner::FStep>> Steps;

	Steps.Add({ TEXT("floor and replay director load"), [this](UWorld* World, int32 Frame)
	{
		AMatsuriFloorGameMode* Mode = World ? World->GetAuthGameMode<AMatsuriFloorGameMode>() : nullptr;
		if (!Hero(World) || !Mode || !Mode->GetReplayDirector() || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			UGameplayStatics::DeleteGameInSlot(TestRecordSlot, 0);
			Run(World)->UseRecordSlot(TestRecordSlot);
		}
		return DoneIf(Frame > 5);
	} });

	Steps.Add({ TEXT("record more than six seconds"), [this, ReplayFrame](UWorld* World, int32 Frame)
	{
		AMatsuriFloorGameMode* Mode = World->GetAuthGameMode<AMatsuriFloorGameMode>();
		if (!Mode || !Mode->GetReplayDirector())
		{
			return EStep::Wait;
		}
		const FMatsuriReplayBuffer& History = Mode->GetReplayDirector()->GetHistory();
		if (History.Num() < FMatsuriReplayBuffer::DefaultMaxFrames || History.Duration() < 5.5f)
		{
			return EStep::Continue;
		}
		*ReplayFrame = History.Num() > 1 ? 1 : -1;
		TestEqual(TEXT("history is bounded after more than six seconds"), History.Num(), FMatsuriReplayBuffer::DefaultMaxFrames);
		TestTrue(TEXT("history spans the replay window"), History.Duration() > 5.5f);
		return EStep::Done;
	} });

	Steps.Add({ TEXT("fatal hit starts replay"), [this, ScoreBefore, Triggered](UWorld* World, int32 Frame)
	{
		AMatsuriFloorGameMode* Mode = World->GetAuthGameMode<AMatsuriFloorGameMode>();
		if (!Mode || !Mode->GetReplayDirector() || Frame == 0)
		{
			return Frame == 0 ? EStep::Continue : EStep::Wait;
		}
		if (!*Triggered)
		{
			*ScoreBefore = static_cast<float>(Run(World)->GetRunState().Score.RunScore);
			const bool bAccepted = Mode->HurtPlayer(nullptr, 10000.0f, FText::FromString(TEXT("test fatal hit")), -1);
			TestTrue(TEXT("fatal damage was accepted"), bAccepted);
			TestEqual(TEXT("fatal damage does not alter score"), static_cast<float>(Run(World)->GetRunState().Score.RunScore), *ScoreBefore);
			TestEqual(TEXT("health reaches zero"), Run(World)->GetRunState().Health, 0.0f);
			TestTrue(TEXT("replay is active"), Mode->GetReplayDirector()->IsReplaying());
			TestTrue(TEXT("world is paused during replay"), World->IsPaused());
			*Triggered = true;
		}
		return EStep::Done;
	} });

	Steps.Add({ TEXT("replay can advance and skip"), [this, ReplayFrame, ScoreBefore](UWorld* World, int32 Frame)
	{
		AMatsuriFloorGameMode* Mode = World->GetAuthGameMode<AMatsuriFloorGameMode>();
		AMatsuriReplayDirector* Replay = Mode ? Mode->GetReplayDirector() : nullptr;
		if (!Replay || !Replay->IsReplaying())
		{
			return EStep::Wait;
		}
		if (Frame > 4)
		{
			TestTrue(TEXT("replay selects a historical frame"), Replay->GetPlaybackFrameIndex() >= *ReplayFrame);
			Replay->FinishReplay();
			TestFalse(TEXT("skip finishes replay"), Replay->IsReplaying());
			TestEqual(TEXT("score remains unchanged after replay"), static_cast<float>(Run(World)->GetRunState().Score.RunScore), *ScoreBefore);
			return EStep::Done;
		}
		return EStep::Continue;
	} });

	Steps.Add({ TEXT("finish is idempotent and game over appears"), [this, ScoreBefore](UWorld* World, int32)
	{
		AMatsuriFloorGameMode* Mode = World->GetAuthGameMode<AMatsuriFloorGameMode>();
		if (!FindWidget<UMatsuriGameOverWidget>(World))
		{
			return EStep::Wait;
		}
		if (Mode && Mode->GetReplayDirector())
		{
			Mode->GetReplayDirector()->FinishReplay();
		}
		TestEqual(TEXT("run ends once after replay"), Run(World)->GetLastRunEnd(), EMatsuriRunEnd::OutOfHealth);
		TestEqual(TEXT("score is still unchanged"), static_cast<float>(Run(World)->GetRunState().Score.RunScore), *ScoreBefore);
		TestTrue(TEXT("game over is paused"), World->IsPaused());
		return EStep::Done;
	} });

	SetCVar(TEXT("matsuri.Threats"), false);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Mio));
	UseTestBossBrain();
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps), false, true));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); RestoreBossBrain(); }, 0.0f));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossReplayE2ETest, "SuzuneMatsuri.E2E.BossReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriBossReplayE2ETest::RunTest(const FString&)
{
	TSharedRef<int32> ScoreBefore = MakeShared<int32>(0);
	TSharedRef<bool> bSawHistoricalBullets = MakeShared<bool>(false);
	TSharedRef<bool> bScreenshotRequested = MakeShared<bool>(false);
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("boss replay loads"), [this](UWorld* World, int32 Frame)
	{
		AMatsuriBossGameMode* Mode = World ? World->GetAuthGameMode<AMatsuriBossGameMode>() : nullptr;
		if (!Mode || !Hero(World) || !Mode->GetField() || !Mode->GetReplayDirector() || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			UGameplayStatics::DeleteGameInSlot(TestRecordSlot, 0);
			Run(World)->UseRecordSlot(TestRecordSlot);
		}
		return DoneIf(Frame > 5);
	} });
	Steps.Add({ TEXT("boss has fired real bullets"), [this](UWorld* World, int32)
	{
		AMatsuriBossGameMode* Mode = World->GetAuthGameMode<AMatsuriBossGameMode>();
		if (!Mode || !Mode->GetField())
		{
			return EStep::Wait;
		}
		if (Mode->GetField()->Num() <= 0)
		{
			return EStep::Continue;
		}
		// Wait for actual recorded projectile frames rather than killing the player
		// on the first emission; slow rendered frames can otherwise skip the entire
		// tiny replay and its screenshot window.
		const FMatsuriReplayBuffer& History = Mode->GetReplayDirector()->GetHistory();
		int32 BulletFrames = 0;
		for (int32 Index = 0; Index < History.Num(); ++Index)
		{
			const FMatsuriReplayFrame* Recorded = History.At(Index);
			BulletFrames += Recorded && Recorded->Bullets.Num() > 0 ? 1 : 0;
		}
		if (History.Duration() < 2.0f || BulletFrames < 12)
		{
			Mode->GrantInvulnerability(0.5f);
			return EStep::Continue;
		}
		TestTrue(TEXT("Boss field has active bullets before death"), Mode->GetField()->Num() > 0);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("real bullet death starts replay"), [this, ScoreBefore](UWorld* World, int32 Frame)
	{
		AMatsuriBossGameMode* Mode = World->GetAuthGameMode<AMatsuriBossGameMode>();
		if (!Mode || !Mode->GetReplayDirector() || Frame == 0)
		{
			return Frame == 0 ? EStep::Continue : EStep::Wait;
		}
		if (Mode->IsPlayerInvulnerable()) { return EStep::Continue; }
		*ScoreBefore = Run(World)->GetRunState().Score.RunScore;
		TestTrue(TEXT("fatal damage accepted after real bullet history"), Mode->HurtPlayer(Mode->GetBoss(), 10000.0f, FText::FromString(TEXT("boss danmaku")), 0));
		TestTrue(TEXT("Boss replay starts"), Mode->GetReplayDirector()->IsReplaying());
		return EStep::Done;
	} });
	Steps.Add({ TEXT("Boss replay displays historical bullets and completes"), [this, ScoreBefore, bSawHistoricalBullets, bScreenshotRequested](UWorld* World, int32 Frame)
	{
		AMatsuriBossGameMode* Mode = World->GetAuthGameMode<AMatsuriBossGameMode>();
		AMatsuriReplayDirector* Replay = Mode ? Mode->GetReplayDirector() : nullptr;
		if (!Replay)
		{
			return EStep::Wait;
		}
		if (Replay->IsReplaying())
		{
			if (!*bScreenshotRequested && FApp::CanEverRender())
			{
				const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/MatsuriShots");
				IFileManager::Get().MakeDirectory(*Directory, true);
				FScreenshotRequest::RequestScreenshot(Directory / TEXT("replay_boss.png"), true, false);
				*bScreenshotRequested = true;
			}
			if (Replay->GetPlaybackBulletCount() > 0)
			{
				*bSawHistoricalBullets = true;
			}
			return EStep::Continue;
		}
		if (!FindWidget<UMatsuriGameOverWidget>(World))
		{
			return EStep::Wait;
		}
		TestEqual(TEXT("Boss replay ends in out of health"), Run(World)->GetLastRunEnd(), EMatsuriRunEnd::OutOfHealth);
		TestEqual(TEXT("Boss replay does not change score"), Run(World)->GetRunState().Score.RunScore, *ScoreBefore);
		TestTrue(TEXT("replay applied historical Boss bullets"), *bSawHistoricalBullets);
		TestTrue(TEXT("rendered replay screenshot requested"), !FApp::CanEverRender() || *bScreenshotRequested);
		TestTrue(TEXT("Boss replay completed automatically"), Frame > 0);
		return EStep::Done;
	} });

	UseTestBossBrain();
	SetCVar(TEXT("matsuri.Threats"), true);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Mio));
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(Matsuri::Floors::BossFloor)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps), false, true));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); RestoreBossBrain(); }, 0.0f));
	return true;
}

#endif

