// Real PIE regressions for bullet damage acceptance, sweeping and fatal cleanup.

#include "MatsuriTestKit.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "GameFramework/CharacterMovementComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriBossGameMode.h"
#include "MatsuriBoss.h"
#include "MatsuriBossBrainComponent.h"
#include "MatsuriBossSkills.h"
#include "MatsuriDanmakuField.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriGameOverWidget.h"
#include "MatsuriSkillComponent.h"

using namespace MatsuriTest;

namespace
{
	struct FDamageState
	{
		float Health = 0.0f;
		int32 Hits = 0;
		int32 Touches = 0;
		int32 Skill = 0;
		FVector BlinkStart = FVector::ZeroVector;
		FVector BlinkDirection = FVector::ForwardVector;
	};

	AMatsuriBossGameMode* DamageMode(UWorld* World)
	{
		return World ? World->GetAuthGameMode<AMatsuriBossGameMode>() : nullptr;
	}

	FMatsuriBullet TestBullet(const FVector& Position, const FVector& Velocity = FVector::ZeroVector)
	{
		FMatsuriBullet Bullet;
		Bullet.Position = Position;
		Bullet.Velocity = Velocity;
		Bullet.Radius = 10.0f;
		Bullet.Lifetime = 5.0f;
		return Bullet;
	}

