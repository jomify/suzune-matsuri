// Suzune's Night Festival - end-to-end roguelite run in PIE: all three floors with live yokai, then the boss.
// Title -> Start -> clear 1F -> pick an omamori -> 2F loads (score, lives and charm carried) ->
// ... -> clear 3F -> omamori -> 本殿 (boss) -> defeat Tamamo -> final victory, record saved. Her fight
// itself is covered by SuzuneMatsuri.E2E.Boss; here she is simply worn down. The hero runs in god mode so random hits
// cannot fail the run; hit rules are covered by SuzuneMatsuri.E2E.LivesAndTimeUp.
// Run: Automation RunTests SuzuneMatsuri.E2E.FloorProgression

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriHeroSelectWidget.h"
#include "MatsuriMainMenuWidget.h"
#include "MatsuriOmamoriWidget.h"
#include "MatsuriTypes.h"
#include "MatsuriVictoryWidget.h"
#include "MatsuriYokai.h"
#include "MatsuriBoss.h"

using namespace MatsuriTest;

namespace
{
	struct FRunState
	{
		FCoinCollector Collector;
		EMatsuriOmamori Picked = EMatsuriOmamori::Count;
		int32 ScoreAtClear = 0;
		float HealthAtClear = 0.0f;
		int32 FloorsChecked = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriProgressionE2ETest, "SuzuneMatsuri.E2E.FloorProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriProgressionE2ETest::RunTest(const FString&)
{
	TSharedRef<FRunState> State = MakeShared<FRunState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;

	Steps.Add({ TEXT("title -> start"), [](UWorld* World, int32)
	{
		UMatsuriMainMenuWidget* Menu = IsMap(World, Matsuri::Maps::Title) ? FindWidget<UMatsuriMainMenuWidget>(World) : nullptr;
		if (!Menu)
		{
			return EStep::Wait;
		}
		UGameplayStatics::DeleteGameInSlot(TestRecordSlot, 0);
		Run(World)->UseRecordSlot(TestRecordSlot);
		FindButton(*Menu, TEXT("Start Game"))->OnClicked.Broadcast();
		return EStep::Done;
	} });

	Steps.Add({ TEXT("hero select -> Mio"), [](UWorld* World, int32)
	{
		UMatsuriHeroSelectWidget* Select = FindWidget<UMatsuriHeroSelectWidget>(World);
		if (!Select)
		{
			return EStep::Wait;
		}
		Select->ChooseHero(EMatsuriHero::Mio);	// 3 lives, 20-round magazine
		return EStep::Done;
	} });
	for (int32 Floor = 0; Floor < Matsuri::Floors::Count; ++Floor)
	{
		Steps.Add({ FString::Printf(TEXT("%dF loads"), Floor + 1), [this, State, Floor](UWorld* World, int32 Frame)
		{
			if (!IsMap(World, Matsuri::Floors::MapFor(Floor)) || !Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
			{
				return EStep::Wait;
			}
			if (Frame == 0)
			{
				const FMatsuriRunState& RunState = Run(World)->GetRunState();
				TestEqual(TEXT("run is on this floor"), RunState.FloorIndex, Floor);
				TestTrue(TEXT("yokai roam the floor"), TActorIterator<AMatsuriYokai>(World) ? true : false);
				if (Floor > 0)
				{
					TestEqual(TEXT("run score carried to the next floor"), RunState.Score.RunScore, State->ScoreAtClear);
					TestEqual(TEXT("floor score starts at zero"), RunState.Score.FloorScore, 0);
					TestTrue(TEXT("chosen charm is active"), RunState.Charms.Contains(State->Picked));
					TestEqual(TEXT("health restored at the next floor"), RunState.Health, RunState.GetMaxHealth());
				}
				++State->FloorsChecked;
				State->Collector = FCoinCollector();
			}
			return DoneIf(SettleAndShoot(Frame, *FString::Printf(TEXT("2%d_floor%d"), Floor + 1, Floor + 1)));
		} });
		Steps.Add({ FString::Printf(TEXT("clear %dF"), Floor + 1), [this, State](UWorld* World, int32)
		{
			return DoneIf(State->Collector.Tick(*this, World));
		} });
		Steps.Add({ FString::Printf(TEXT("%dF victory"), Floor + 1), [this, State](UWorld* World, int32 Frame)
		{
			UMatsuriVictoryWidget* Victory = FindWidget<UMatsuriVictoryWidget>(World);
			if (!Victory)
			{
				return EStep::Wait;
			}
			if (!SettleAndShoot(Frame, TEXT("25_floor_clear")))
			{
				return EStep::Continue;
			}
			State->ScoreAtClear = Run(World)->GetRunState().Score.RunScore;
			State->HealthAtClear = Run(World)->GetRunState().Health;
			UButton* Next = FindButton(*Victory, TEXT("Choose an Omamori"));
			TestNotNull(TEXT("floor clear leads to the omamori choice (3F leads to the boss)"), Next);
			Next->OnClicked.Broadcast();
			return EStep::Done;
		} });
		Steps.Add({ FString::Printf(TEXT("omamori after %dF"), Floor + 1), [this, State, Floor](UWorld* World, int32 Frame)
		{
			UMatsuriOmamoriWidget* Picker = FindWidget<UMatsuriOmamoriWidget>(World);
			if (!Picker)
			{
				return EStep::Wait;
			}
			if (Floor == 0 && !SettleAndShoot(Frame, TEXT("26_omamori")))
			{
				return EStep::Continue;
			}
			TestEqual(TEXT("three charms offered"), Picker->GetOffer().Num(), Matsuri::Omamori::OfferSize);
			State->Picked = Picker->GetOffer()[0];
			Picker->ChooseCard(0);
			return EStep::Done;
		} });
	}

	Steps.Add({ TEXT("boss floor loads"), [this, State](UWorld* World, int32 Frame)
	{
		if (!IsMap(World, Matsuri::Floors::MapFor(Matsuri::Floors::BossFloor)) || !Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			const FMatsuriRunState& RunState = Run(World)->GetRunState();
			TestEqual(TEXT("run is on the boss floor"), RunState.FloorIndex, Matsuri::Floors::BossFloor);
			TestEqual(TEXT("run score carried to the boss"), RunState.Score.RunScore, State->ScoreAtClear);
			TestTrue(TEXT("Tamamo is there"), TActorIterator<AMatsuriBoss>(World) ? true : false);
			++State->FloorsChecked;
		}
		return DoneIf(SettleAndShoot(Frame, TEXT("27_boss_floor")));
	} });
	Steps.Add({ TEXT("wear Tamamo down"), [](UWorld* World, int32)
	{
		TActorIterator<AMatsuriBoss> Boss(World);
		if (!Boss)
		{
			return EStep::Wait;
		}
		if (Boss->IsDefeated())
		{
			return EStep::Done;
		}
		// Ignored during her greeting and between bars, as in play.
		UGameplayStatics::ApplyDamage(*Boss, 10.0f, World->GetFirstPlayerController(), Hero(World), nullptr);
		return EStep::Continue;
	} });
	Steps.Add({ TEXT("final victory"), [this, State](UWorld* World, int32 Frame)
	{
		UMatsuriVictoryWidget* Victory = FindWidget<UMatsuriVictoryWidget>(World);
		if (!Victory)
		{
			return EStep::Wait;
		}
		if (!SettleAndShoot(Frame, TEXT("29_final_victory")))
		{
			return EStep::Continue;
		}
		const FMatsuriRecord& Record = Run(World)->GetRecord();
		TestEqual(TEXT("every floor was entered and checked"), State->FloorsChecked, Matsuri::Floors::Total);
		TestEqual(TEXT("run ended in victory"), Run(World)->GetLastRunEnd(), EMatsuriRunEnd::Victory);
		TestEqual(TEXT("victory recorded"), Record.Victories, 1);
		TestEqual(TEXT("deepest floor saved"), Record.DeepestFloor, Matsuri::Floors::Total);
		TestNotNull(TEXT("final screen offers Play Again"), FindButton(*Victory, TEXT("Play Again")));
		return EStep::Done;
	} });

	UseTestBossBrain();
	SetCVar(TEXT("matsuri.God"), true);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Maps::Title));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.God"), false); RestoreBossBrain(); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
