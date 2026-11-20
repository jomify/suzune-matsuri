// Suzune's Night Festival - end-to-end boss fight in PIE on the real courtyard.
// The grapple lands her on a platform; fences stop bullets until they break; a bullet costs a life;
// Tamamo fights with her local learner and records every skill with its reward; each new health bar
// clears the screen; defeating her ends the run in victory and is written to her memory.
// Run: Automation RunTests SuzuneMatsuri.E2E.Boss

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriArenaPieces.h"
#include "MatsuriBoss.h"
#include "MatsuriBossBarWidget.h"
#include "MatsuriBossBrainComponent.h"
#include "MatsuriBossGameMode.h"
#include "MatsuriBossMemory.h"
#include "MatsuriCollectible.h"
#include "MatsuriDanmakuField.h"
#include "MatsuriFloors.h"
#include "MatsuriGrappleComponent.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriVictoryWidget.h"

using namespace MatsuriTest;

namespace
{
	struct FBossState
	{
		float Health = 0.0f;
		int32 CoverHits = 0;
		float Since = 0.0f;
		int32 Collectibles = 0;
		int32 MostBullets = 0;
	};

	AMatsuriBossGameMode* Mode(UWorld* World)
	{
		return World ? World->GetAuthGameMode<AMatsuriBossGameMode>() : nullptr;
	}

	FMatsuriBullet Slow(const FVector& At, const FVector& Velocity)
	{
		FMatsuriBullet Bullet;
		Bullet.Position = At;
		Bullet.Velocity = Velocity;
		Bullet.Radius = 16.0f;
		Bullet.Lifetime = 6.0f;
		Bullet.Damage = 12.0f;
		return Bullet;
	}