	FVector Chest(UWorld* World)
	{
		FVector Result;
		DamageMode(World)->GetField()->GetPlayerHitSphere(Result);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossDamageE2ETest, "SuzuneMatsuri.E2E.BossDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriBossDamageE2ETest::RunTest(const FString&)
{
	TSharedRef<FDamageState> State = MakeShared<FDamageState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("damage fixture loads"), [this](UWorld* World, int32 Frame)
	{
		AMatsuriBossGameMode* Mode = DamageMode(World);
		if (!Hero(World) || !Mode || !Mode->GetField() || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			Run(World)->UseRecordSlot(TestRecordSlot);
			TeleportHero(World, FVector(0, -600, 110));
			Hero(World)->GetCharacterMovement()->DisableMovement();
			Mode->GetField()->SetActorTickEnabled(false); // advance known intervals, not renderer-dependent frames
			Mode->GetField()->SetArena(FMatsuriBossArena());
			TestFalse(TEXT("the model and boss combat are idle"), Mode->HasBossFightStarted());
		}
		return DoneIf(Frame >= 3);
	} });
	Steps.Add({ TEXT("fast bullet crossing costs twelve HP"), [this](UWorld* World, int32)
	{
		AMatsuriDanmakuField* Field = DamageMode(World)->GetField();
		Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth());
		const float Health = Run(World)->GetRunState().Health;
		const int32 Hits = Field->GetStats().Hits;
		TArray<FMatsuriBullet> Volley = { TestBullet(Chest(World) - FVector(37.375f, 0, 0), FVector(1495, 0, 0)) };
		Volley[0].Damage = 12.0f;
		Field->Spawn(Volley);
		Field->Tick(0.05f);
		TestEqual(TEXT("a swept hit deals the boss bullet damage"), Run(World)->GetRunState().Health, Health - 12.0f);
		TestEqual(TEXT("only accepted damage is a hit"), Field->GetStats().Hits, Hits + 1);
		TestEqual(TEXT("the damaging bullet is consumed"), Field->Num(), 0);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("same skill keeps per-bullet damage"), [this, State](UWorld* World, int32 Frame)
	{
		AMatsuriBossGameMode* Mode = DamageMode(World);
		if (Mode->IsPlayerInvulnerable()) { return Frame == 0 ? EStep::Wait : EStep::Continue; }
		AMatsuriDanmakuField* Field = Mode->GetField();
		if (Frame == 0)
		{
			Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth());
			State->Health = Run(World)->GetRunState().Health;
			FMatsuriBullet Light = TestBullet(Chest(World));
			Light.Skill = static_cast<uint8>(EMatsuriBossSkill::Ring);
			Light.Damage = 12.0f;
			TArray<FMatsuriBullet> Volley = { Light };
			Field->Spawn(Volley);
			Field->Tick(1.0f / 60.0f);
			TestEqual(TEXT("same skill light bullet deals twelve"), Run(World)->GetRunState().Health, State->Health - 12.0f);
			return EStep::Continue;
		}
		if (Mode->IsPlayerInvulnerable()) { return EStep::Continue; }
		Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth());
		const float Before = Run(World)->GetRunState().Health;
		FMatsuriBullet Heavy = TestBullet(Chest(World));
		Heavy.Skill = static_cast<uint8>(EMatsuriBossSkill::Ring);
		Heavy.Damage = 30.0f;
		TArray<FMatsuriBullet> Volley = { Heavy };
		Field->Spawn(Volley);
		Field->Tick(1.0f / 60.0f);
		TestEqual(TEXT("same skill heavy bullet deals thirty"), Run(World)->GetRunState().Health, Before - 30.0f);
		Field->ClearAll();
		return EStep::Done;
	} });
	Steps.Add({ TEXT("all six skills share accepted damage rules"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0) { State->Skill = 0; }
		if (DamageMode(World)->IsPlayerInvulnerable()) { return EStep::Continue; }
		AMatsuriDanmakuField* Field = DamageMode(World)->GetField();
		Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth());
		const float Health = Run(World)->GetRunState().Health;
		FMatsuriBullet Bullet = TestBullet(Chest(World));
		Bullet.Skill = static_cast<uint8>(State->Skill);
		const EMatsuriBossSkill Skill = static_cast<EMatsuriBossSkill>(State->Skill);
		const float ExpectedDamage = Matsuri::BossSkills::BulletDamageFor(Matsuri::BossSkills::Preset(Skill, 0));
		Bullet.Damage = ExpectedDamage;
		Bullet.Style = Skill == EMatsuriBossSkill::Pursuit || Skill == EMatsuriBossSkill::Phantom ? EMatsuriBulletStyle::Rice
			: Skill == EMatsuriBossSkill::TailSweep ? EMatsuriBulletStyle::Large : EMatsuriBulletStyle::Orb;
		TArray<FMatsuriBullet> Volley = { Bullet };
		Field->Spawn(Volley);
		Field->Tick(1.0f / 60.0f);
		TestEqual(FString::Printf(TEXT("%s carries its own damage"), Matsuri::BossSkills::Id(Skill)), Run(World)->GetRunState().Health, Health - ExpectedDamage);
		++State->Skill;
		return DoneIf(State->Skill == Matsuri::BossSkills::Count);
	} });
	Steps.Add({ TEXT("invalid bullet damage is ignored"), [this](UWorld* World, int32 Frame)
	{
		AMatsuriBossGameMode* Mode = DamageMode(World);
		if (Mode->IsPlayerInvulnerable()) { return EStep::Wait; }
		AMatsuriDanmakuField* Field = Mode->GetField();
		if (Frame == 0) { Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth()); }
		const float Before = Run(World)->GetRunState().Health;
		FMatsuriBullet Bullet = TestBullet(Chest(World));
		Bullet.Damage = Frame == 0 ? -1.0f : Frame == 1 ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
		TArray<FMatsuriBullet> Volley = { Bullet };
		const int32 Hits = Field->GetStats().Hits;
		Field->Spawn(Volley);
		Field->Tick(1.0f / 60.0f);
		TestEqual(TEXT("invalid damage does not change HP"), Run(World)->GetRunState().Health, Before);
		TestEqual(TEXT("invalid damage does not count as a hit"), Field->GetStats().Hits, Hits);
		Field->ClearAll();
		return DoneIf(Frame >= 2);
	} });
	Steps.Add({ TEXT("replay drawing leaves live collision state intact"), [this](UWorld* World, int32)
	{
		AMatsuriDanmakuField* Field = DamageMode(World)->GetField();
		const FVector LivePosition = Chest(World) + FVector(500, 0, 0);
		TArray<FMatsuriBullet> Volley = { TestBullet(LivePosition) };
		Field->Spawn(Volley);
		const int32 Hits = Field->GetStats().Hits;
		TArray<FMatsuriBullet> Recorded = { TestBullet(LivePosition - FVector(100, 0, 0)), TestBullet(LivePosition - FVector(200, 0, 0)) };
		Field->ShowReplayBullets(Recorded);
		TestEqual(TEXT("captured drawing does not replace live bullets"), Field->Num(), 1);
		TestTrue(TEXT("the live projectile stays at its live position"), Field->GetBullets()[0].Position.Equals(LivePosition));
		TestEqual(TEXT("visual playback cannot register a hit"), Field->GetStats().Hits, Hits);
		Field->ClearAll();
		return EStep::Done;
	} });
	Steps.Add({ TEXT("telegraph has no damage until armed"), [this](UWorld* World, int32)
	{
		if (DamageMode(World)->IsPlayerInvulnerable()) { return EStep::Wait; }
		AMatsuriDanmakuField* Field = DamageMode(World)->GetField();
		Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth());
		const float Health = Run(World)->GetRunState().Health;
		FMatsuriBullet Bullet = TestBullet(Chest(World));
		Bullet.Delay = 0.2f;
		TArray<FMatsuriBullet> Volley = { Bullet };
		Field->Spawn(Volley);
		Field->Tick(0.1f);
		TestEqual(TEXT("a warning cannot hurt"), Run(World)->GetRunState().Health, Health);
		Field->Tick(0.11f);
		TestEqual(TEXT("the armed bullet can hurt"), Run(World)->GetRunState().Health, Health - 10.0f);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("immune contact can hurt when immunity ends"), [this, State](UWorld* World, int32 Frame)
	{
		AMatsuriBossGameMode* Mode = DamageMode(World);
		AMatsuriDanmakuField* Field = Mode->GetField();
		if (Frame == 0)
		{
			if (Mode->IsPlayerInvulnerable()) { return EStep::Wait; }
			State->Health = Run(World)->GetRunState().Health;
			State->Hits = Field->GetStats().Hits;
			State->Touches = Field->GetStats().Touches;
			Mode->GrantInvulnerability(0.1f);
			TArray<FMatsuriBullet> Volley = { TestBullet(Chest(World)) };
			Field->Spawn(Volley);
			Field->Tick(0.01f);
			TestEqual(TEXT("immune contact preserves HP"), Run(World)->GetRunState().Health, State->Health);
			TestEqual(TEXT("immune contact is not a hit"), Field->GetStats().Hits, State->Hits);
			TestEqual(TEXT("immune reach is counted once"), Field->GetStats().Touches, State->Touches + 1);
			return EStep::Continue;
		}
		if (Mode->IsPlayerInvulnerable()) { return EStep::Continue; }
		Field->Tick(0.01f);
		TestEqual(TEXT("the same live bullet can hurt after immunity"), Run(World)->GetRunState().Health, State->Health - 10.0f);
		TestEqual(TEXT("the later accepted hit is counted"), Field->GetStats().Hits, State->Hits + 1);
		TestEqual(TEXT("touch accounting is not repeated"), Field->GetStats().Touches, State->Touches + 1);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("same-frame bullets respect actual damage acceptance"), [this](UWorld* World, int32)
	{
		if (DamageMode(World)->IsPlayerInvulnerable()) { return EStep::Wait; }
		AMatsuriDanmakuField* Field = DamageMode(World)->GetField();
		Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth());
		const float Health = Run(World)->GetRunState().Health;
		const int32 Hits = Field->GetStats().Hits;
		TArray<FMatsuriBullet> Volley = { TestBullet(Chest(World)), TestBullet(Chest(World)) };
		Field->Spawn(Volley);
		Field->Tick(0.01f);
		TestEqual(TEXT("the first hit starts the grace window"), Run(World)->GetRunState().Health, Health - 10.0f);
		TestEqual(TEXT("the rejected second hit is not counted"), Field->GetStats().Hits, Hits + 1);
		TestEqual(TEXT("only the accepted projectile is consumed"), Field->Num(), 1);
		Field->ClearAll();
		return EStep::Done;
	} });
	Steps.Add({ TEXT("thin cover wins only before the player"), [this](UWorld* World, int32)
	{
		if (DamageMode(World)->IsPlayerInvulnerable()) { return EStep::Wait; }
		AMatsuriDanmakuField* Field = DamageMode(World)->GetField();
		FMatsuriBossArena Arena;
		FMatsuriArenaBox Cover;
		Cover.Center = Chest(World) - FVector(70, 0, 0);
		Cover.Extent = FVector(10, 50, 50);
		Arena.Solids.Add(Cover);
		Field->SetArena(Arena);
		Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth());
		const float Health = Run(World)->GetRunState().Health;
		const int32 CoverHits = Field->GetStats().CoverHits;
		TArray<FMatsuriBullet> Volley = { TestBullet(Chest(World) - FVector(120, 0, 0), FVector(2400, 0, 0)) };
		Field->Spawn(Volley);
		Field->Tick(0.1f);
		TestEqual(TEXT("the wall prevents a tunneled hit"), Run(World)->GetRunState().Health, Health);
		TestEqual(TEXT("the thin wall absorbs the bullet"), Field->GetStats().CoverHits, CoverHits + 1);
		Arena.Solids[0].Center = Chest(World) + FVector(70, 0, 0);
		Field->SetArena(Arena);
		Volley = { TestBullet(Chest(World) - FVector(120, 0, 0), FVector(2400, 0, 0)) };
		Field->Spawn(Volley);
		Field->Tick(0.1f);
		TestEqual(TEXT("a wall behind cannot erase an earlier player hit"), Run(World)->GetRunState().Health, Health - 10.0f);
		Field->SetArena(FMatsuriBossArena());
		return EStep::Done;
	} });
	Steps.Add({ TEXT("player motion is included in sweeping"), [this](UWorld* World, int32)
	{
		if (DamageMode(World)->IsPlayerInvulnerable()) { return EStep::Wait; }
		AMatsuriDanmakuField* Field = DamageMode(World)->GetField();
		Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth());
		const float Health = Run(World)->GetRunState().Health;
		const FVector Start = Hero(World)->GetActorLocation();
		Hero(World)->SetActorLocation(Start - FVector(40, 0, 0), false, nullptr, ETeleportType::None);
		Field->Tick(0.0f);
		TArray<FMatsuriBullet> Volley = { TestBullet(Chest(World) + FVector(40, 0, 0)) };
		Field->Spawn(Volley);
		Hero(World)->SetActorLocation(Start + FVector(40, 0, 0), false, nullptr, ETeleportType::None);
		Field->Tick(1.0f / 60.0f);
		TestEqual(TEXT("crossing a stationary bullet cannot tunnel"), Run(World)->GetRunState().Health, Health - 10.0f);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("Suzune blink skips intermediate bullets but not its landing"), [this, State](UWorld* World, int32 Frame)
	{
		AMatsuriBossGameMode* Mode = DamageMode(World);
		AMatsuriDanmakuField* Field = Mode->GetField();
		if (Frame == 0)
		{
			if (Mode->IsPlayerInvulnerable()) { return EStep::Wait; }
			State->BlinkStart = Hero(World)->GetActorLocation();
			State->BlinkDirection = ClearDirection(World, 700.0f);
			World->GetFirstPlayerController()->SetControlRotation(State->BlinkDirection.Rotation());
			return EStep::Continue;
		}
		if (Frame < 5) { return EStep::Continue; }
		Run(World)->HealPlayer(Run(World)->GetRunState().GetMaxHealth());
		const float Health = Run(World)->GetRunState().Health;
		const int32 Hits = Field->GetStats().Hits;
		Field->Tick(0.0f);
		TArray<FMatsuriBullet> Volley = { TestBullet(Chest(World) + State->BlinkDirection * 300.0f) };
		Field->Spawn(Volley);
		UMatsuriSkillComponent* Skills = Hero(World)->FindComponentByClass<UMatsuriSkillComponent>();
		TestTrue(TEXT("the actual Suzune Shift skill executes"), Skills && Skills->TryUseSkill(EMatsuriSkillSlot::Shift));
		TestTrue(TEXT("blink crossed the intermediate projectile"), FVector::Dist2D(State->BlinkStart, Hero(World)->GetActorLocation()) > 400.0f);
		Field->Tick(1.0f / 60.0f);
		TestEqual(TEXT("skipped space causes no phantom damage"), Run(World)->GetRunState().Health, Health);
		TestEqual(TEXT("blink is not counted as a swept hit"), Field->GetStats().Hits, Hits);
		Volley = { TestBullet(Chest(World)) };
		Field->Spawn(Volley);
		Field->Tick(1.0f / 60.0f);
		TestEqual(TEXT("a real bullet at the blink landing still hurts"), Run(World)->GetRunState().Health, Health - 10.0f);
		Field->ClearAll();
		return EStep::Done;
	} });
	Steps.Add({ TEXT("fatal bullet ends combat without invalidating traversal"), [this](UWorld* World, int32)
	{
		AMatsuriBossGameMode* Mode = DamageMode(World);
		if (Mode->IsPlayerInvulnerable()) { return EStep::Wait; }
		AMatsuriDanmakuField* Field = Mode->GetField();
		SetCVar(TEXT("matsuri.Replay"), false); // exercise the synchronous fallback settlement
		Mode->StartBossFight();
		AMatsuriBoss* Boss = Mode->GetBoss();
		UMatsuriBossBrainComponent* Brain = Boss->FindComponentByClass<UMatsuriBossBrainComponent>();
		Brain->StepStarted(Matsuri::BossSkills::Preset(EMatsuriBossSkill::Ring, 0), FMatsuriBossObservation(), Field->GetStats(), Boss->GetHealthFraction(), 0.0f);
		Run(World)->HurtPlayer(Run(World)->GetRunState().Health - 4.0f);
		const int32 Hits = Field->GetStats().Hits;
		TArray<FMatsuriBullet> Volley = { TestBullet(Chest(World)), TestBullet(Chest(World)) };
		Field->Spawn(Volley);
		Field->Tick(1.0f / 60.0f);
		TestEqual(TEXT("the fatal hit clamps HP to zero"), Run(World)->GetRunState().Health, 0.0f);
		TestEqual(TEXT("only the fatal accepted hit counts"), Field->GetStats().Hits, Hits + 1);
		TestFalse(TEXT("combat has stopped before replay or cleanup"), Mode->IsFloorActive());
		TestNotNull(TEXT("disabled replay settles directly to game over"), FindWidget<UMatsuriGameOverWidget>(World));
		const TArray<FMatsuriBossStep>& Fight = Brain->GetThisFight();
		TestTrue(TEXT("the final skill was scored"), Fight.Num() > 0);
		if (Fight.Num() > 0)
		{
			TestEqual(TEXT("the fatal accepted hit is included before synchronous scoring"), Fight.Last().Outcome.Hits, 1);
		}
		return EStep::Done;
	} });

	UseTestBossBrain();
	SetCVar(TEXT("matsuri.Story"), false);
	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.God"), false);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Suzune));
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(Matsuri::Floors::BossFloor)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.Story"), true);
		SetCVar(TEXT("matsuri.Threats"), true);
		SetCVar(TEXT("matsuri.God"), false);
		SetCVar(TEXT("matsuri.Replay"), true);
		RestoreBossBrain();
	}, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
