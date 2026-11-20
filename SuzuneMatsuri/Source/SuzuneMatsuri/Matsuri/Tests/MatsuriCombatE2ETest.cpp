// Suzune's Night Festival - end-to-end combat test in PIE.
// Shoot a chochin (1 hit) and a kasa (3 hits) with real projectiles, check drops and ammo use,
// dry-fire when empty, refill from a talisman, and kill a chochin with a free melee strike.
// Run: Automation RunTests SuzuneMatsuri.E2E.Combat

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "MatsuriAmmoPickup.h"
#include "MatsuriAudio.h"
#include "MatsuriFx.h"
#include "MatsuriCollectible.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriDamageNumber.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriProjectile.h"
#include "MatsuriYokaiKinds.h"
#include "MatsuriYokai.h"

using namespace MatsuriTest;

namespace
{
	struct FCombatState
	{
		TWeakObjectPtr<AMatsuriYokai> Target;
		int32 AmmoBefore = 0;
		int32 CoinsBefore = 0;
		int32 Attacks = 0;		// shots + swings, counted from the weapon's own event
		int32 AttacksBefore = 0;
		int32 DirectionIndex = 0;
		bool bDryFired = false;
		int32 HitsConfirmed = 0;
		int32 SoundsBefore = 0;	// from the weapon's hit-confirm event (drives the HUD hit marker)
		int32 HitsBefore = 0;
		float FiredAt = 0.0f;
	};

	UMatsuriCombatComponent* Combat(UWorld* World)
	{
		return Hero(World)->FindComponentByClass<UMatsuriCombatComponent>();
	}

	/** Spawns a yokai Distance in front of the hero (facing +X from the start) and aims at it. */
	template <typename TYokai>
	TYokai* SpawnInFront(UWorld* World, float Distance)
	{
		const FVector Spot = Hero(World)->GetActorLocation() + FVector(Distance, 0.0f, 0.0f);
		TYokai* Yokai = World->SpawnActor<TYokai>(Spot, FRotator::ZeroRotator);
		Yokai->SetArenaHalfExtent(Matsuri::Floors::SpecFor(0, Run(World)->GetRunState().Seed).HalfExtent);
		return Yokai;
	}

	template <typename TYokai>
	TYokai* SpawnAlong(UWorld* World, const FVector& Dir, float Distance)
	{
		TYokai* Yokai = World->SpawnActor<TYokai>(Hero(World)->GetActorLocation() + Dir * Distance, FRotator::ZeroRotator);
		Yokai->SetArenaHalfExtent(Matsuri::Floors::SpecFor(0, Run(World)->GetRunState().Seed).HalfExtent);
		return Yokai;
	}

