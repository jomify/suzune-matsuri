// Suzune's Night Festival - end-to-end test of every heroine's weapon, passive and Shift / E / Q skill.
// One PIE session per heroine (matsuri.Hero), real yokai spawned in front of her.
// Run: Automation RunTests SuzuneMatsuri.E2E.Skills

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriDecoy.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriProjectile.h"
#include "MatsuriSkillComponent.h"
#include "MatsuriYokaiKinds.h"

using namespace MatsuriTest;

namespace
{
	using FSteps = TArray<TPair<FString, FStepRunner::FStep>>;

	struct FSkillState
	{
		TArray<TWeakObjectPtr<AActor>> Targets;
		FVector Mark = FVector::ZeroVector;
		int32 Count = 0;
		int32 Number = 0;
		double StartTime = 0.0;	// world seconds; waits are time-based so they hold at any frame rate

		void Start(const UWorld* World) { StartTime = World->GetTimeSeconds(); }
		double Since(const UWorld* World) const { return World->GetTimeSeconds() - StartTime; }
	};

	UMatsuriSkillComponent* Skills(UWorld* World) { return Hero(World)->FindComponentByClass<UMatsuriSkillComponent>(); }
	UMatsuriCombatComponent* Combat(UWorld* World) { return Hero(World)->FindComponentByClass<UMatsuriCombatComponent>(); }
	AMatsuriFloorGameMode* FloorMode(UWorld* World) { return World->GetAuthGameMode<AMatsuriFloorGameMode>(); }

	/** Spawns a yokai at an offset from the hero (x forward, y right; the hero faces +X). */
	template <typename TYokai>
	TYokai* SpawnNear(UWorld* World, const FVector2D& Offset)
	{
		const FVector Spot = Hero(World)->GetActorLocation() + FVector(Offset, 0.0f);
		TYokai* Yokai = World->SpawnActor<TYokai>(Spot, FRotator::ZeroRotator);
		Yokai->SetArenaHalfExtent(Matsuri::Floors::SpecFor(0, Run(World)->GetRunState().Seed).HalfExtent);
		return Yokai;
	}

	/** Uses a skill and checks it left something on screen. */
	bool UseSkillWithFx(FAutomationTestBase& Test, UWorld* World, EMatsuriSkillSlot Slot)
	{
		const int32 Before = CountFx(World);
		const bool bUsed = Skills(World)->TryUseSkill(Slot);
		Test.TestTrue(FString::Printf(TEXT("skill %d shows an effect"), static_cast<int32>(Slot)), !bUsed || CountFx(World) > Before);
		return bUsed;
	}

	int32 CountAlive(const FSkillState& State)
	{
		return State.Targets.FilterByPredicate([](const TWeakObjectPtr<AActor>& Target) { return Target.IsValid(); }).Num();
	}

	void ClearYokai(UWorld* World)
	{
		for (TActorIterator<AMatsuriYokai> It(World); It; ++It)
		{
			It->Destroy();
		}
	}

