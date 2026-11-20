// Suzune's Night Festival - real-input regression test in PIE.
// After every UI -> level change (title Start, omamori pick, Try Again) the player must be able to
// walk with W and shoot with the left mouse button, sent through the game viewport like real input.
// Regression for: menus set UI-only input on the (level-surviving) viewport and floors never reset it.
// Run: Automation RunTests SuzuneMatsuri.E2E.RealInput

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriFloors.h"
#include "MatsuriGameOverWidget.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriHeroSelectWidget.h"
#include "MatsuriMainMenuWidget.h"
#include "MatsuriOmamoriWidget.h"
#include "MatsuriTypes.h"
#include "MatsuriVictoryWidget.h"

using namespace MatsuriTest;

namespace
{
	constexpr int32 WalkFrames = 45;
	constexpr float MinWalkDistance = 150.0f;

	struct FInputState
	{
		FVector WalkStart = FVector::ZeroVector;
		int32 AmmoBefore = 0;
		FCoinCollector Collector;
		TWeakObjectPtr<UWorld> LeftWorld;	// the world we navigated away from
	};

	/** A floor is "ready" when it is a new world with the hero and HUD up. */
	bool FloorReady(UWorld* World, const FInputState& State, int32 FloorIndex)
	{
		return World != State.LeftWorld.Get() && IsMap(World, Matsuri::Floors::MapFor(FloorIndex))
			&& Hero(World) && FindWidget<UMatsuriHUDWidget>(World);
	}

	/** Steps proving W walks and LMB fires on the current floor, via the viewport. */
	void AddControlChecks(FAutomationTestBase& Test, TArray<TPair<FString, FStepRunner::FStep>>& Steps,
		TSharedRef<FInputState> State, int32 FloorIndex, const FString& After)
	{
		Steps.Add({ TEXT("W walks after ") + After, [&Test, State, FloorIndex, After](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				if (!FloorReady(World, *State, FloorIndex))
				{
					return EStep::Wait;
				}
				Test.TestTrue(TEXT("viewport accepts game input after ") + After, ViewportAcceptsGameInput(World));
				State->WalkStart = Hero(World)->GetActorLocation();
				SendKey(World, EKeys::W, IE_Pressed);
			}
			if (Frame < WalkFrames)
			{
				return EStep::Continue;
			}
			SendKey(World, EKeys::W, IE_Released);
			const float Walked = FVector::Dist2D(Hero(World)->GetActorLocation(), State->WalkStart);
			Test.TestTrue(FString::Printf(TEXT("holding W moved the hero (%.0f cm) after %s"), Walked, *After), Walked > MinWalkDistance);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("LMB fires after ") + After, [&Test, State, After](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				State->AmmoBefore = Run(World)->GetRunState().Ammo;
				SendKey(World, EKeys::LeftMouseButton, IE_Pressed);
			}
			if (Frame < 3)
			{
				return EStep::Continue;
			}
			SendKey(World, EKeys::LeftMouseButton, IE_Released);
			Test.TestTrue(TEXT("clicking fired a shot after ") + After, Run(World)->GetRunState().Ammo < State->AmmoBefore);
			return EStep::Done;
		} });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriInputE2ETest, "SuzuneMatsuri.E2E.RealInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriInputE2ETest::RunTest(const FString&)
{
	TSharedRef<FInputState> State = MakeShared<FInputState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;

	Steps.Add({ TEXT("title -> Start Game"), [State](UWorld* World, int32)
	{
		UMatsuriMainMenuWidget* Menu = IsMap(World, Matsuri::Maps::Title) ? FindWidget<UMatsuriMainMenuWidget>(World) : nullptr;
		if (!Menu)
		{
			return EStep::Wait;
		}
		Run(World)->UseRecordSlot(TestRecordSlot);
		State->LeftWorld = World;
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
	AddControlChecks(*this, Steps, State, 0, TEXT("the title screen"));

	Steps.Add({ TEXT("clear 1F"), [this, State](UWorld* World, int32)
	{
		return DoneIf(State->Collector.Tick(*this, World));
	} });
	Steps.Add({ TEXT("victory -> omamori -> 2F"), [State](UWorld* World, int32)
	{
		if (UMatsuriVictoryWidget* Victory = FindWidget<UMatsuriVictoryWidget>(World))
		{
			FindButton(*Victory, TEXT("Choose an Omamori"))->OnClicked.Broadcast();
			return EStep::Continue;
		}
		UMatsuriOmamoriWidget* Picker = FindWidget<UMatsuriOmamoriWidget>(World);
		if (!Picker)
		{
			return EStep::Wait;
		}
		State->LeftWorld = World;
		Picker->ChooseCard(0);
		return EStep::Done;
	} });
	AddControlChecks(*this, Steps, State, 1, TEXT("the omamori pick"));

	Steps.Add({ TEXT("time up -> Try Again"), [State](UWorld* World, int32)
	{
		if (AMatsuriFloorGameMode* Mode = World->GetAuthGameMode<AMatsuriFloorGameMode>(); Mode && Mode->IsFloorActive())
		{
			if (Mode->GetRemainingSeconds() > 1.0f)
			{
				Mode->SetRemainingSeconds(0.5f);	// once; then let it actually run out
			}
			return EStep::Continue;
		}
		UMatsuriGameOverWidget* GameOver = FindWidget<UMatsuriGameOverWidget>(World);
		if (!GameOver)
		{
			return EStep::Wait;
		}
		State->LeftWorld = World;
		FindButton(*GameOver, TEXT("Try Again"))->OnClicked.Broadcast();
		return EStep::Done;
	} });
	AddControlChecks(*this, Steps, State, 0, TEXT("Try Again"));

	SetCVar(TEXT("matsuri.Threats"), false);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Maps::Title));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
