// Suzune's Night Festival - end-to-end test of the brief's core loop, played in PIE.
// Title -> Start Game -> floor loads -> wall blocks movement -> collect coins -> victory UI.
// Threats are switched off so this checks the mandatory loop on its own.
// Run: Automation RunTests SuzuneMatsuri.E2E.CoreLoop   (with a renderer it also saves screenshots)

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriHeroSelectWidget.h"
#include "MatsuriMainMenuWidget.h"
#include "MatsuriTypes.h"
#include "MatsuriVictoryWidget.h"

using namespace MatsuriTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriCoreLoopE2ETest, "SuzuneMatsuri.E2E.CoreLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriCoreLoopE2ETest::RunTest(const FString&)
{
	constexpr int32 WallPushFrames = 120;
	constexpr float WallProbeInset = 120.0f;
	TSharedRef<FCoinCollector> Collector = MakeShared<FCoinCollector>();

	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("title menu"), [this](UWorld* World, int32 Frame)
	{
		UMatsuriMainMenuWidget* Menu = IsMap(World, Matsuri::Maps::Title) ? FindWidget<UMatsuriMainMenuWidget>(World) : nullptr;
		if (!Menu)
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			Run(World)->UseRecordSlot(TestRecordSlot);
		}
		if (!SettleAndShoot(Frame, TEXT("01_title")))
		{
			return EStep::Continue;
		}
		UButton* Start = FindButton(*Menu, TEXT("Start Game"));
		TestNotNull(TEXT("main menu has Start Game"), Start);
		TestNotNull(TEXT("main menu has Quit"), FindButton(*Menu, TEXT("Quit")));
		Start->OnClicked.Broadcast();
		return EStep::Done;
	} });
	Steps.Add({ TEXT("hero select -> Mio"), [](UWorld* World, int32 Frame)
	{
		UMatsuriHeroSelectWidget* Select = FindWidget<UMatsuriHeroSelectWidget>(World);
		if (!Select)
		{
			return EStep::Wait;
		}
		if (!SettleAndShoot(Frame, TEXT("04_hero_select"), 20))
		{
			return EStep::Continue;
		}
		Select->ChooseHero(EMatsuriHero::Mio);	// 3 lives, 20-round magazine
		return EStep::Done;
	} });
	Steps.Add({ TEXT("floor loads"), [this](UWorld* World, int32 Frame)
	{
		if (!IsMap(World, Matsuri::Floors::MapFor(0)) || !Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			TestTrue(TEXT("floor has multiple coins"), CountCollectibles(World) > 1);
		}
		return DoneIf(SettleAndShoot(Frame, TEXT("02_floor_start")));
	} });
	Steps.Add({ TEXT("wall blocks movement"), [this, WallPushFrames, WallProbeInset](UWorld* World, int32 Frame)
	{
		// The start cell sits against the west fence, so walking west from it can only meet the fence.
		const float WallFace = -Matsuri::Floors::SpecFor(0, Run(World)->GetRunState().Seed).HalfExtent;
		ACharacter* Player = Hero(World);
		if (Frame == 0)
		{
			const FVector Start = World->GetAuthGameMode<AMatsuriFloorGameMode>()->GetCheckpoint();
			TeleportHero(World, FVector(WallFace + WallProbeInset + 60.0f, Start.Y, 120.0f));
		}
		Player->AddMovementInput(-FVector::ForwardVector, 1.0f);
		if (Frame < WallPushFrames)
		{
			return EStep::Continue;
		}
		TestTrue(TEXT("walking into the wall is blocked"), Player->GetActorLocation().X - Player->GetSimpleCollisionRadius() >= WallFace - 1.0f);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("collect coins"), [this, Collector](UWorld* World, int32)
	{
		return DoneIf(Collector->Tick(*this, World));
	} });
	Steps.Add({ TEXT("victory"), [this, Collector](UWorld* World, int32 Frame)
	{
		if (!FindWidget<UMatsuriVictoryWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			const FMatsuriScoreState& Score = Run(World)->GetScoreState();
			TestTrue(TEXT("score reached target"), Score.FloorScore >= Score.FloorTarget);
			TestTrue(TEXT("game pauses on victory"), World->IsPaused());
			AddInfo(FString::Printf(TEXT("Collected %d coins, score %d / %d"), Collector->Collected, Score.FloorScore, Score.FloorTarget));
		}
		return DoneIf(SettleAndShoot(Frame, TEXT("03_victory")));
	} });

	SetCVar(TEXT("matsuri.Threats"), false);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Maps::Title));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