	int32 CountProjectiles(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AMatsuriProjectile> It(World); It; ++It) { ++Count; }
		return Count;
	}

	/** Location of target Index, or the hero's own spot if it is gone (the assertion that follows then fails, no crash). */
	FVector TargetLocation(UWorld* World, const FSkillState& State, int32 Index)
	{
		const AActor* Target = State.Targets.IsValidIndex(Index) ? State.Targets[Index].Get() : nullptr;
		return Target ? Target->GetActorLocation() : Hero(World)->GetActorLocation();
	}

	/** Shared first step: floor up, hero facing +X at the start, test save slot, fresh test state. */
	void AddFloorReady(FSteps& Steps, TSharedRef<FSkillState> State)
	{
		Steps.Add({ TEXT("floor ready"), [State](UWorld* World, int32 Frame)
		{
			if (!Hero(World) || !FindWidget<UMatsuriHUDWidget>(World) || !Skills(World))
			{
				return EStep::Wait;
			}
			if (Frame == 0)
			{
				*State = FSkillState();	// targets from the previous heroine's PIE world are gone
				Run(World)->UseRecordSlot(TestRecordSlot);
				// Mio's magnets can hoover up the whole floor; clearing it would pause the game mid-test.
				Run(World)->SetFloorTargetForTesting(100000);
				World->GetFirstPlayerController()->SetControlRotation(FRotator::ZeroRotator);
			}
			return DoneIf(Frame > 5);
		} });
	}

	/** Puts the hero back at the start, facing +X, with no yokai around. */
	FStepRunner::FStep Reset(TSharedRef<FSkillState> State)
	{
		return [State](UWorld* World, int32 Frame)
		{
			ClearYokai(World);
			TeleportHero(World, FloorMode(World)->GetCheckpoint());
			World->GetFirstPlayerController()->SetControlRotation(FRotator::ZeroRotator);
			State->Targets.Reset();
			return DoneIf(Frame > 2);
		};
	}

	void AddSakura(FAutomationTestBase& Test, FSteps& Steps, TSharedRef<FSkillState> State)
	{
		Steps.Add({ TEXT("sakura passive"), [&Test](UWorld* World, int32)
		{
			Test.TestEqual(TEXT("sakura starts with extra health"), Run(World)->GetRunState().Health, Matsuri::Run::StartHealth + 50.0f);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("sakura charged arrow pierces three"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				for (const float X : { 300.0f, 450.0f, 600.0f })
				{
					State->Targets.Add(SpawnNear<AMatsuriChochin>(World, FVector2D(X, 0.0f)));
				}
				return EStep::Continue;
			}
			if (Frame == 1)
			{
				AimAt(World, TargetLocation(World, *State, 1));	// the camera follows next frame
			}
			if (Frame == 3)
			{
				Test.TestTrue(TEXT("full-charge arrow fires"), Combat(World)->TryFire(1.0f));
				State->Start(World);
			}
			if (Frame <= 3 || State->Since(World) < 0.5)
			{
				return EStep::Continue;
			}
			Test.TestEqual(TEXT("one arrow through three lantern ghosts"), CountAlive(*State), 0);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("reset"), Reset(State) });
		Steps.Add({ TEXT("sakura Shift barrier"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				State->Targets.Add(SpawnNear<AMatsuriKasa>(World, FVector2D(200.0f, 0.0f)));
				return EStep::Continue;
			}
			if (Frame == 2)
			{
				State->Mark = TargetLocation(World, *State, 0);
				Test.TestTrue(TEXT("結界 fires"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::Shift));
				Test.TestTrue(TEXT("結界 makes the hero invulnerable"), FloorMode(World)->IsPlayerInvulnerable());
				const AMatsuriYokai* Kasa = Cast<AMatsuriYokai>(State->Targets[0].Get());
				Test.TestNotNull(TEXT("kasa still there"), Kasa);
				Test.TestTrue(TEXT("結界 pushes the yokai away"), Kasa && FVector::Dist2D(Kasa->GetActorLocation(), Hero(World)->GetActorLocation()) > 450.0f);
				Test.TestTrue(TEXT("結界 staggers the yokai"), Kasa && Kasa->IsStunned());
				Test.TestFalse(TEXT("cooldown blocks a second use"), Skills(World)->TryUseSkill(EMatsuriSkillSlot::Shift));
			}
			return DoneIf(Frame > 3);
		} });
		Steps.Add({ TEXT("reset"), Reset(State) });
		Steps.Add({ TEXT("sakura E purify cone"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				State->Targets.Add(SpawnNear<AMatsuriKasa>(World, FVector2D(450.0f, 0.0f)));
				State->Targets.Add(SpawnNear<AMatsuriKasa>(World, FVector2D(-450.0f, 0.0f)));	// behind: outside the cone
				return EStep::Continue;
			}
			if (Frame == 2)
			{
				Test.TestTrue(TEXT("お祓い fires"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::E));
				Test.TestTrue(TEXT("yokai in front is stunned"), (Cast<AMatsuriYokai>(State->Targets[0].Get()) && Cast<AMatsuriYokai>(State->Targets[0].Get())->IsStunned()));
				Test.TestFalse(TEXT("yokai behind is not"), (Cast<AMatsuriYokai>(State->Targets[1].Get()) && Cast<AMatsuriYokai>(State->Targets[1].Get())->IsStunned()));
				State->Mark = TargetLocation(World, *State, 0);
				State->Start(World);
			}
			if (Frame <= 2 || State->Since(World) < 1.5)	// stun lasts 3 s
			{
				return EStep::Continue;
			}
			Test.TestTrue(TEXT("stunned yokai stays put"), FVector::Dist2D(TargetLocation(World, *State, 0), State->Mark) < 5.0f);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("reset"), Reset(State) });
		Steps.Add({ TEXT("sakura Q great purge"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				State->Targets.Add(SpawnNear<AMatsuriKappa>(World, FVector2D(900.0f, 300.0f)));
				State->Targets.Add(SpawnNear<AMatsuriKasa>(World, FVector2D(-700.0f, -500.0f)));
				Test.TestFalse(TEXT("ultimate needs a full charge"), Skills(World)->TryUseSkill(EMatsuriSkillSlot::Q));
				Run(World)->AddUltCharge(1.0f);
				return EStep::Continue;
			}
			Test.TestTrue(TEXT("大祓 fires when charged"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::Q));
			Test.TestEqual(TEXT("every yokai within 15 m purged"), CountAlive(*State), 0);
			Test.TestEqual(TEXT("ultimate spent"), Run(World)->GetRunState().UltCharge, 0.0f);
			return EStep::Done;
		} });
	}

	void AddSuzune(FAutomationTestBase& Test, FSteps& Steps, TSharedRef<FSkillState> State)
	{
		Steps.Add({ TEXT("suzune passives and fan"), [&Test](UWorld* World, int32 Frame)
		{
			ACharacter* Player = Hero(World);
			const float BaseSpeed = Player->GetClass()->GetDefaultObject<ACharacter>()->GetCharacterMovement()->MaxWalkSpeed;
			Test.TestEqual(TEXT("double jump"), Player->JumpMaxCount, 2);
			Test.TestEqual(TEXT("15% faster"), Player->GetCharacterMovement()->MaxWalkSpeed, BaseSpeed * 1.15f, 0.5f);
			const int32 AmmoBefore = Run(World)->GetRunState().Ammo;
			const int32 ShotsBefore = CountProjectiles(World);
			Test.TestTrue(TEXT("throws"), Combat(World)->TryFire());
			Test.TestEqual(TEXT("three shuriken per throw"), CountProjectiles(World) - ShotsBefore, 3);
			Test.TestEqual(TEXT("one ammo per throw"), Run(World)->GetRunState().Ammo, AmmoBefore - 1);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("suzune Shift blink"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				// Expected distance: the full 6 m, unless scenery is in the way (then she stops in front of it).
				ACharacter* Player = Hero(World);
				const FVector Start = Player->GetActorLocation();
				FHitResult Block;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(MatsuriTestBlink), false, Player);
				const bool bBlocked = World->SweepSingleByChannel(Block, Start, Start + FVector(600.0f, 0.0f, 0.0f), FQuat::Identity, ECC_Pawn,
					Player->GetCapsuleComponent()->GetCollisionShape(), Params);
				const float Expected = bBlocked ? static_cast<float>(Block.Location.X - Start.X) : 600.0f;

				Test.TestTrue(TEXT("瞬身 fires"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::Shift));
				const float Moved = static_cast<float>(Player->GetActorLocation().X - Start.X);
				Test.TestEqual(FString::Printf(TEXT("blinks forward (%s)"), bBlocked ? TEXT("stopped by scenery") : TEXT("clear: 6 m")), Moved, Expected, 5.0f);
				Test.TestTrue(TEXT("blink went somewhere"), Moved > 50.0f || bBlocked);
				return EStep::Continue;
			}
			return DoneIf(Frame > 2);
		} });
		Steps.Add({ TEXT("suzune blink stops at walls"), [&Test](UWorld* World, int32 Frame)
		{
			const float WallFace = Matsuri::Floors::SpecFor(0, Run(World)->GetRunState().Seed).HalfExtent;
			if (Frame == 0)
			{
				TeleportHero(World, FVector(WallFace - 250.0f, 0.0f, 120.0f));
				return EStep::Continue;
			}
			if (Skills(World)->GetCooldownRemaining(EMatsuriSkillSlot::Shift) > 0.0f)
			{
				return EStep::Continue;
			}
			Test.TestTrue(TEXT("瞬身 fires toward the wall"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::Shift));
			ACharacter* Player = Hero(World);
			Test.TestTrue(TEXT("blink does not pass the wall"), Player->GetActorLocation().X + Player->GetSimpleCollisionRadius() <= WallFace + 1.0f);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("reset"), Reset(State) });
		Steps.Add({ TEXT("suzune E decoy lures yokai"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				Test.TestTrue(TEXT("分身 fires"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::E));
				Test.TestTrue(TEXT("decoy exists"), TActorIterator<AMatsuriDecoy>(World) ? true : false);
				TeleportHero(World, Hero(World)->GetActorLocation() + FVector(0.0f, -1200.0f, 0.0f));	// walk away from it
				State->Targets.Add(SpawnNear<AMatsuriChochin>(World, FVector2D(0.0f, 1900.0f)));	// 700 from the decoy
				return EStep::Continue;
			}
			if (Frame == 1)
			{
				State->Mark = TActorIterator<AMatsuriDecoy>(World)->GetActorLocation();
				State->Number = FMath::RoundToInt32(FVector::Dist2D(TargetLocation(World, *State, 0), State->Mark));
				State->Start(World);
			}
			if (Frame <= 1 || State->Since(World) < 1.5)
			{
				return EStep::Continue;
			}
			const float Now = FVector::Dist2D(TargetLocation(World, *State, 0), State->Mark);
			Test.TestTrue(FString::Printf(TEXT("ghost drifts to the decoy (%d -> %.0f cm)"), State->Number, Now), Now < State->Number - 150.0f);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("reset"), Reset(State) });
		Steps.Add({ TEXT("suzune Q shadow stitch"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				State->Targets.Add(SpawnNear<AMatsuriKappa>(World, FVector2D(800.0f, 250.0f)));
				Run(World)->AddUltCharge(1.0f);
				Test.TestTrue(TEXT("影縫い fires when charged"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::Q));
				State->Count = Run(World)->GetRunState().Ammo;
				State->Number = 0;
				return EStep::Continue;
			}
			if (Combat(World)->TryFire())
			{
				++State->Number;
				for (TActorIterator<AMatsuriProjectile> It(World); It; ++It)
				{
					Test.TestTrue(TEXT("stitched shuriken home in"), It->FindComponentByClass<UProjectileMovementComponent>()->bIsHomingProjectile);
				}
			}
			if (State->Number < 3)
			{
				return EStep::Continue;
			}
			Test.TestEqual(TEXT("stitched throws cost no ammo"), Run(World)->GetRunState().Ammo, State->Count);
			return EStep::Done;
		} });
	}

	void AddMio(FAutomationTestBase& Test, FSteps& Steps, TSharedRef<FSkillState> State)
	{
		Steps.Add({ TEXT("mio ginka passive"), [&Test](UWorld* World, int32)
		{
			Test.TestEqual(TEXT("ginka worth 3 for Mio"), Run(World)->GetRunState().PointsFor(EMatsuriCollectible::Ginka), 3);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("mio bell bounces"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				State->Targets.Add(SpawnNear<AMatsuriChochin>(World, FVector2D(380.0f, 0.0f)));
				State->Targets.Add(SpawnNear<AMatsuriChochin>(World, FVector2D(380.0f, 320.0f)));
				return EStep::Continue;
			}
			if (Frame == 1)
			{
				AimAt(World, TargetLocation(World, *State, 0));	// the camera follows next frame
			}
			if (Frame == 3)
			{
				State->Count = Run(World)->GetRunState().Ammo;
				Test.TestTrue(TEXT("bell fires"), Combat(World)->TryFire());
				State->Start(World);
			}
			if (Frame <= 3 || State->Since(World) < 0.6)
			{
				return EStep::Continue;
			}
			Test.TestEqual(TEXT("one bell, two lantern ghosts"), CountAlive(*State), 0);
			Test.TestEqual(TEXT("one ammo used"), Run(World)->GetRunState().Ammo, State->Count - 1);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("reset"), Reset(State) });
		Steps.Add({ TEXT("mio Shift beckoning cat"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				const FVector Spot = Hero(World)->GetActorLocation() + FVector(0.0f, 450.0f, 0.0f);
				AMatsuriCollectible* Coin = World->SpawnActor<AMatsuriCollectible>(FVector(Spot.X, Spot.Y, 90.0f), FRotator::ZeroRotator);
				State->Targets.Add(Coin);
				State->Count = Run(World)->GetRunState().Score.RunScore;
				Test.TestTrue(TEXT("招き猫 fires"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::Shift));
				State->Start(World);
				return EStep::Continue;
			}
			if (State->Targets[0].IsValid() && State->Since(World) < 3.0)
			{
				return EStep::Continue;
			}
			Test.TestFalse(TEXT("coin flew in and was collected"), State->Targets[0].IsValid());
			// At least the koban; the magnet may also pull in coins dropped earlier in this session.
			Test.TestTrue(TEXT("koban scored"), Run(World)->GetRunState().Score.RunScore >= State->Count + 1);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("reset"), Reset(State) });
		Steps.Add({ TEXT("mio E zeni"), [&Test, State](UWorld* World, int32 Frame)
		{
			UMatsuriRunSubsystem* RunState = Run(World);
			if (Frame == 0)
			{
				RunState->SpendScore(RunState->GetRunState().Score.RunScore);	// broke
				Test.TestFalse(TEXT("投げ銭 needs 5 score"), Skills(World)->TryUseSkill(EMatsuriSkillSlot::E));
				Test.TestEqual(TEXT("a failed throw starts no cooldown"), Skills(World)->GetCooldownRemaining(EMatsuriSkillSlot::E), 0.0f);
				RunState->Collect(EMatsuriCollectible::Magatama);
				RunState->Collect(EMatsuriCollectible::Magatama);
				State->Targets.Add(SpawnNear<AMatsuriKasa>(World, FVector2D(380.0f, 0.0f)));
				return EStep::Continue;
			}
			if (Frame == 1)
			{
				AimAt(World, TargetLocation(World, *State, 0));	// the camera follows next frame
			}
			if (Frame == 3)
			{
				State->Count = RunState->GetRunState().Ammo;
				const int32 ScoreBeforeThrow = RunState->GetRunState().Score.RunScore;
				Test.TestTrue(TEXT("投げ銭 fires"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::E));
				Test.TestEqual(TEXT("costs 5 score"), RunState->GetRunState().Score.RunScore, ScoreBeforeThrow - Matsuri::Run::ZeniCost);
				State->Start(World);
			}
			if (Frame <= 3 || State->Since(World) < 0.6)
			{
				return EStep::Continue;
			}
			Test.TestFalse(TEXT("one heavy coin fells a kasa"), State->Targets[0].IsValid());
			// Ammo can only go up here (the kasa may drop ammo that the still-running magnet pulls in).
			Test.TestTrue(TEXT("no ammo used"), RunState->GetRunState().Ammo >= State->Count);
			return EStep::Done;
		} });
		Steps.Add({ TEXT("reset"), Reset(State) });
		Steps.Add({ TEXT("mio Q full house"), [&Test, State](UWorld* World, int32 Frame)
		{
			if (Frame == 0)
			{
				const FVector Spot = Hero(World)->GetActorLocation() + FVector(0.0f, 2500.0f, 0.0f);
				State->Targets.Add(World->SpawnActor<AMatsuriCollectible>(FVector(Spot.X, Spot.Y, 90.0f), FRotator::ZeroRotator));
				Run(World)->AddUltCharge(1.0f);
				Test.TestTrue(TEXT("千客万来 fires"), UseSkillWithFx(Test, World, EMatsuriSkillSlot::Q));
				Test.TestTrue(TEXT("double drops active"), FloorMode(World)->IsDoubleDropActive());
				State->Start(World);
				return EStep::Continue;
			}
			if (State->Targets[0].IsValid() && State->Since(World) < 1.4)	// magnet lasts 1.5 s
			{
				return EStep::Continue;
			}
			{
				Test.TestFalse(TEXT("far coin pulled in and collected"), State->Targets[0].IsValid());
				State->Count = CountCollectibles(World);
				AMatsuriChochin* Ghost = SpawnNear<AMatsuriChochin>(World, FVector2D(1200.0f, 0.0f));
				UGameplayStatics::ApplyDamage(Ghost, 5.0f, World->GetFirstPlayerController(), Hero(World), nullptr);
				Test.TestEqual(TEXT("yokai drop twice"), CountCollectibles(World), State->Count + 2);
			}
			return EStep::Done;
		} });
	}

	/** Queues one PIE session on 1F as Hero, running the given steps. */
	void QueueHeroSession(FAutomationTestBase* Test, EMatsuriHero HeroId, FSteps Steps)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([HeroId] { SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(HeroId)); }, 0.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(Test, MoveTemp(Steps)));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriSkillsE2ETest, "SuzuneMatsuri.E2E.Skills",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriSkillsE2ETest::RunTest(const FString&)
{
	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.God"), true);
	SetCVar(TEXT("matsuri.Maze"), false);	// open arena: skill geometry must not depend on a random maze

	const TSharedRef<FSkillState> State = MakeShared<FSkillState>();
	using FAdder = void (*)(FAutomationTestBase&, FSteps&, TSharedRef<FSkillState>);
	const TPair<EMatsuriHero, FAdder> Sessions[] = {
		{ EMatsuriHero::Sakura, &AddSakura }, { EMatsuriHero::Suzune, &AddSuzune }, { EMatsuriHero::Mio, &AddMio } };
	for (const TPair<EMatsuriHero, FAdder>& Session : Sessions)
	{
		FSteps Steps;
		AddFloorReady(Steps, State);
		Session.Value(*this, Steps, State);
		QueueHeroSession(this, Session.Key, MoveTemp(Steps));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.Threats"), true);
		SetCVar(TEXT("matsuri.God"), false);
		SetIntCVar(TEXT("matsuri.Hero"), 0);
		SetCVar(TEXT("matsuri.Maze"), true);
	}, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
