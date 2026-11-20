// Suzune's Night Festival - pictures of the Mountain Temple pack's own maps, to see its pieces before using
// them (not in the SuzuneMatsuri suite). Skipped when the pack is not in the project.
// Run: Automation RunTests MatsuriArt.PackPreview   (-windowed -ResX=2560 -ResY=1440)

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/PackageName.h"
#include "Tests/AutomationEditorCommon.h"

using namespace MatsuriTest;

namespace
{
	FStepRunner::FStep Picture(const TCHAR* Name)
	{
		return [Name](UWorld* World, int32 Frame)
		{
			if (!World || !World->GetFirstPlayerController())
			{
				return EStep::Wait;
			}
			return DoneIf(SettleAndShoot(Frame, Name, 200));
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriPackPreviewTest, "MatsuriArt.PackPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriPackPreviewTest::RunTest(const FString&)
{
	const TCHAR* const Maps[] = { TEXT("/Game/Mountain_Shrine/Maps/Asset_Gym"), TEXT("/Game/Mountain_Shrine/Maps/Final_Assembly") };
	const TCHAR* const Shots[] = { TEXT("98_pack_asset_gym"), TEXT("98_pack_final_assembly") };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Maps); ++i)
	{
		if (!FPackageName::DoesPackageExist(Maps[i]))
		{
			AddInfo(TEXT("the Mountain Temple pack is not in the project: nothing to show"));
			return true;
		}
		TArray<TPair<FString, FStepRunner::FStep>> Steps;
		Steps.Add({ Shots[i], Picture(Shots[i]) });
		ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Maps[i]));
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