	FVector Feet(UWorld* World)
	{
		return Hero(World)->GetActorLocation() - FVector(0, 0, Hero(World)->GetSimpleCollisionHalfHeight());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossE2ETest, "SuzuneMatsuri.E2E.Boss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriBossE2ETest::RunTest(const FString&)
{
	TSharedRef<FBossState> State = MakeShared<FBossState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;

	Steps.Add({ TEXT("courtyard loads"), [this](UWorld* World, int32 Frame)
	{
		if (!Hero(World) || !Mode(World) || !Mode(World)->GetBoss() || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			Run(World)->UseRecordSlot(TestRecordSlot);
			int32 Hooks = 0, Fences = 0;
			for (TActorIterator<AMatsuriHookPoint> It(World); It; ++It) { ++Hooks; }
			for (TActorIterator<AMatsuriBreakableCover> It(World); It; ++It) { ++Fences; }
			TestEqual(TEXT("six grapple rings"), Hooks, 6);
			TestEqual(TEXT("four fences"), Fences, 4);
			TestNotNull(TEXT("the boss HUD is up"), Mode(World)->GetBossBar());
			TestNotNull(TEXT("the heroine has the grapple"), Hero(World)->FindComponentByClass<UMatsuriGrappleComponent>());
			TestFalse(TEXT("with threats off she waits"), Mode(World)->HasBossFightStarted());
		}
		return DoneIf(Frame > 10);
	} });

	Steps.Add({ TEXT("grapple onto the west platform"), [this](UWorld* World, int32 Frame)
	{
		UMatsuriGrappleComponent* Grapple = Hero(World)->FindComponentByClass<UMatsuriGrappleComponent>();
		const FMatsuriBossArena& Arena = Mode(World)->GetArena();
		if (Frame == 0)
		{
			TeleportHero(World, FVector(-400.0f, -300.0f, 110.0f));
			return EStep::Continue;
		}
		if (Frame < 6)
		{
			AimAt(World, Arena.HookPoints[0]);	// the ring over platform_w
			return EStep::Continue;
		}
		if (Frame == 6)
		{
			TestNotNull(TEXT("the ring under the crosshair is targeted"), Grapple->GetTarget());
			TestTrue(TEXT("F pulls"), Grapple->TryGrapple());
			return EStep::Continue;
		}
		if (Grapple->IsPulling() || Frame < 90)
		{
			return EStep::Continue;	// pulling, then landing
		}
		const int32 Zone = Matsuri::BossArena::ZoneAt(Arena, Feet(World));
		TestTrue(FString::Printf(TEXT("she lands on the platform (feet at z %.0f)"), Feet(World).Z), Arena.Zones.IsValidIndex(Zone) && Arena.Zones[Zone].Name == TEXT("platform_w"));
		return EStep::Done;
	} });

	Steps.Add({ TEXT("a fence stops bullets, then breaks"), [this, State](UWorld* World, int32 Frame)
	{
		AMatsuriDanmakuField* Field = Mode(World)->GetField();
		AMatsuriBreakableCover* Fence = nullptr;
		for (TActorIterator<AMatsuriBreakableCover> It(World); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), FVector(0, -1000, 0)) < 50.0f) { Fence = *It; }
		}
		if (Frame == 0)
		{
			State->CoverHits = Field->GetStats().CoverHits;
			TArray<FMatsuriBullet> Volley;
			for (int32 i = 0; i < AMatsuriBreakableCover::MaxHits + 5; ++i)
			{
				Volley.Add(Slow(FVector(-200.0f + i * 14.0f, -1500.0f, 90.0f), FVector(0, 900, 0)));	// north, into fence_s
			}
			Field->Spawn(Volley);
			return EStep::Continue;
		}
		if (Frame < 60)
		{
			return EStep::Continue;
		}
		TestTrue(TEXT("the fence took them"), Field->GetStats().CoverHits - State->CoverHits >= AMatsuriBreakableCover::MaxHits);
		TestTrue(TEXT("and broke"), Fence && !Fence->IsStanding());
		return EStep::Done;
	} });

	Steps.Add({ TEXT("a bullet deals HP damage"), [this, State](UWorld* World, int32 Frame)
	{
		AMatsuriDanmakuField* Field = Mode(World)->GetField();
		if (Frame == 0)
		{
			SetCVar(TEXT("matsuri.God"), false);
			TeleportHero(World, FVector(0.0f, -600.0f, 110.0f));
			State->Health = Run(World)->GetRunState().Health;
			return EStep::Continue;
		}
		if (Frame == 5)
		{
			FVector Chest;
			Field->GetPlayerHitSphere(Chest);
			TArray<FMatsuriBullet> One = { Slow(Chest + FVector(300, 0, 0), FVector(-600, 0, 0)) };
			Field->Spawn(One);
			return EStep::Continue;
		}
		if (Frame < 60)
		{
			return EStep::Continue;
		}
		TestTrue(TEXT("the bullet reached her"), Field->GetStats().Hits >= 1);
		TestEqual(TEXT("one bullet deals twelve HP"), Run(World)->GetRunState().Health, State->Health - 12.0f);
		SetCVar(TEXT("matsuri.God"), true);
		return EStep::Done;
	} });

	Steps.Add({ TEXT("she fights and learns"), [this, State](UWorld* World, int32 Frame)
	{
		AMatsuriBoss* Boss = Mode(World)->GetBoss();
		AMatsuriDanmakuField* Field = Mode(World)->GetField();
		if (Frame == 0)
		{
			TeleportHero(World, FVector(-900.0f, -820.0f, 110.0f));	// behind the south-west lantern
			Mode(World)->StartBossFight();
			State->Since = World->GetTimeSeconds();
			return EStep::Continue;
		}
		AimAt(World, Boss->GetActorLocation());
		State->MostBullets = FMath::Max(State->MostBullets, Field->Num());
		if (Frame == 360)
		{
			TeleportHero(World, FVector(0.0f, -1400.0f, 110.0f));	// out in the open for the picture
		}
		if (Frame == 420 && FApp::CanEverRender())
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Automation/MatsuriShots");
			FScreenshotRequest::RequestScreenshot(Dir / TEXT("80_boss_fight.png"), true, false);
		}
		const UMatsuriBossBrainComponent* Brain = Boss->GetBrain();
		if (Brain->GetThisFight().Num() < 3 && World->GetTimeSeconds() - State->Since < 30.0)
		{
			return EStep::Continue;
		}
		TestTrue(TEXT("three skills were scored"), Brain->GetThisFight().Num() >= 3);
		TestEqual(TEXT("with the model off, nothing came from it"), Brain->GetDecisions(EMatsuriBossSource::Model), 0);
		TestTrue(TEXT("the learner chose them"), Brain->GetDecisions(EMatsuriBossSource::Learner) >= 3);
		TestTrue(FString::Printf(TEXT("bullets flew (most at once: %d)"), State->MostBullets), State->MostBullets > 20);
		TestTrue(TEXT("the bullet budget held"), State->MostBullets <= AMatsuriDanmakuField::MaxBullets);
		bool bFinite = true;
		for (const FMatsuriBossStep& Step : Brain->GetThisFight())
		{
			bFinite &= FMath::IsFinite(Step.Reward) && Step.Observation.Phase == 1;
		}
		TestTrue(TEXT("every step has a reward"), bFinite);
		TestTrue(TEXT("her memory is on disk, in the test folder"), FPaths::FileExists(TestBossMemoryFolder() / TEXT("steps.jsonl")));
		TestFalse(TEXT("the HUD names her skill"), Mode(World)->GetBossBar()->GetSkillText().IsEmpty());
		return EStep::Done;
	} });

	Steps.Add({ TEXT("each new bar clears the screen"), [this](UWorld* World, int32 Frame)
	{
		AMatsuriBoss* Boss = Mode(World)->GetBoss();
		AMatsuriDanmakuField* Field = Mode(World)->GetField();
		if (Boss->GetPhase() >= 2)
		{
			TestTrue(TEXT("phase 2 begins below two thirds"), Boss->GetHealthFraction() <= AMatsuriBoss::PhaseThreshold(1) + KINDA_SMALL_NUMBER);
			TestEqual(TEXT("her bullets are gone"), Field->Num(), 0);
			const float Before = Boss->GetHealth();
			UGameplayStatics::ApplyDamage(Boss, 5.0f, World->GetFirstPlayerController(), Hero(World), nullptr);
			TestEqual(TEXT("and she cannot be hurt for a moment"), Boss->GetHealth(), Before);
			return EStep::Done;
		}
		UGameplayStatics::ApplyDamage(Boss, 3.0f, World->GetFirstPlayerController(), Hero(World), nullptr);
		return EStep::Continue;
	} });

	Steps.Add({ TEXT("defeat her"), [this, State](UWorld* World, int32 Frame)
	{
		AMatsuriBoss* Boss = Mode(World)->GetBoss();
		if (Frame == 0)
		{
			State->Collectibles = CountCollectibles(World);
		}
		if (!Boss->IsDefeated())
		{
			UGameplayStatics::ApplyDamage(Boss, 3.0f, World->GetFirstPlayerController(), Hero(World), nullptr);
			return EStep::Continue;
		}
		TestTrue(TEXT("she scatters into magatama"), CountCollectibles(World) >= State->Collectibles + AMatsuriBoss::MagatamaDrops);
		return EStep::Done;
	} });

	Steps.Add({ TEXT("final victory and her record of it"), [this](UWorld* World, int32 Frame)
	{
		UMatsuriVictoryWidget* Victory = FindWidget<UMatsuriVictoryWidget>(World);
		if (!Victory)
		{
			return EStep::Wait;
		}
		TestEqual(TEXT("the run ends in victory"), Run(World)->GetLastRunEnd(), EMatsuriRunEnd::Victory);
		FMatsuriBossStore Memory(TestBossMemoryFolder());
		Memory.Load();
		TestTrue(TEXT("the fight is in her memory, as a loss"), Memory.GetFights().Num() >= 1 && !Memory.GetFights().Last().bBossWon);
		return EStep::Done;
	} });

	UseTestBossBrain();
	FMatsuriBossStore(TestBossMemoryFolder()).Wipe();
	SetCVar(TEXT("matsuri.Threats"), false);	// she waits until the test starts her
	SetCVar(TEXT("matsuri.God"), true);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Sakura));
	SetStringCVar(TEXT("matsuri.BossHealth"), TEXT("60"));
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(Matsuri::Floors::BossFloor)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.Threats"), true);
		SetCVar(TEXT("matsuri.God"), false);
		SetStringCVar(TEXT("matsuri.BossHealth"), TEXT("150"));
		RestoreBossBrain();
	}, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
