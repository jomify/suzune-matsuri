// Suzune's Night Festival - shots obey their physics in a real floor (PIE).
// Playtest: "arrows and thrown knives should obey physics". Each heroine fires at the floor ahead:
// Sakura's arrows and Suzune's kunai stick into it, pointing down along their flight; Mio's bell bounces
// back up. A screenshot shows the stuck shots. Skips the sticking checks without the weapon art.
// Run: Automation RunTests SuzuneMatsuri.E2E.ShotPhysics

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Engine/StaticMeshActor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriArt.h"
#include "MatsuriAudio.h"
#include "MatsuriArtTable.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriProjectile.h"

using namespace MatsuriTest;

namespace
{
	constexpr double WaitSeconds = 1.5;
	const FRotator AtTheFloor(-35.0f, 0.0f, 0.0f);	// the floor about 2.5 m ahead
	const TCHAR* const Shots[] = { TEXT("53_stuck_arrows"), TEXT("54_stuck_kunai"), TEXT("55_bell_bounce") };

	struct FShotState
	{
		double FiredAt = 0.0;
		int32 Fired = 0;
		TWeakObjectPtr<AMatsuriProjectile> Bell;
		bool bBellFell = false;
		bool bBellRose = false;
		int32 SoundsBefore = 0;
	};

	int32 CountStuck(UWorld* World, FVector* OutForward = nullptr)
	{
		int32 Count = 0;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("MatsuriStuck")))
			{
				++Count;
				if (OutForward)
				{
					*OutForward = It->GetActorForwardVector();
				}
			}
		}
		return Count;
	}

	void AddHeroRun(FAutomationTestBase& Test, int32 HeroIndex)
	{
		TSharedRef<FShotState> State = MakeShared<FShotState>();
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
			World->GetFirstPlayerController()->SetControlRotation(AtTheFloor);
			return DoneIf(Frame > 20 && World->GetTimeSeconds() > 1.5);	// the floor's start-up has settled the view
		} });
		Steps.Add({ TEXT("fire at the floor"), [&Test, State, HeroIndex](UWorld* World, int32 Frame)
		{
			UMatsuriCombatComponent* Combat = Hero(World)->FindComponentByClass<UMatsuriCombatComponent>();
			const EMatsuriHero Heroine = static_cast<EMatsuriHero>(HeroIndex);
			const bool bArt = Matsuri::Art::Mesh(Matsuri::ArtTable::Shot(Heroine).Mesh) != nullptr;
			if (Frame == 0)
			{
				State->FiredAt = World->GetTimeSeconds();
				State->SoundsBefore = Matsuri::Audio::PlayCount(Heroine == EMatsuriHero::Mio ? EMatsuriSfx::BellBounce : EMatsuriSfx::ArrowThunk);
			}
			// Three shots, a little apart, so the screenshot shows a cluster. The aim is set a moment before
			// each shot: the camera takes the new rotation on the next frame.
			const double Since = World->GetTimeSeconds() - State->FiredAt;
			if (State->Fired < 3)
			{
				World->GetFirstPlayerController()->SetControlRotation(AtTheFloor + FRotator(0.0f, (State->Fired - 1) * 6.0f, 0.0f));
			}
			if (State->Fired < 3 && Since >= 0.2 + State->Fired * 0.5)
			{
				TSet<AMatsuriProjectile*> Before;
				for (TActorIterator<AMatsuriProjectile> It(World); It; ++It) { Before.Add(*It); }
				if (Combat->TryFire(1.0f))
				{
					++State->Fired;
					for (TActorIterator<AMatsuriProjectile> It(World); It; ++It)
					{
						if (!Before.Contains(*It) && !State->Bell.IsValid())
						{
							State->Bell = *It;
							State->bBellFell = true;	// fired at the floor: it starts out falling (and may land this very frame)
						}
					}
				}
				return EStep::Continue;
			}
			// Watch the first shot fly: does it come back up off the floor?
			if (AMatsuriProjectile* Shot = State->Bell.Get())
			{
				const float Vz = Shot->FindComponentByClass<UProjectileMovementComponent>()->Velocity.Z;
				State->bBellFell |= Vz < -50.0f;
				State->bBellRose |= State->bBellFell && Vz > 50.0f;
			}
			if (Since < 1.0 + WaitSeconds)
			{
				return EStep::Continue;
			}

			if (Heroine == EMatsuriHero::Mio)
			{
				Test.TestTrue(TEXT("the bell bounced back up off the floor"), State->bBellRose);
				Test.TestTrue(TEXT("the bounce clinks"), Matsuri::Audio::PlayCount(EMatsuriSfx::BellBounce) > State->SoundsBefore);
				Test.TestEqual(TEXT("bells do not stick"), CountStuck(World), 0);
			}
			else if (bArt)
			{
				FVector Forward;
				const int32 Stuck = CountStuck(World, &Forward);
				Test.TestTrue(FString::Printf(TEXT("%d shot(s) stuck in the floor"), Stuck), Stuck >= 2);
				Test.TestTrue(FString::Printf(TEXT("a stuck shot points down along its flight (forward %s)"), *Forward.ToCompactString()), Forward.Z < -0.2f);
				Test.TestFalse(TEXT("a sticking shot does not bounce"), State->bBellRose);
				Test.TestTrue(TEXT("it lands with a thunk"), Matsuri::Audio::PlayCount(EMatsuriSfx::ArrowThunk) > State->SoundsBefore);
			}
			return EStep::Done;
		} });
		Steps.Add({ TEXT("screenshot"), [HeroIndex](UWorld* World, int32 Frame)
		{
			World->GetFirstPlayerController()->SetControlRotation(FRotator(-25.0f, 0.0f, 0.0f));
			return DoneIf(SettleAndShoot(Frame, Shots[HeroIndex], 20));
		} });

		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([HeroIndex] { SetIntCVar(TEXT("matsuri.Hero"), HeroIndex); }, 0.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(&Test, MoveTemp(Steps)));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriShotPhysicsE2ETest, "SuzuneMatsuri.E2E.ShotPhysics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriShotPhysicsE2ETest::RunTest(const FString&)
{
	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.God"), true);
	SetCVar(TEXT("matsuri.Maze"), false);	// open ground in front
	for (int32 HeroIndex = 0; HeroIndex < Matsuri::Heroes::Count; ++HeroIndex)
	{
		AddHeroRun(*this, HeroIndex);
	}
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.Threats"), true); SetCVar(TEXT("matsuri.God"), false); SetCVar(TEXT("matsuri.Maze"), true); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
