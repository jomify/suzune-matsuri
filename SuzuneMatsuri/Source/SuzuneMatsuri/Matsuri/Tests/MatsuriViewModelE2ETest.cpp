// Suzune's Night Festival - the weapon in view follows the view and shots leave from its tip.
// Playtest: "the weapon does not follow properly". For each heroine and several view angles: the weapon
// keeps its rest pose in camera space, its tip sits in the lower right of the screen, a shot starts at
// that tip, and firing kicks the weapon back. Skips quietly without the weapon art.
// Run: Automation RunTests SuzuneMatsuri.E2E.ViewModel

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Camera/PlayerCameraManager.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriProjectile.h"
#include "MatsuriViewModel.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "MatsuriArtTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SuzuneMatsuriCharacter.h"

using namespace MatsuriTest;

namespace
{
	constexpr float PoseTolerance = 1.5f;		// cm, once sway has settled
	constexpr float MuzzleTolerance = 2.0f;		// cm between the tip and a fresh shot
	constexpr double SettleSeconds = 0.6;		// sway and bob die away

	const FRotator Views[] = { FRotator(0.0f, 0.0f, 0.0f), FRotator(-45.0f, 35.0f, 0.0f), FRotator(40.0f, -70.0f, 0.0f), FRotator(10.0f, 160.0f, 0.0f) };
	const TCHAR* const HeroShots[] = { TEXT("50_view_sakura"), TEXT("51_view_suzune"), TEXT("52_view_mio") };

	struct FViewState
	{
		int32 ViewIndex = 0;
		double Since = 0.0;
		float WalkSpeed = 0.0f;
		FVector RestNock = FVector::ZeroVector;
		bool bReleased = false;
		bool bDrawShot = false;
	};

	UMatsuriViewModelComponent* ViewModel(UWorld* World)
	{
		return Hero(World) ? Hero(World)->FindComponentByClass<UMatsuriViewModelComponent>() : nullptr;
	}

	FVector OffsetFromCameraOf(UWorld* World, const FVector& At)
	{
		const APlayerCameraManager* Camera = World->GetFirstPlayerController()->PlayerCameraManager;
		return Camera->GetCameraRotation().UnrotateVector(At - Camera->GetCameraLocation());
	}

	/** The view model's offset from the camera, in camera space (forward, right, up). */
	FVector OffsetFromCamera(UWorld* World, const UMatsuriViewModelComponent& View)
	{
		const APlayerCameraManager* Camera = World->GetFirstPlayerController()->PlayerCameraManager;
		return Camera->GetCameraRotation().UnrotateVector(View.GetComponentLocation() - Camera->GetCameraLocation());
	}

