// Suzune's Night Festival - yokai fight by their nature (PIE, open arena with placed walls).
// Playtest: "the monsters are not very smart; melee ones should ambush using their traits, ranged ones
// should use cover". The chochin freezes when watched and closes in when not; the kappa ducks behind a
// block between throws; the kasa waits behind a wall and pounces when the player steps into view.
// Run: Automation RunTests SuzuneMatsuri.E2E.YokaiTactics

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Engine/StaticMeshActor.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriFloors.h"
#include "MatsuriGraybox.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriProjectile.h"
#include "MatsuriYokaiKinds.h"

using namespace MatsuriTest;

namespace
{
	struct FTacticsState
	{
		TWeakObjectPtr<AMatsuriYokai> Yokai;
		FVector Dir = FVector::ForwardVector;
		double Since = 0.0;
		float StartDistance = 0.0f;
		float PhaseDistance = 0.0f;
		FVector PhaseLocation = FVector::ZeroVector;
		int32 Samples = 0;
		int32 HiddenSamples = 0;
		int32 Throws = 0;
		int32 Phase = 0;
	};

	template <typename TYokai>
	TYokai* SpawnAt(UWorld* World, const FVector& Location)
	{
		TYokai* Yokai = World->SpawnActor<TYokai>(Location, FRotator::ZeroRotator);
		Yokai->SetArenaHalfExtent(Matsuri::Floors::SpecFor(0, Run(World)->GetRunState().Seed).HalfExtent);
		return Yokai;
	}

