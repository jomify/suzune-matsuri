// Suzune's Night Festival - art gallery: every imported Tripo mesh lined up in front of the camera.
// Not a pass/fail gate for gameplay: it checks the art loads and saves screenshots to review
// orientation, scale and textures. Skips quietly when no art has been imported yet.
// Run: Automation RunTests SuzuneMatsuri.Art.Gallery

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriArt.h"
#include "MatsuriFloors.h"
#include "MatsuriFx.h"
#include "MatsuriHUDWidget.h"

using namespace MatsuriTest;

namespace
{
	const TCHAR* const SmallArt[] = {
		TEXT("SM_Weapon_HamaYumi"), TEXT("SM_Weapon_Shuriken"), TEXT("SM_Weapon_Suzu"), TEXT("SM_Weapon_Ooban"),
		TEXT("SM_Pickup_Koban"), TEXT("SM_Pickup_Ginka"), TEXT("SM_Pickup_Magatama"), TEXT("SM_Pickup_Ofuda"),
		TEXT("SM_Yokai_Chochin"), TEXT("SM_Yokai_Kasa"), TEXT("SM_Yokai_Kappa"), TEXT("SM_Prop_Chochin") };
	const TCHAR* const LargeArt[] = {
		TEXT("SM_Prop_StoneLantern"), TEXT("SM_Prop_Yatai"), TEXT("SM_Prop_Torii"), TEXT("SM_Prop_KitsuneStatue"), TEXT("SM_Prop_Bamboo") };

	/** Lines meshes up along Y at Distance in front of Eye, each turned to face the camera (-X). Returns how many exist. */
	int32 LineUp(UWorld* World, const FVector& Eye, float Distance, float Spacing, TArrayView<const TCHAR* const> Names, float Z)
	{
		int32 Placed = 0;
		const float Width = Spacing * (Names.Num() - 1);
		for (int32 i = 0; i < Names.Num(); ++i)
		{
			UStaticMesh* Mesh = Matsuri::Art::Mesh(Names[i]);
			if (!Mesh)
			{
				continue;
			}
			const FVector Spot = Eye + FVector(Distance, -Width * 0.5f + i * Spacing, 0.0f);
			AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(FVector(Spot.X, Spot.Y, Z), FRotator::ZeroRotator);
			Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
			++Placed;
		}
		return Placed;
	}

	int32 LineUpHeroes(UWorld* World, const FVector& Eye)
	{
		int32 Placed = 0;
		for (int32 i = 0; i < Matsuri::Heroes::Count; ++i)
		{
			const EMatsuriHero Hero = static_cast<EMatsuriHero>(i);
			USkeletalMesh* Mesh = Matsuri::Art::HeroMesh(Hero, TEXT("Idle"));
			if (!Mesh)
			{
				continue;
			}
			const FVector Spot = Eye + FVector(300.0f, (i - 1) * 110.0f, 0.0f);
			ASkeletalMeshActor* Actor = World->SpawnActor<ASkeletalMeshActor>(FVector(Spot.X, Spot.Y, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
			USkeletalMeshComponent* Component = Actor->GetSkeletalMeshComponent();
			Component->SetRelativeScale3D(FVector(1.6f));	// rigged heroines come out 1 m tall
			Component->SetSkeletalMesh(Mesh);
			if (UAnimSequence* Idle = Matsuri::Art::HeroAnim(Hero, TEXT("Idle")))
			{
				Component->PlayAnimation(Idle, true);
			}
			++Placed;
		}
		return Placed;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriArtGalleryTest, "SuzuneMatsuri.Art.Gallery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriArtGalleryTest::RunTest(const FString&)
{
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	auto Shot = [this](const TCHAR* Name, TFunction<int32(UWorld*, const FVector&)> Arrange)
	{
		return [this, Name, Arrange](UWorld* World, int32 Frame)
		{
			if (!Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
			{
				return EStep::Wait;
			}
			if (Frame == 0)
			{
				const FVector Eye = Hero(World)->GetActorLocation();
				// Clear the sight line: hide set dressing and obstacles in front (the floor itself stays).
				for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
				{
					const FVector At = It->GetActorLocation() - Eye;
					const bool bFloor = It->GetActorLocation().Z < 0.0f;
					if (!bFloor && At.X > 50.0f && At.X < 3000.0f && FMath::Abs(At.Y) < 2200.0f)
					{
						It->SetActorHiddenInGame(true);
					}
				}
				World->GetFirstPlayerController()->SetControlRotation(FRotator(-6.0f, 0.0f, 0.0f));
				const int32 Placed = Arrange(World, Eye);
				AddInfo(FString::Printf(TEXT("%s: %d asset(s) placed"), Name, Placed));
				if (Placed == 0)
				{
					return EStep::Done;	// nothing imported yet
				}
			}
			return DoneIf(SettleAndShoot(Frame, Name, 60));
		};
	};
	Steps.Add({ TEXT("small art"), Shot(TEXT("40_art_small"), [](UWorld* World, const FVector& Eye) { return LineUp(World, Eye, 220.0f, 40.0f, SmallArt, 110.0f); }) });
	Steps.Add({ TEXT("clear"), [](UWorld* World, int32 Frame)
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetPathName().Contains(TEXT("/Tripo/")))
			{
				It->Destroy();
			}
		}
		return DoneIf(Frame > 2);
	} });
	Steps.Add({ TEXT("large art"), Shot(TEXT("41_art_large"), [](UWorld* World, const FVector& Eye) { return LineUp(World, Eye, 1500.0f, 650.0f, LargeArt, 0.0f); }) });
	Steps.Add({ TEXT("heroines"), Shot(TEXT("42_art_heroines"), [](UWorld* World, const FVector& Eye) { return LineUpHeroes(World, Eye); }) });
	Steps.Add({ TEXT("niagara templates"), [](UWorld* World, int32 Frame)
	{
		// The three engine templates side by side, re-fired so the screenshot catches them in flight.
		const FVector Eye = Hero(World)->GetActorLocation();
		if (Frame % 20 == 0)
		{
			Matsuri::Fx::Template(World, Matsuri::Fx::ETemplate::Explosion, Eye + FVector(400.0f, -160.0f, 0.0f), FVector::UpVector, 0.5f);
			Matsuri::Fx::Template(World, Matsuri::Fx::ETemplate::RadialBurst, Eye + FVector(400.0f, 0.0f, 0.0f), FVector::UpVector, 0.35f);
			Matsuri::Fx::Template(World, Matsuri::Fx::ETemplate::Directional, Eye + FVector(400.0f, 160.0f, -60.0f), FVector::UpVector, 0.3f);
		}
		return DoneIf(SettleAndShoot(Frame, TEXT("43_niagara_templates"), 26));
	} });

	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.Maze"), false);	// open arena: nothing in front of the camera
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); SetCVar(TEXT("matsuri.Maze"), true); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
