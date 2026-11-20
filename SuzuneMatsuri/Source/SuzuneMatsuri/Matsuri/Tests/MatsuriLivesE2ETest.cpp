// Suzune's Night Festival - end-to-end lives / game-over / timer / high-score test in PIE.
// Repeated yokai touches reduce HP with a short grace window, then GAME OVER.
// Try Again starts a fresh run; the countdown hitting zero is the second way to lose.
// Both runs land in the (test-only) save slot, which is checked on disk.
// Run: Automation RunTests SuzuneMatsuri.E2E.LivesAndTimeUp

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriFloors.h"
#include "MatsuriGameOverWidget.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriYokaiKinds.h"

using namespace MatsuriTest;

namespace
{
	constexpr float IFrameWait = 0.6f;	// a little over the post-hit grace window
	constexpr int32 OrdinaryDamage = 30;
	constexpr int32 HitsToDeath = 7;

	struct FHealthState
	{
		float HealthBefore = 0.0f;
		double HitTime = 0.0;
		TWeakObjectPtr<UWorld> GameOverWorld;	// the reloaded floor must be a new world, not this one
	};

	AMatsuriFloorGameMode* FloorMode(UWorld* World)
	{
		return World ? World->GetAuthGameMode<AMatsuriFloorGameMode>() : nullptr;
	}

	/** Drops a lantern ghost right onto the hero: the spawn overlap is a contact hit. */
	void TouchYokai(UWorld* World)
	{
		World->SpawnActor<AMatsuriChochin>(Hero(World)->GetActorLocation(), FRotator::ZeroRotator);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriLivesE2ETest, "SuzuneMatsuri.E2E.LivesAndTimeUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriLivesE2ETest::RunTest(const FString&)
{
	TSharedRef<FHealthState> State = MakeShared<FHealthState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;

	Steps.Add({ TEXT("floor loads"), [this](UWorld* World, int32 Frame)
	{
		if (!Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			UGameplayStatics::DeleteGameInSlot(TestRecordSlot, 0);
			Run(World)->UseRecordSlot(TestRecordSlot);
			TestEqual(TEXT("fresh record"), Run(World)->GetRecord().RunsPlayed, 0);
		}
		return DoneIf(Frame > 5);
	} });

	for (int32 Hit = 1; Hit <= HitsToDeath; ++Hit)
	{
		Steps.Add({ FString::Printf(TEXT("yokai touch %d"), Hit), [this, State, Hit](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				State->HealthBefore = Run(World)->GetRunState().Health;
				TeleportHero(World, FloorMode(World)->GetCheckpoint() + FVector(150.0f, 0.0f, 0.0f));
				return EStep::Continue;
			}
			if (Frame == 2)
			{
				TouchYokai(World);
				State->HitTime = FPlatformTime::Seconds();
			}
			if (Frame <= 2)
			{
				return EStep::Continue;
			}
			if (Hit < HitsToDeath)
			{
				if (Frame == 4)
				{
					TestEqual(TEXT("touching a yokai costs 30 HP"), Run(World)->GetRunState().Health, State->HealthBefore - OrdinaryDamage);
					TestTrue(TEXT("invulnerable right after the hit"), FloorMode(World)->IsPlayerInvulnerable());
					TouchYokai(World);	// second touch during i-frames
				}
				if (Frame == 6)
				{
					TestEqual(TEXT("grace blocks a second hit"), Run(World)->GetRunState().Health, State->HealthBefore - OrdinaryDamage);
					for (TActorIterator<AMatsuriYokai> It(World); It; ++It)
					{
						It->Destroy();	// otherwise they chase the respawned hero and add hits
					}
				}
				return DoneIf(FPlatformTime::Seconds() - State->HitTime > IFrameWait);
			}
			return DoneIf(Frame > 4);
		} });
	}

	Steps.Add({ TEXT("game over: out of health"), [this, State](UWorld* World, int32 Frame)
	{
		UMatsuriGameOverWidget* GameOver = FindWidget<UMatsuriGameOverWidget>(World);
		if (!GameOver)
		{
			return EStep::Wait;
		}
		if (!SettleAndShoot(Frame, TEXT("11_game_over_lives")))
		{
			return EStep::Continue;
		}
		State->GameOverWorld = World;
		TestEqual(TEXT("ended out of health"), Run(World)->GetLastRunEnd(), EMatsuriRunEnd::OutOfHealth);
		TestTrue(TEXT("paused on game over"), World->IsPaused());
		TestEqual(TEXT("run recorded"), Run(World)->GetRecord().RunsPlayed, 1);
		UButton* Retry = FindButton(*GameOver, TEXT("Try Again"));
		TestNotNull(TEXT("game over offers Try Again"), Retry);
		Retry->OnClicked.Broadcast();
		return EStep::Done;
	} });
	Steps.Add({ TEXT("fresh run after Try Again"), [this, State](UWorld* World, int32 Frame)
	{
		if (World == State->GameOverWorld.Get() || !FindWidget<UMatsuriHUDWidget>(World) || !Run(World)->IsRunActive() || !FloorMode(World))
		{
			return EStep::Wait;
		}
		TestEqual(TEXT("health restored"), Run(World)->GetRunState().Health, Run(World)->GetRunState().GetMaxHealth());
		TestEqual(TEXT("score reset"), Run(World)->GetRunState().Score.RunScore, 0);
		FloorMode(World)->SetRemainingSeconds(1.0f);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("game over: time up"), [this](UWorld* World, int32 Frame)
	{
		if (!FindWidget<UMatsuriGameOverWidget>(World))
		{
			return EStep::Wait;
		}
		if (!SettleAndShoot(Frame, TEXT("12_game_over_time")))
		{
			return EStep::Continue;
		}
		TestEqual(TEXT("ended by the countdown"), Run(World)->GetLastRunEnd(), EMatsuriRunEnd::TimeUp);
		TestEqual(TEXT("both runs recorded"), Run(World)->GetRecord().RunsPlayed, 2);
		TestTrue(TEXT("save slot exists on disk"), UGameplayStatics::DoesSaveGameExist(TestRecordSlot, 0));

		FString Json;
		const FString JsonPath = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (FString(TestRecordSlot) + TEXT(".json"));
		TestTrue(TEXT("readable JSON copy written"), FFileHelper::LoadFileToString(Json, *JsonPath) && Json.Contains(TEXT("\"runsPlayed\": 2")));
		return EStep::Done;
	} });

	SetCVar(TEXT("matsuri.Threats"), false);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Mio));	// Mio: base health, no passive
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