	/** A solid grey block (a wall or a crate) placed for the test. */
	AStaticMeshActor* PlaceBlock(UWorld* World, const FVector& Center, const FVector& Size, const FRotator& Rotation)
	{
		AStaticMeshActor* Block = World->SpawnActor<AStaticMeshActor>(Center, Rotation);
		UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cube));
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Block->SetActorScale3D(Size / 100.0f);
		return Block;
	}

	bool PlayerSees(UWorld* World, const AActor& Target)
	{
		FVector Eye;
		FRotator Unused;
		Hero(World)->GetActorEyesViewPoint(Eye, Unused);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Hero(World));
		Params.AddIgnoredActor(&Target);
		return !World->LineTraceTestByChannel(Eye, Target.GetActorLocation(), ECC_Visibility, Params);
	}

	void ClearYokai(UWorld* World)
	{
		for (TActorIterator<AMatsuriYokai> It(World); It; ++It) { It->Destroy(); }
		for (TActorIterator<AMatsuriProjectile> It(World); It; ++It) { It->Destroy(); }
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriYokaiTacticsE2ETest, "SuzuneMatsuri.E2E.YokaiTactics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriYokaiTacticsE2ETest::RunTest(const FString&)
{
	TSharedRef<FTacticsState> State = MakeShared<FTacticsState>();
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
		}
		return DoneIf(Frame > 10 && World->GetTimeSeconds() > 1.0);
	} });

	// The chochin: watched, it barely creeps; unwatched, it closes in fast.
	Steps.Add({ TEXT("chochin creeps when watched, stalks when not"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			ClearYokai(World);
			State->Dir = ClearDirection(World, 1300.0f);
			State->Yokai = SpawnAt<AMatsuriChochin>(World, Hero(World)->GetActorLocation() + State->Dir * 1100.0f);
			State->Since = World->GetTimeSeconds();
			State->Phase = 0;
			return EStep::Continue;
		}
		AMatsuriYokai* Chochin = State->Yokai.Get();
		if (!Chochin)
		{
			AddError(TEXT("the chochin vanished"));
			return EStep::Done;
		}
		const float Distance = FVector::Dist2D(Chochin->GetActorLocation(), Hero(World)->GetActorLocation());
		const double Elapsed = World->GetTimeSeconds() - State->Since;
		if (State->Phase == 0)
		{
			AimAt(World, Chochin->GetActorLocation());	// stare it down
			if (Elapsed < 0.3)
			{
				State->StartDistance = Distance;
				return EStep::Continue;
			}
			if (Elapsed < 2.3)
			{
				return EStep::Continue;
			}
			TestTrue(FString::Printf(TEXT("watched for 2 s, it only crept (%.0f cm closer)"), State->StartDistance - Distance), State->StartDistance - Distance <= 160.0f);
			State->Phase = 1;
			State->PhaseDistance = Distance;
			State->Since = World->GetTimeSeconds();
			return EStep::Continue;
		}
		// Look the other way.
		World->GetFirstPlayerController()->SetControlRotation((-State->Dir).Rotation());
		if (Elapsed < 1.5 && Distance > 200.0f)
		{
			return EStep::Continue;
		}
		TestTrue(FString::Printf(TEXT("unwatched, it closed in fast (%.0f cm in %.1f s)"), State->PhaseDistance - Distance, Elapsed), State->PhaseDistance - Distance >= 450.0f);
		return EStep::Done;
	} });

	// The kappa: throws, then ducks behind the block.
	Steps.Add({ TEXT("kappa fights from cover"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			ClearYokai(World);
			State->Dir = ClearDirection(World, 1300.0f);
			const FVector Side = FVector::CrossProduct(FVector::UpVector, State->Dir);
			const FVector From = Hero(World)->GetActorLocation() * FVector(1, 1, 0);
			PlaceBlock(World, From + State->Dir * 760.0f + Side * 260.0f + FVector(0, 0, 100.0f), FVector(140.0f, 140.0f, 200.0f), State->Dir.Rotation());
			State->Yokai = SpawnAt<AMatsuriKappa>(World, From + State->Dir * 920.0f + FVector(0, 0, 60.0f));
			State->Since = World->GetTimeSeconds();
			State->Samples = State->HiddenSamples = State->Throws = 0;
			return EStep::Continue;
		}
		AMatsuriYokai* Kappa = State->Yokai.Get();
		if (!Kappa)
		{
			AddError(TEXT("the kappa vanished"));
			return EStep::Done;
		}
		AimAt(World, Kappa->GetActorLocation());
		for (TActorIterator<AMatsuriProjectile> It(World); It; ++It)
		{
			if (It->IsHostileToPlayer() && !It->Tags.Contains(TEXT("Counted")))
			{
				It->Tags.Add(TEXT("Counted"));
				++State->Throws;
			}
		}
		const double Elapsed = World->GetTimeSeconds() - State->Since;
		if (Elapsed > 2.5)	// after its first throw (cooldown 1.5 s) and the dash for cover
		{
			++State->Samples;
			State->HiddenSamples += PlayerSees(World, *Kappa) ? 0 : 1;
		}
		if (Elapsed < 6.0)
		{
			return EStep::Continue;
		}
		TestTrue(FString::Printf(TEXT("it threw (%d cucumbers)"), State->Throws), State->Throws >= 1);
		const float Hidden = State->Samples > 0 ? static_cast<float>(State->HiddenSamples) / State->Samples : 0.0f;
		TestTrue(FString::Printf(TEXT("it spent most of the fight behind cover (%.0f %% hidden)"), Hidden * 100.0f), Hidden >= 0.5f);
		return EStep::Done;
	} });

	// The kasa: waits behind a wall, pounces when the player steps into view.
	Steps.Add({ TEXT("kasa lurks behind a wall, then pounces"), [this, State](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			ClearYokai(World);
			State->Dir = ClearDirection(World, 1300.0f);
			const FVector From = Hero(World)->GetActorLocation() * FVector(1, 1, 0);
			PlaceBlock(World, From + State->Dir * 520.0f + FVector(0, 0, 150.0f), FVector(40.0f, 420.0f, 300.0f), State->Dir.Rotation());
			State->Yokai = SpawnAt<AMatsuriKasa>(World, From + State->Dir * 760.0f + FVector(0, 0, 60.0f));
			State->Since = World->GetTimeSeconds();
			State->Phase = 0;
			return EStep::Continue;
		}
		AMatsuriYokai* Kasa = State->Yokai.Get();
		if (!Kasa)
		{
			AddError(TEXT("the kasa vanished"));
			return EStep::Done;
		}
		const double Elapsed = World->GetTimeSeconds() - State->Since;
		if (State->Phase == 0)
		{
			if (Elapsed < 0.4)
			{
				State->PhaseLocation = Kasa->GetActorLocation();
				return EStep::Continue;
			}
			if (Elapsed < 2.0)
			{
				return EStep::Continue;
			}
			const float Moved = FVector::Dist2D(Kasa->GetActorLocation(), State->PhaseLocation);
			TestTrue(FString::Printf(TEXT("out of sight behind the wall, it lurked (moved %.0f cm)"), Moved), Moved <= 60.0f);
			// Come round into view and within reach: beyond it along the line already checked clear
			// (a spot off to the side could be inside one of the arena's random crates).
			TeleportHero(World, Kasa->GetActorLocation() * FVector(1, 1, 0) + State->Dir * 380.0f + FVector(0, 0, 100.0f));
			State->Phase = 1;
			State->Since = World->GetTimeSeconds();
			return EStep::Continue;
		}
		const float Distance = FVector::Dist2D(Kasa->GetActorLocation(), Hero(World)->GetActorLocation());
		if (Elapsed < 1.2 && Distance > 160.0f)
		{
			return EStep::Continue;
		}
		TestTrue(FString::Printf(TEXT("in view and in reach, it pounced (%.0f cm away after %.2f s)"), Distance, Elapsed), Distance <= 160.0f);
		return EStep::Done;
	} });

	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.God"), true);
	SetCVar(TEXT("matsuri.Maze"), false);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Mio));
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.Threats"), true); SetCVar(TEXT("matsuri.God"), false); SetCVar(TEXT("matsuri.Maze"), true); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
