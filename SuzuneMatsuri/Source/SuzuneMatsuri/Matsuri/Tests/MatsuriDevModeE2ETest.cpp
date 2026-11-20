// Suzune's Night Festival - developer mode end to end: the title's 開発者モード panel jumps straight to the
// boss with the chosen heroine and switches; the showcase cycles her skills without teaching her anything;
// the in-game keys (sent through the viewport like real input) start her next bar, restock, and clear.
// Run: Automation RunTests SuzuneMatsuri.E2E.DevMode

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "MatsuriBoss.h"
#include "MatsuriBossBrainComponent.h"
#include "MatsuriDev.h"
#include "MatsuriDevComponent.h"
#include "MatsuriDevWidget.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriMainMenuWidget.h"
#include "MatsuriTypes.h"
#include "MatsuriVictoryWidget.h"

using namespace MatsuriTest;

namespace
{
	AMatsuriBoss* Boss(UWorld* World)
	{
		TActorIterator<AMatsuriBoss> It(World);
		return It ? *It : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriDevModeE2ETest, "SuzuneMatsuri.E2E.DevMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriDevModeE2ETest::RunTest(const FString&)
{
	TArray<TPair<FString, FStepRunner::FStep>> Steps;

	Steps.Add({ TEXT("title -> developer panel"), [this](UWorld* World, int32)
	{
		UMatsuriMainMenuWidget* Menu = IsMap(World, Matsuri::Maps::Title) ? FindWidget<UMatsuriMainMenuWidget>(World) : nullptr;
		if (!Menu)
		{
			return EStep::Wait;
		}
		Run(World)->UseRecordSlot(TestRecordSlot);
		UButton* Dev = FindButton(*Menu, TEXT("Developer Mode"));
		TestNotNull(TEXT("the title has a developer button"), Dev);
		if (Dev)
		{
			Dev->OnClicked.Broadcast();
		}
		return EStep::Done;
	} });

	Steps.Add({ TEXT("pick Suzune, god mode, showcase; jump to the boss"), [this](UWorld* World, int32 Frame)
	{
		UMatsuriDevWidget* Panel = FindWidget<UMatsuriDevWidget>(World);
		if (!Panel)
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			TestTrue(TEXT("opening the panel turns the developer keys on"), Matsuri::Dev::IsEnabled());
			Panel->ChooseHero(EMatsuriHero::Suzune);
		}
		if (!SettleAndShoot(Frame, TEXT("05_dev_panel"), 30))
		{
			return EStep::Continue;
		}
		UButton* God = FindButton(*Panel, TEXT("無敵 God: OFF"));
		UButton* Showcase = FindButton(*Panel, TEXT("技の展示 Showcase: OFF"));
		TestNotNull(TEXT("god switch shows OFF"), God);
		TestNotNull(TEXT("showcase switch shows OFF"), Showcase);
		if (God) { God->OnClicked.Broadcast(); }
		if (Showcase) { Showcase->OnClicked.Broadcast(); }
		TestNotNull(TEXT("the god switch now reads ON"), FindButton(*Panel, TEXT("無敵 God: ON")));
		TestTrue(TEXT("and god mode is on"), Matsuri::Dev::GetBool(TEXT("matsuri.God")));
		Panel->JumpTo(Matsuri::Floors::BossFloor);
		return EStep::Done;
	} });

	Steps.Add({ TEXT("straight into the boss floor"), [this](UWorld* World, int32 Frame)
	{
		if (!IsMap(World, Matsuri::Floors::MapFor(Matsuri::Floors::BossFloor)) || !Hero(World) || !Boss(World) || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		const FMatsuriRunState& State = Run(World)->GetRunState();
		TestEqual(TEXT("the run is on the boss floor"), State.FloorIndex, Matsuri::Floors::BossFloor);
		TestEqual(TEXT("with the chosen heroine"), State.Hero, EMatsuriHero::Suzune);
		TestNotNull(TEXT("the heroine has the developer keys"), Hero(World)->FindComponentByClass<UMatsuriDevComponent>());
		return EStep::Done;
	} });

	Steps.Add({ TEXT("the showcase cycles her skills and records nothing"), [this](UWorld* World, int32 Frame)
	{
		const UMatsuriBossBrainComponent* Brain = Boss(World)->GetBrain();
		if (Brain->GetThisFight().Num() < 2)
		{
			return Frame < 60 * 30 ? EStep::Continue : EStep::Done;
		}
		TestEqual(TEXT("first her rings"), Brain->GetThisFight()[0].Action.Skill, EMatsuriBossSkill::Ring);
		TestEqual(TEXT("then her spiral"), Brain->GetThisFight()[1].Action.Skill, EMatsuriBossSkill::Spiral);
		TestFalse(TEXT("nothing written to her memory"), FPaths::FileExists(TestBossMemoryFolder() / TEXT("steps.jsonl")));
		return EStep::Done;
	} });

	Steps.Add({ TEXT("F7 (real key): her next bar; F6 restocks"), [this](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			TestEqual(TEXT("she is on her first bar"), Boss(World)->GetPhase(), 1);
			TestTrue(TEXT("F7 goes through the viewport"), SendKey(World, EKeys::F7, IE_Pressed));
			SendKey(World, EKeys::F7, IE_Released);
			return EStep::Continue;
		}
		if (Frame < 3)
		{
			return EStep::Continue;
		}
		TestEqual(TEXT("F7 started her second bar"), Boss(World)->GetPhase(), 2);
		Run(World)->ConsumeAmmo(Run(World)->GetRunState().Ammo);
		Matsuri::Dev::Restock(World);
		TestEqual(TEXT("F6 fills the ammo"), Run(World)->GetRunState().Ammo, Run(World)->GetRunState().GetAmmoCap());
		return EStep::Done;
	} });

	Steps.Add({ TEXT("F5 defeats her: final victory"), [this](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			Matsuri::Dev::ClearFloor(World);
			TestTrue(TEXT("she is defeated"), Boss(World)->IsDefeated());
			return EStep::Continue;
		}
		UMatsuriVictoryWidget* Victory = FindWidget<UMatsuriVictoryWidget>(World);
		if (!Victory)
		{
			return EStep::Continue;
		}
		TestEqual(TEXT("the run ends in victory"), Run(World)->GetLastRunEnd(), EMatsuriRunEnd::Victory);
		return EStep::Done;
	} });

	UseTestBossBrain();
	FMatsuriBossStore(TestBossMemoryFolder()).Wipe();
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Maps::Title));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		Matsuri::Dev::SetEnabled(false);
		SetCVar(TEXT("matsuri.God"), false);
		SetCVar(TEXT("matsuri.BossShowcase"), false);
		RestoreBossBrain();
	}, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
