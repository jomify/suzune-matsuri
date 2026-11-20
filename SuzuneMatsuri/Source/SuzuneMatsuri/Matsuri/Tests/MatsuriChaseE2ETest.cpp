// Suzune's Night Festival - end-to-end test that yokai hunt the hero through a real maze floor.
// Each kind is spawned five corridor steps away (around walls, not in a straight line) and must close in.
// Run: Automation RunTests SuzuneMatsuri.E2E.Chase

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriYokaiKinds.h"

using namespace MatsuriTest;

namespace
{
	constexpr int32 SpawnSteps = 5;
	constexpr double ChaseSeconds = 25.0;

	struct FChaseState
	{
		TWeakObjectPtr<AMatsuriYokai> Yokai;
		double StartTime = 0.0;
		int32 BestSteps = MAX_int32;
		float BestDistance = TNumericLimits<float>::Max();	// straight line
	};

	const FMatsuriMaze* Maze(UWorld* World)
	{
		const AMatsuriFloorGameMode* Mode = World->GetAuthGameMode<AMatsuriFloorGameMode>();
		return Mode ? Mode->GetMaze() : nullptr;
	}

	int32 StepsBetween(const FMatsuriMaze& Maze, const FVector& A, const FVector& B)
	{
		return Maze.StepsTo(Maze.CellAt(FVector2D(B)))[Maze.Index(Maze.CellAt(FVector2D(A)))];
	}

	/** Spawns TYokai in a cell exactly SpawnSteps corridor steps from the hero, beside the doorway it would leave by (clear of crates). */
	template <typename TYokai>
	TYokai* SpawnAroundCorners(UWorld* World, const FMatsuriMaze& Maze)
	{
		const TArray<int32> Steps = Maze.StepsTo(Maze.CellAt(FVector2D(Hero(World)->GetActorLocation())));
		const int32 Cell = Steps.IndexOfByKey(SpawnSteps);
		if (Cell == INDEX_NONE)
		{
			return nullptr;
		}
		const FVector2D Center = Maze.CellCenter(Maze.CellOf(Cell));
		const FVector2D Door = Maze.NextWaypoint(Center, FVector2D(Hero(World)->GetActorLocation()));
		const FVector2D Spot = Center + (Door - Center).GetSafeNormal() * (Maze.CellSize() * 0.32f);
		TYokai* Yokai = World->SpawnActor<TYokai>(FVector(Spot, 100.0f), FRotator::ZeroRotator);
		Yokai->SetArenaHalfExtent(Maze.HalfExtent);
		return Yokai;
	}

	/**
	 *  Done when the yokai is MustReach corridor steps from the hero, or - when ReachDistance > 0 - within
	 *  that straight-line distance (the kappa stops at its throwing range, which on some random mazes is
	 *  reached three steps out: it then hides and throws, as designed).
	 */
	template <typename TYokai>
	FStepRunner::FStep ChaseStep(FAutomationTestBase& Test, TSharedRef<FChaseState> State, const TCHAR* Kind, int32 MustReach, float ReachDistance = 0.0f)
	{
		return [&Test, State, Kind, MustReach, ReachDistance](UWorld* World, int32 Frame)
		{
			const FMatsuriMaze* Grid = Maze(World);
			if (Frame == 0)
			{
				Test.TestNotNull(TEXT("floor is a maze"), Grid);
				for (TActorIterator<AMatsuriYokai> It(World); It; ++It) { It->Destroy(); }
				TeleportHero(World, World->GetAuthGameMode<AMatsuriFloorGameMode>()->GetCheckpoint());
				return EStep::Continue;
			}
			if (Frame == 2)
			{
				State->Yokai = Grid ? SpawnAroundCorners<TYokai>(World, *Grid) : nullptr;
				Test.TestNotNull(FString::Printf(TEXT("%s spawned %d steps away"), Kind, SpawnSteps), State->Yokai.Get());
				State->StartTime = World->GetTimeSeconds();
				State->BestSteps = MAX_int32;
				State->BestDistance = TNumericLimits<float>::Max();
				return State->Yokai.IsValid() ? EStep::Continue : EStep::Done;
			}
			// This test is about finding the way through the maze, not the chochin's stare game: look down.
			World->GetFirstPlayerController()->SetControlRotation(FRotator(-85.0f, 0.0f, 0.0f));
			if (Frame < 2 || !State->Yokai.IsValid())
			{
				return EStep::Continue;
			}
			const int32 Steps = StepsBetween(*Grid, State->Yokai->GetActorLocation(), Hero(World)->GetActorLocation());
			State->BestSteps = FMath::Min(State->BestSteps, Steps);
			State->BestDistance = FMath::Min(State->BestDistance, static_cast<float>(FVector::Dist2D(State->Yokai->GetActorLocation(), Hero(World)->GetActorLocation())));
			const bool bReached = State->BestSteps <= MustReach || (ReachDistance > 0.0f && State->BestDistance <= ReachDistance);
			if (!bReached && World->GetTimeSeconds() - State->StartTime < ChaseSeconds)
			{
				return EStep::Continue;
			}
			if (!bReached)
			{
				// Say what it is stuck on: every solid thing within a body's reach of it.
				const FVector At = State->Yokai->GetActorLocation();
				Test.AddInfo(FString::Printf(TEXT("%s stuck at %s, hero at %s"), Kind, *At.ToCompactString(), *Hero(World)->GetActorLocation().ToCompactString()));
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(It->GetRootComponent());
					if (*It != State->Yokai.Get() && Root && Root->GetCollisionEnabled() != ECollisionEnabled::NoCollision
						&& Root->Bounds.GetBox().ComputeSquaredDistanceToPoint(At) < FMath::Square(120.0f))
					{
						Test.AddInfo(FString::Printf(TEXT("  near: %s (%s) box %s hidden=%d"), *It->GetName(), *GetNameSafe(It->GetClass()),
							*Root->Bounds.GetBox().ToString(), It->IsHidden()));
					}
				}
			}
			Test.TestTrue(FString::Printf(TEXT("%s closed in through the maze: %d -> %d steps (needed <= %d%s), %.0f m away, in %.1f s"), Kind, SpawnSteps,
				State->BestSteps, MustReach, ReachDistance > 0.0f ? *FString::Printf(TEXT(" or within %.0f m"), ReachDistance / 100.0f) : TEXT(""),
				State->BestDistance / 100.0f, World->GetTimeSeconds() - State->StartTime), bReached);
			return EStep::Done;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriChaseE2ETest, "SuzuneMatsuri.E2E.Chase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriChaseE2ETest::RunTest(const FString&)
{
	TSharedRef<FChaseState> State = MakeShared<FChaseState>();
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
		return DoneIf(Frame > 5);
	} });
	Steps.Add({ TEXT("lantern ghost hunts"), ChaseStep<AMatsuriChochin>(*this, State, TEXT("chochin"), 0) });
	Steps.Add({ TEXT("umbrella hunts"), ChaseStep<AMatsuriKasa>(*this, State, TEXT("kasa"), 0) });
	Steps.Add({ TEXT("kappa stalks to throwing range"), ChaseStep<AMatsuriKappa>(*this, State, TEXT("kappa"), 2, AMatsuriKappa::ThrowRange) });

	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.God"), true);
	SetCVar(TEXT("matsuri.Maze"), true);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Mio));
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); SetCVar(TEXT("matsuri.God"), false); }, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