	/** Keeps aiming and firing at the target until it dies. */
	bool ShootUntilDead(FCombatState& State, UWorld* World)
	{
		if (!State.Target.IsValid())
		{
			return true;
		}
		AimAt(World, State.Target->GetActorLocation());
		Combat(World)->TryFire();
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriCombatE2ETest, "SuzuneMatsuri.E2E.Combat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriCombatE2ETest::RunTest(const FString&)
{
	TSharedRef<FCombatState> State = MakeShared<FCombatState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;

	Steps.Add({ TEXT("floor loads"), [this, State](UWorld* World, int32 Frame)
	{
		if (!Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			Run(World)->UseRecordSlot(TestRecordSlot);
			TestNotNull(TEXT("hero has a weapon"), Combat(World));
			Combat(World)->OnAttack.AddLambda([State] { ++State->Attacks; });
			Combat(World)->OnHitConfirmed.AddLambda([State] { ++State->HitsConfirmed; });
		}
		return DoneIf(Frame > 5);
	} });
	Steps.Add({ TEXT("shots fly where you aim (8 directions)"), [this, State](UWorld* World, int32)
	{
		// Regression: a world-space launch direction was rotated again as if it were local,
		// so shots only went straight when facing the starting +X direction.
		constexpr int32 Directions = 8;
		constexpr float MinAlignment = 0.98f;	// ~11 degrees; the bug produced 0 or less
		const FRotator Aim(-8.0f, State->DirectionIndex * 360.0f / Directions, 0.0f);
		World->GetFirstPlayerController()->SetControlRotation(Aim);
		if (!Combat(World)->TryFire())
		{
			return EStep::Continue;	// still on cooldown
		}
		AMatsuriProjectile* Newest = nullptr;
		for (TActorIterator<AMatsuriProjectile> It(World); It; ++It)
		{
			if (!Newest || It->GetGameTimeSinceCreation() < Newest->GetGameTimeSinceCreation())
			{
				Newest = *It;
			}
		}
		const float Alignment = Newest ? FVector::DotProduct(Newest->GetVelocity().GetSafeNormal(), Aim.Vector()) : -1.0f;
		TestTrue(FString::Printf(TEXT("shot at yaw %.0f flies along the aim (alignment %.3f)"), Aim.Yaw, Alignment), Alignment > MinAlignment);
		if (++State->DirectionIndex < Directions)
		{
			return EStep::Continue;
		}
		Run(World)->AddAmmo(100);
		World->GetFirstPlayerController()->SetControlRotation(FRotator::ZeroRotator);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("shoot a chochin"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			State->Target = SpawnInFront<AMatsuriChochin>(World, 350.0f);
			State->AmmoBefore = Run(World)->GetRunState().Ammo;
			State->CoinsBefore = CountCollectibles(World);
			return EStep::Continue;
		}
		if (!ShootUntilDead(*State, World))
		{
			return EStep::Continue;
		}
		TestEqual(TEXT("one shot kills a chochin"), Run(World)->GetRunState().Ammo, State->AmmoBefore - 1);
		TestEqual(TEXT("chochin drops a koban"), CountCollectibles(World), State->CoinsBefore + 1);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("shoot a kasa"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			State->Target = SpawnInFront<AMatsuriKasa>(World, 380.0f);
			State->AttacksBefore = State->Attacks;
			return EStep::Continue;
		}
		if (!ShootUntilDead(*State, World))
		{
			return EStep::Continue;
		}
		// Count shots, not ammo: a kasa that dies at the hero's feet may drop ammo that is picked up at once.
		TestTrue(TEXT("a kasa takes three hits"), State->Attacks - State->AttacksBefore >= 3);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("combat showcase screenshot"), [](UWorld* World, int32 Frame)
	{
		constexpr int32 ShotSettle = 24;	// short: yokai close in fast
		if (Frame == 0)
		{
			Run(World)->AddAmmo(20);
			SpawnInFront<AMatsuriChochin>(World, 650.0f)->AddActorWorldOffset(FVector(0.0f, -220.0f, 0.0f));
			SpawnInFront<AMatsuriKasa>(World, 560.0f);
			SpawnInFront<AMatsuriKappa>(World, 900.0f)->AddActorWorldOffset(FVector(0.0f, 260.0f, 0.0f));
		}
		AimAt(World, Hero(World)->GetActorLocation() + FVector(700.0f, 0.0f, 20.0f));
		if (Frame % 5 == 0)
		{
			Combat(World)->TryFire();	// bell shots in flight for the picture
		}
		if (!SettleAndShoot(Frame, TEXT("10_combat"), ShotSettle))
		{
			return EStep::Continue;
		}
		for (TActorIterator<AMatsuriYokai> It(World); It; ++It)
		{
			It->Destroy();
		}
		return EStep::Done;
	} });
	Steps.Add({ TEXT("dry fire then refill"), [this, State](UWorld* World, int32 Frame)
	{
		UMatsuriRunSubsystem* RunState = Run(World);
		if (Frame == 0)
		{
			RunState->ConsumeAmmo(RunState->GetRunState().Ammo);
			Combat(World)->OnDryFire.AddLambda([State] { State->bDryFired = true; });
			for (TActorIterator<AMatsuriAmmoPickup> It(World); It; ++It)
			{
				It->Destroy();	// earlier kills may have dropped ammo; the refill must come from our talisman only
			}
			return EStep::Continue;
		}
		if (Frame == 20)
		{
			TestFalse(TEXT("cannot fire with no ammo"), Combat(World)->TryFire());
			TestTrue(TEXT("dry fire is reported (HUD flashes)"), State->bDryFired);
			AMatsuriAmmoPickup* Talisman = World->SpawnActorDeferred<AMatsuriAmmoPickup>(AMatsuriAmmoPickup::StaticClass(),
				FTransform(Hero(World)->GetActorLocation() + FVector(150.0f, 0.0f, 0.0f)));
			Talisman->Configure(5, false);
			Talisman->FinishSpawning(Talisman->GetActorTransform());
			TeleportHero(World, Talisman->GetActorLocation());
		}
		if (Frame < 25)
		{
			return EStep::Continue;
		}
		TestEqual(TEXT("talisman refills ammo"), RunState->GetRunState().Ammo, 5);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("reset for melee"), [State](UWorld* World, int32 Frame)
	{
		for (TActorIterator<AMatsuriYokai> It(World); It; ++It) { It->Destroy(); }
		World->GetFirstPlayerController()->SetControlRotation(FRotator::ZeroRotator);
		Run(World)->ConsumeAmmo(Run(World)->GetRunState().Ammo);
		return DoneIf(Frame > 40);	// also lets the previous swing cool down
	} });
	Steps.Add({ TEXT("one strike fells a lantern ghost"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			State->Target = SpawnInFront<AMatsuriChochin>(World, 160.0f);
			return EStep::Continue;
		}
		const int32 FxBefore = CountFx(World);
		TestTrue(TEXT("strike lands"), Combat(World)->TryMelee());
		TestFalse(TEXT("2 damage: a lantern ghost dies to one strike"), State->Target.IsValid());
		TestEqual(TEXT("melee uses no ammo"), Run(World)->GetRunState().Ammo, 0);
		TestTrue(TEXT("the swing and the kill are visible"), CountFx(World) > FxBefore);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("strike shoves and staggers, but only in front"), [this, State](UWorld* World, int32 Frame)
	{
		static TWeakObjectPtr<AMatsuriKasa> Front, Behind;
		static FVector FrontBefore;
		if (Frame == 0)
		{
			// Bells ricochet now: a stray one from an earlier step must not hit the kasa behind.
			for (TActorIterator<AMatsuriProjectile> It(World); It; ++It) { It->Destroy(); }
			const FVector Dir = ClearDirection(World, 650.0f);	// room for the 350 cm shove
			Front = SpawnAlong<AMatsuriKasa>(World, Dir, 180.0f);
			Behind = SpawnAlong<AMatsuriKasa>(World, -Dir, 180.0f);
			Behind->Stun(10.0f);	// held in place: a hop round to the front would put it in the swing
			return EStep::Continue;
		}
		if (Frame < 40)
		{
			AimAt(World, Front->GetActorLocation());
			return EStep::Continue;	// swing cooldown; face the front kasa
		}
		FrontBefore = Front->GetActorLocation();
		TestTrue(TEXT("strike lands"), Combat(World)->TryMelee());
		TestTrue(TEXT("kasa survives with 1 of 3 health"), Front.IsValid() && FMath::IsNearlyEqual(Front->GetHealth(), 1.0f));
		TestTrue(TEXT("kasa is knocked back"), Front.IsValid() && FVector::Dist2D(Front->GetActorLocation(), FrontBefore) > 250.0f);
		TestTrue(TEXT("kasa is staggered"), Front.IsValid() && Front->IsStunned());
		TestTrue(TEXT("the yokai behind is untouched"), Behind.IsValid() && FMath::IsNearlyEqual(Behind->GetHealth(), 3.0f));
		return EStep::Done;
	} });
	Steps.Add({ TEXT("strike swats a cucumber"), [this, State](UWorld* World, int32 Frame)
	{
		static TWeakObjectPtr<AMatsuriProjectile> Cucumber;
		static bool bDeflected = false;
		if (Frame == 0)
		{
			for (TActorIterator<AMatsuriYokai> It(World); It; ++It) { It->Destroy(); }
			Combat(World)->OnDeflect.AddLambda([] { bDeflected = true; });
			const FVector Eye = Hero(World)->GetActorLocation() + FVector(0, 0, 60.0f);
			const FVector From = Eye + ClearDirection(World, 1000.0f) * 900.0f;	// a crate must not eat it first
			AimAt(World, From);	// the swing goes where the hero faces
			const FTransform Transform((Eye - From).Rotation(), From);
			AMatsuriProjectile* Shot = World->SpawnActorDeferred<AMatsuriProjectile>(AMatsuriProjectile::StaticClass(), Transform);
			FMatsuriShot Spec;
			Spec.Direction = Eye - From;
			Spec.Speed = 900.0f;
			Spec.bHostileToPlayer = true;
			Spec.Color = FLinearColor::Green;
			Shot->Configure(Spec);
			Shot->FinishSpawning(Transform);
			Cucumber = Shot;
			// A dropped coin in its path must not swallow it (pickup triggers used to block hostile shots).
			World->SpawnActor<AMatsuriCollectible>((From + Eye) * 0.5f, FRotator::ZeroRotator);
			return EStep::Continue;
		}
		if (Cucumber.IsValid() && FVector::Dist2D(Cucumber->GetActorLocation(), Hero(World)->GetActorLocation()) > 200.0f)
		{
			return EStep::Continue;	// let it come within reach
		}
		TestTrue(TEXT("cucumber still in the air when swung at"), Cucumber.IsValid());
		TestTrue(TEXT("strike lands"), Combat(World)->TryMelee());
		TestFalse(TEXT("cucumber knocked out of the air"), Cucumber.IsValid());
		TestTrue(TEXT("deflect is reported"), bDeflected);
		return EStep::Done;
	} });
	// Playtest: "hits register late". A yokai pressed against the muzzle must still be hit, at once.
	Steps.Add({ TEXT("point-blank shot registers"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			for (TActorIterator<AMatsuriYokai> It(World); It; ++It) { It->Destroy(); }
			Run(World)->AddAmmo(20);
			State->Target = SpawnInFront<AMatsuriKasa>(World, 70.0f);	// body spans 20..120 cm: the muzzle (50 cm) is inside it
			State->Target->Stun(10.0f);	// hold still: this checks registration, not aim
			return EStep::Continue;
		}
		if (Frame < 30)
		{
			AimAt(World, State->Target->GetActorLocation());
			return EStep::Continue;	// fire interval, and the camera catching up with the aim
		}
		if (Frame == 30)
		{
			State->HitsBefore = State->HitsConfirmed;
			TestTrue(TEXT("fires"), Combat(World)->TryFire());
			return EStep::Continue;
		}
		TestTrue(TEXT("the kasa touching the muzzle is hit"), !State->Target.IsValid() || State->Target->GetHealth() < 3.0f);
		TestTrue(TEXT("the hit is confirmed"), State->HitsConfirmed > State->HitsBefore);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("a hit across a corridor lands fast and is confirmed the same frame"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			for (TActorIterator<AMatsuriYokai> It(World); It; ++It) { It->Destroy(); }
			State->Target = SpawnAlong<AMatsuriKasa>(World, ClearDirection(World, 1600.0f), 1500.0f);	// a clear corridor
			State->Target->Stun(10.0f);	// a hopping kasa could dodge; this checks flight time
			return EStep::Continue;
		}
		if (Frame < 30)
		{
			AimAt(World, State->Target->GetActorLocation());
			return EStep::Continue;
		}
		if (Frame == 30)
		{
			State->HitsBefore = State->HitsConfirmed;
			State->FiredAt = World->GetTimeSeconds();
			State->SoundsBefore = Matsuri::Audio::PlayCount(EMatsuriSfx::HitTick) + Matsuri::Audio::PlayCount(EMatsuriSfx::Kill);
			const int32 Bells = Matsuri::Audio::PlayCount(EMatsuriSfx::BellShot);
			TestTrue(TEXT("fires"), Combat(World)->TryFire());
			TestTrue(TEXT("Mio's shot rings her bell"), Matsuri::Audio::PlayCount(EMatsuriSfx::BellShot) > Bells);
			return EStep::Continue;
		}
		const bool bHit = !State->Target.IsValid() || State->Target->GetHealth() < 3.0f;
		const float Elapsed = World->GetTimeSeconds() - State->FiredAt;
		if (!bHit)
		{
			if (Elapsed > 1.0f)
			{
				AddError(TEXT("the shot never reached the kasa 15 m away"));
				return EStep::Done;
			}
			return EStep::Continue;
		}
		TestTrue(FString::Printf(TEXT("hit lands within 0.25 s (took %.3f s)"), Elapsed), Elapsed <= 0.25f);
		TestTrue(TEXT("the hit marker fires on the frame of the hit"), State->HitsConfirmed > State->HitsBefore);
		int32 Numbers = 0;
		for (TActorIterator<AMatsuriDamageNumber> It(World); It; ++It) { ++Numbers; }
		TestTrue(TEXT("the hit pops a damage number"), Numbers > 0);
		TestTrue(TEXT("a yokai bursting shows the engine's explosion template"), Matsuri::Fx::TemplateCount() > 0 || !FModuleManager::Get().IsModuleLoaded(TEXT("Niagara")));
		TestTrue(TEXT("the hit is heard (tick or kill chime)"), Matsuri::Audio::PlayCount(EMatsuriSfx::HitTick) + Matsuri::Audio::PlayCount(EMatsuriSfx::Kill) > State->SoundsBefore);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("a melee hit is confirmed too"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			for (TActorIterator<AMatsuriYokai> It(World); It; ++It) { It->Destroy(); }
			State->Target = SpawnInFront<AMatsuriKasa>(World, 160.0f);
			State->Target->Stun(10.0f);
			return EStep::Continue;
		}
		if (Frame < 40)
		{
			AimAt(World, State->Target->GetActorLocation());
			return EStep::Continue;	// swing cooldown; face it (the last step aimed elsewhere)
		}
		State->HitsBefore = State->HitsConfirmed;
		const int32 Swings = Matsuri::Audio::PlayCount(EMatsuriSfx::MeleeSwing);
		const int32 Thuds = Matsuri::Audio::PlayCount(EMatsuriSfx::MeleeHit);
		TestTrue(TEXT("strike lands"), Combat(World)->TryMelee());
		TestTrue(TEXT("melee hit confirmed"), State->HitsConfirmed > State->HitsBefore);
		TestTrue(TEXT("the swing whooshes and the hit lands with a sound"), Matsuri::Audio::PlayCount(EMatsuriSfx::MeleeSwing) > Swings && Matsuri::Audio::PlayCount(EMatsuriSfx::MeleeHit) > Thuds);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("effect pieces are reused, not spawned again"), [this, State](UWorld* World, int32 Frame)
	{
		// Profiling: spawning a fresh actor per effect piece cost a swing 20-40 ms. Once the last swing's
		// pieces have faded (and the cooldown is over), a new swing must take them from the pool.
		constexpr int32 FadeFrames = 60;
		if (Frame < FadeFrames)
		{
			return EStep::Continue;
		}
		int32 Pieces = 0;
		for (TActorIterator<AMatsuriFx> It(World); It; ++It) { ++Pieces; }
		TestTrue(TEXT("swings again"), Combat(World)->TryMelee());
		int32 PiecesAfter = 0;
		for (TActorIterator<AMatsuriFx> It(World); It; ++It) { ++PiecesAfter; }
		TestTrue(FString::Printf(TEXT("no new effect actors (%d before, %d after)"), Pieces, PiecesAfter), PiecesAfter <= Pieces);
		TestTrue(TEXT("and the swing is visible"), CountFx(World) > 0);
		return EStep::Done;
	} });

	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.God"), true);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Mio));
	SetCVar(TEXT("matsuri.Maze"), false);	// open arena: shots and spawns must not depend on a random maze
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); SetCVar(TEXT("matsuri.God"), false); SetCVar(TEXT("matsuri.Maze"), true); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