	void AddHeroRun(FAutomationTestBase& Test, int32 HeroIndex)
	{
		TSharedRef<FViewState> State = MakeShared<FViewState>();
		TArray<TPair<FString, FStepRunner::FStep>> Steps;
		Steps.Add({ TEXT("floor ready"), [](UWorld* World, int32 Frame)
		{
			if (!Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
			{
				return EStep::Wait;
			}
			if (Frame == 0)
			{
				Run(World)->UseRecordSlot(TestRecordSlot);
				Run(World)->SetFloorTargetForTesting(100000);
				Run(World)->AddAmmo(50);
			}
			return DoneIf(Frame > 5);
		} });
		Steps.Add({ TEXT("weapon follows every view"), [&Test, State](UWorld* World, int32 Frame)
		{
			UMatsuriViewModelComponent* View = ViewModel(World);
			if (!View)
			{
				Test.AddInfo(TEXT("no weapon art imported: nothing to check"));
				return EStep::Done;
			}
			if (Frame == 0 || World->GetTimeSeconds() - State->Since < SettleSeconds)
			{
				if (Frame == 0)
				{
					State->ViewIndex = 0;
					State->Since = World->GetTimeSeconds();
				}
				World->GetFirstPlayerController()->SetControlRotation(Views[State->ViewIndex]);
				return EStep::Continue;
			}
			// Playtest: no hands, and never the template's grey mannequin arms.
			if (const ASuzuneMatsuriCharacter* Template = Cast<ASuzuneMatsuriCharacter>(Hero(World)))
			{
				Test.TestFalse(TEXT("the template's grey mannequin arms are hidden"), Template->GetFirstPersonMesh()->IsVisible());
			}
			const FVector Offset = OffsetFromCamera(World, *View);
			Test.TestTrue(FString::Printf(TEXT("view %s: weapon holds its pose in view (off by %.2f cm)"), *Views[State->ViewIndex].ToCompactString(),
				FVector::Dist(Offset, View->GetRestOffset())), FVector::Dist(Offset, View->GetRestOffset()) <= PoseTolerance);

			FVector2D Screen;
			int32 Width = 0, Height = 0;
			World->GetFirstPlayerController()->GetViewportSize(Width, Height);
			if (Width > 0 && World->GetFirstPlayerController()->ProjectWorldLocationToScreen(View->GetMuzzleLocation(), Screen))
			{
				if (View->IsBow())
				{
					// Playtest: drawing must not block the view. Like Hanzo's, the bow and the nocked arrow stay at the
					// lower left; the arrow's head sits below and left of the crosshair, outside the middle of the view.
					const bool bLowerLeft = Screen.X < Width * 0.5f && Screen.Y > Height * 0.5f;
					const bool bOutsideMiddle = FMath::Abs(Screen.X - Width * 0.5f) > Width * 0.08f || FMath::Abs(Screen.Y - Height * 0.5f) > Height * 0.12f;
					Test.TestTrue(FString::Printf(TEXT("the bow keeps the middle of the view clear (arrow head at %.0f, %.0f of %d x %d)"), Screen.X, Screen.Y, Width, Height),
						bLowerLeft && bOutsideMiddle);
				}
				else
				{
					Test.TestTrue(FString::Printf(TEXT("weapon tip in the lower right of the screen (%.0f, %.0f of %d x %d)"), Screen.X, Screen.Y, Width, Height),
						Screen.X > Width * 0.5f && Screen.X < Width && Screen.Y > Height * 0.5f && Screen.Y < Height);
				}
			}
			if (++State->ViewIndex >= UE_ARRAY_COUNT(Views))
			{
				return EStep::Done;
			}
			State->Since = World->GetTimeSeconds();
			return EStep::Continue;
		} });
		Steps.Add({ TEXT("shots leave from the tip, and kick the weapon back"), [&Test](UWorld* World, int32 Frame)
		{
			UMatsuriViewModelComponent* View = ViewModel(World);
			UMatsuriCombatComponent* Combat = Hero(World)->FindComponentByClass<UMatsuriCombatComponent>();
			if (!View || !Combat)
			{
				return EStep::Done;
			}
			if (Frame < 20)
			{
				World->GetFirstPlayerController()->SetControlRotation(FRotator(-5.0f, 0.0f, 0.0f));
				return EStep::Continue;
			}
			if (Frame == 20)
			{
				TSet<AMatsuriProjectile*> Before;
				for (TActorIterator<AMatsuriProjectile> It(World); It; ++It) { Before.Add(*It); }
				const FVector Muzzle = View->GetMuzzleLocation();
				Test.TestTrue(TEXT("fires"), Combat->TryFire());
				for (TActorIterator<AMatsuriProjectile> It(World); It; ++It)
				{
					if (!Before.Contains(*It))
					{
						Test.TestTrue(FString::Printf(TEXT("the shot starts at the weapon tip (%.2f cm away)"), FVector::Dist(It->GetActorLocation(), Muzzle)),
							FVector::Dist(It->GetActorLocation(), Muzzle) <= MuzzleTolerance);
						break;
					}
				}
				return EStep::Continue;
			}
			if (Frame == 22)
			{
				Test.TestTrue(TEXT("firing kicks the weapon back"), OffsetFromCamera(World, *View).X < View->GetRestOffset().X - 0.5f);
				return EStep::Continue;
			}
			return DoneIf(Frame > 60);	// let the kick recover before the screenshot
		} });
		if (Matsuri::ArtTable::IsBow(static_cast<EMatsuriHero>(HeroIndex)))
		{
			Steps.Add({ TEXT("drawing the bow"), [&Test, State](UWorld* World, int32 Frame)
			{
				UMatsuriViewModelComponent* View = ViewModel(World);
				UMatsuriCombatComponent* Combat = Hero(World)->FindComponentByClass<UMatsuriCombatComponent>();
				UCharacterMovementComponent* Movement = Hero(World)->GetCharacterMovement();
				if (!View || !Combat)
				{
					return EStep::Done;
				}
				World->GetFirstPlayerController()->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
				if (Frame == 0)
				{
					State->Since = World->GetTimeSeconds();
					State->WalkSpeed = Movement->MaxWalkSpeed;
					State->RestNock = OffsetFromCameraOf(World, View->GetDrawLocation());
					Combat->StartFiring();
					return EStep::Continue;
				}
				if (World->GetTimeSeconds() - State->Since < 1.2)
				{
					if (!State->bDrawShot && World->GetTimeSeconds() - State->Since >= 0.9 && FApp::CanEverRender())
					{
						State->bDrawShot = true;	// evidence: the full draw
						FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/MatsuriShots/56_sakura_full_draw.png"), true, false);
					}
					return EStep::Continue;	// a full draw takes 0.8 s
				}
				if (!State->bReleased)
				{
					const FVector Nock = OffsetFromCameraOf(World, View->GetDrawLocation());
					Test.TestTrue(TEXT("fully drawn"), Combat->IsCharging() && Combat->GetChargeAlpha() >= 1.0f);
					Test.TestTrue(FString::Printf(TEXT("the string comes back toward her (%.1f cm)"), State->RestNock.X - Nock.X), State->RestNock.X - Nock.X >= 12.0f);	// the Hanzo-style short draw: 12 cm of string
					Test.TestTrue(TEXT("she walks at half speed while drawing"), FMath::IsNearlyEqual(Movement->MaxWalkSpeed, State->WalkSpeed * UMatsuriCombatComponent::DrawWalkScale, 1.0f));
					Test.TestTrue(TEXT("an arrow is nocked"), View->IsArrowNocked());
					Test.TestTrue(FString::Printf(TEXT("the view narrows with the draw (%.1f degrees)"), View->GetDrawZoom()), View->GetDrawZoom() >= 6.0f);
					TSet<AMatsuriProjectile*> Before;
					for (TActorIterator<AMatsuriProjectile> It(World); It; ++It) { Before.Add(*It); }
					Combat->StopFiring();
					State->bReleased = true;
					for (TActorIterator<AMatsuriProjectile> It(World); It; ++It)
					{
						if (!Before.Contains(*It))
						{
							const float Speed = It->FindComponentByClass<UProjectileMovementComponent>()->Velocity.Size();
							const float Full = Run(World)->GetRunState().GetHero().Weapon.Speed;
							Test.TestTrue(FString::Printf(TEXT("a full draw flies at full speed (%.0f of %.0f)"), Speed, Full), Speed >= Full * 0.95f);
						}
					}
					Test.TestTrue(TEXT("walking speed comes back on release"), FMath::IsNearlyEqual(Movement->MaxWalkSpeed, State->WalkSpeed, 1.0f));
					return EStep::Continue;
				}
				if (World->GetTimeSeconds() - State->Since > 1.8)
				{
					Test.TestTrue(FString::Printf(TEXT("the view comes back after the shot (%.2f degrees)"), View->GetDrawZoom()), FMath::Abs(View->GetDrawZoom()) < 0.5f);
					return EStep::Done;
				}
				return EStep::Continue;
			} });
		}
		Steps.Add({ TEXT("screenshot"), [HeroIndex](UWorld* World, int32 Frame)
		{
			return DoneIf(SettleAndShoot(Frame, HeroShots[HeroIndex], 30));
		} });

		ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(&Test, MoveTemp(Steps)));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriViewModelE2ETest, "SuzuneMatsuri.E2E.ViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriViewModelE2ETest::RunTest(const FString&)
{
	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.God"), true);
	SetCVar(TEXT("matsuri.Maze"), false);	// open ground: nothing in front of the weapon
	for (int32 HeroIndex = 0; HeroIndex < Matsuri::Heroes::Count; ++HeroIndex)
	{
		// Each heroine is a fresh PIE session: the floor reads matsuri.Hero when it starts the run.
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([HeroIndex] { SetIntCVar(TEXT("matsuri.Hero"), HeroIndex); }, 0.0f));
		AddHeroRun(*this, HeroIndex);
	}
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.Threats"), true); SetCVar(TEXT("matsuri.God"), false); SetCVar(TEXT("matsuri.Maze"), true); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
