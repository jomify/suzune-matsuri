// Suzune's Night Festival - fixed views of every scene for visual review (not in the SuzuneMatsuri suite: it
// only takes pictures). Same seed, no yokai, no story: the title, 1F-3F from their start, and the boss
// courtyard from the entrance and from the west platform. Run rendered, before and after an art change.
// Run: Automation RunTests MatsuriArt.Scenes   (-windowed -ResX=2560 -ResY=1440)

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "MatsuriFloors.h"
#include "MatsuriFlow.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriMainMenuWidget.h"
#include "MatsuriTypes.h"

using namespace MatsuriTest;

namespace
{
	constexpr int32 SceneSettleFrames = 150;	// Lumen and the sky settle before the picture

	FStepRunner::FStep Travel(int32 FloorIndex)
	{
		return [FloorIndex](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				Matsuri::Flow::StartAtFloor(World, EMatsuriHero::Sakura, FloorIndex);
			}
			return DoneIf(Frame > 1);
		};
	}

	FStepRunner::FStep Shoot(int32 FloorIndex, const TCHAR* Name, const FVector& Teleport = FVector::ZeroVector, const FRotator& View = FRotator::ZeroRotator)
	{
		return [FloorIndex, Name, Teleport, View](UWorld* World, int32 Frame)
		{
			if (!IsMap(World, Matsuri::Floors::MapFor(FloorIndex)) || !Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
			{
				return EStep::Wait;
			}
			if (!Teleport.IsZero())
			{
				TeleportHero(World, Teleport);
				World->GetFirstPlayerController()->SetControlRotation(View);
			}
			return DoneIf(SettleAndShoot(Frame, Name, SceneSettleFrames));
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriSceneCaptureTest, "MatsuriArt.Scenes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriSceneCaptureTest::RunTest(const FString&)
{
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("title"), [](UWorld* World, int32 Frame)
	{
		if (!IsMap(World, Matsuri::Maps::Title) || !FindWidget<UMatsuriMainMenuWidget>(World))
		{
			return EStep::Wait;
		}
		return DoneIf(SettleAndShoot(Frame, TEXT("95_scene_title"), SceneSettleFrames));
	} });
	const TCHAR* const FloorShots[] = { TEXT("96_scene_1F"), TEXT("96_scene_2F"), TEXT("96_scene_3F") };
	for (int32 Floor = 0; Floor < Matsuri::Floors::Count; ++Floor)
	{
		Steps.Add({ FString::Printf(TEXT("to %dF"), Floor + 1), Travel(Floor) });
		Steps.Add({ FString::Printf(TEXT("%dF picture"), Floor + 1), Shoot(Floor, FloorShots[Floor]) });
	}
	Steps.Add({ TEXT("to the boss"), Travel(Matsuri::Floors::BossFloor) });
	Steps.Add({ TEXT("boss from the entrance"), Shoot(Matsuri::Floors::BossFloor, TEXT("97_scene_boss_entrance")) });
	Steps.Add({ TEXT("boss from the west platform"), Shoot(Matsuri::Floors::BossFloor, TEXT("97_scene_boss_platform"),
		FVector(-1700.0f, 300.0f, 520.0f), FRotator(-8.0f, 20.0f, 0.0f)) });

	SetCVar(TEXT("matsuri.Story"), false);
	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.God"), true);
	SetIntCVar(TEXT("matsuri.Seed"), 12345);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Maps::Title));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.Story"), true);
		SetCVar(TEXT("matsuri.Threats"), true);
		SetCVar(TEXT("matsuri.God"), false);
		SetIntCVar(TEXT("matsuri.Seed"), 0);
	}, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
