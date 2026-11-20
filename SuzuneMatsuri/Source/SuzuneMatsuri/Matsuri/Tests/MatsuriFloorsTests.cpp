// Suzune's Night Festival - floor table and spawn placement unit tests.
// Run: Automation RunTests SuzuneMatsuri.Floors

#include "Misc/AutomationTest.h"
#include "MatsuriFloors.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags MatsuriTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int32 RunSeedsToSweep = 15;

	bool IsThreat(EMatsuriSpawn Type)
	{
		return Type != EMatsuriSpawn::Ammo;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriFloorsEscalateTest, "SuzuneMatsuri.Floors.DifficultyEscalates", MatsuriTestFlags)
bool FMatsuriFloorsEscalateTest::RunTest(const FString&)
{
	for (int32 Floor = 1; Floor < Matsuri::Floors::Count; ++Floor)
	{
		const FMatsuriFloorSpec Prev = Matsuri::Floors::SpecFor(Floor - 1, 7);
		const FMatsuriFloorSpec Next = Matsuri::Floors::SpecFor(Floor, 7);
		TestTrue(TEXT("target rises"), Next.TargetScore > Prev.TargetScore);
		TestTrue(TEXT("time shrinks"), Next.TimeLimitSeconds < Prev.TimeLimitSeconds);
		TestTrue(TEXT("more enemies"), Next.ChochinCount + Next.KasaCount + Next.KappaCount > Prev.ChochinCount + Prev.KasaCount + Prev.KappaCount);
		TestNotEqual(TEXT("own map"), FString(Matsuri::Floors::MapFor(Floor)), FString(Matsuri::Floors::MapFor(Floor - 1)));
	}
	TestFalse(TEXT("3F leads on to the boss"), Matsuri::Floors::IsLastFloor(Matsuri::Floors::Count - 1));
	TestTrue(TEXT("the boss floor is the last"), Matsuri::Floors::IsLastFloor(Matsuri::Floors::BossFloor));
	TestTrue(TEXT("the boss has her own map"), FString(Matsuri::Floors::MapFor(Matsuri::Floors::BossFloor)).Contains(TEXT("L_Boss")));
	TestTrue(TEXT("the boss floor is cleared by her defeat, not a score"), Matsuri::Floors::SpecFor(Matsuri::Floors::BossFloor, 7).TargetScore > 100000);
	TestFalse(TEXT("first floor is not last"), Matsuri::Floors::IsLastFloor(0));
	TestNotEqual(TEXT("each run gets a new layout"), Matsuri::Floors::SpecFor(0, 1).Seed, Matsuri::Floors::SpecFor(0, 2).Seed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriFloorsBeatableTest, "SuzuneMatsuri.Floors.EveryFloorPlacesEverythingAndIsBeatable", MatsuriTestFlags)
bool FMatsuriFloorsBeatableTest::RunTest(const FString&)
{
	for (int32 RunSeed = 1; RunSeed <= RunSeedsToSweep; ++RunSeed)
	{
		for (int32 Floor = 0; Floor < Matsuri::Floors::Count; ++Floor)
		{
			const FMatsuriFloorSpec Spec = Matsuri::Floors::SpecFor(Floor, RunSeed);
			const FMatsuriFloorLayout Layout = Matsuri::Layout::BuildFloorLayout(Spec);
			const FString Where = FString::Printf(TEXT("run %d floor %d"), RunSeed, Floor + 1);

			TestEqual(Where + TEXT(" coins"), Layout.Coins.Num(), Spec.KobanCount + Spec.GinkaCount + Spec.MagatamaCount);
			TestTrue(Where + TEXT(" target reachable"), Layout.TotalPoints() >= Spec.TargetScore);
			TestEqual(Where + TEXT(" kappa"), Layout.CountSpawns(EMatsuriSpawn::Kappa), Spec.KappaCount);
			TestEqual(Where + TEXT(" chochin"), Layout.CountSpawns(EMatsuriSpawn::Chochin), Spec.ChochinCount);
			TestEqual(Where + TEXT(" spikes"), Layout.CountSpawns(EMatsuriSpawn::Spike), Spec.SpikeCount);
			TestEqual(Where + TEXT(" ammo"), Layout.CountSpawns(EMatsuriSpawn::Ammo), Spec.AmmoSpotCount);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriFloorsFairStartTest, "SuzuneMatsuri.Floors.ThreatsStartAwayFromPlayer", MatsuriTestFlags)
bool FMatsuriFloorsFairStartTest::RunTest(const FString&)
{
	for (int32 RunSeed = 1; RunSeed <= RunSeedsToSweep; ++RunSeed)
	{
		for (int32 Floor = 0; Floor < Matsuri::Floors::Count; ++Floor)
		{
			const FMatsuriFloorLayout Layout = Matsuri::Layout::BuildFloorLayout(Matsuri::Floors::SpecFor(Floor, RunSeed));
			const FVector2D Start(Layout.PlayerStart);
			for (const FMatsuriSpawnPlacement& Spawn : Layout.Spawns)
			{
				const float Distance = FVector2D::Distance(Start, FVector2D(Spawn.Location));
				const float Needed = IsThreat(Spawn.Type) ? Matsuri::Layout::ThreatClearRadius : Matsuri::Layout::SpawnClearRadius;
				TestTrue(TEXT("spawn keeps its distance from the start"), Distance >= Needed);
			}
		}
	}
	return true;
}

namespace
{
	bool SegmentHitsBox(const FVector2D& A, const FVector2D& B, const FBox2D& Box)
	{
		constexpr int32 Samples = 40;
		for (int32 i = 0; i <= Samples; ++i)
		{
			if (Box.IsInside(FMath::Lerp(A, B, static_cast<float>(i) / Samples)))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMazeFloorsTest, "SuzuneMatsuri.Floors.MazeFloorsAreSoundAndFair", MatsuriTestFlags)
bool FMatsuriMazeFloorsTest::RunTest(const FString&)
{
	for (int32 RunSeed = 1; RunSeed <= RunSeedsToSweep; ++RunSeed)
	{
		for (int32 Floor = 0; Floor < Matsuri::Floors::Count; ++Floor)
		{
			const FMatsuriFloorSpec Spec = Matsuri::Floors::SpecFor(Floor, RunSeed);
			TestTrue(TEXT("every floor is a maze"), Spec.bMaze);
			const FMatsuriFloorLayout Layout = Matsuri::Layout::BuildFloorLayout(Spec);
			const FMatsuriMaze& Maze = Layout.Maze;
			TestTrue(TEXT("maze built"), Maze.IsValid());

			TArray<FBox2D> Walls, Solid;
			for (const FMatsuriBlockPlacement& Block : Layout.Blocks)
			{
				if (Block.Type == EMatsuriBlock::Wall) { Walls.Add(Block.GetFootprint()); }
				if (Block.Type != EMatsuriBlock::Ground) { Solid.Add(Block.GetFootprint()); }
			}

			// Every open passage really is open: the line between the two cell centres crosses no wall block.
			for (int32 i = 0; i < Maze.CellCount(); ++i)
			{
				const FIntPoint Cell = Maze.CellOf(i);
				for (const FIntPoint& Next : Maze.OpenNeighbours(Cell))
				{
					const bool bBlocked = Walls.ContainsByPredicate([&](const FBox2D& Wall) { return SegmentHitsBox(Maze.CellCenter(Cell), Maze.CellCenter(Next), Wall); });
					TestFalse(TEXT("open passage is not walled off"), bBlocked);
				}
			}

			for (const FMatsuriCoinPlacement& Coin : Layout.Coins)
			{
				const bool bBuried = Solid.ContainsByPredicate([&Coin](const FBox2D& Box) { return Box.ExpandBy(Matsuri::Layout::CoinClearance).IsInside(FVector2D(Coin.Location)); });
				TestFalse(TEXT("coin is in open space"), bBuried);
			}

			const TArray<int32> Steps = Maze.StepsTo(Layout.StartCell);
			for (const FMatsuriSpawnPlacement& Spawn : Layout.Spawns)
			{
				if (Spawn.Type != EMatsuriSpawn::Ammo)
				{
					TestTrue(TEXT("threats start 3+ cells away along the corridors"),
						Steps[Maze.Index(Maze.CellAt(FVector2D(Spawn.Location)))] >= Matsuri::Layout::ThreatMinCells);
				}
			}
			TestEqual(TEXT("start is the west-middle cell"), Maze.CellAt(FVector2D(Layout.PlayerStart)), Layout.StartCell);
			// Playtest: some floors began staring at a wall. The first view must look out of an open doorway.
			const FVector Facing = Layout.PlayerStartRotation.Vector();
			const FMatsuriMaze::EWall Faced = FMath::Abs(Facing.X) >= FMath::Abs(Facing.Y)
				? (Facing.X > 0.0 ? FMatsuriMaze::East : FMatsuriMaze::West)
				: (Facing.Y > 0.0 ? FMatsuriMaze::North : FMatsuriMaze::South);
			TestFalse(TEXT("the start faces an open doorway, not a wall"), Maze.HasWall(Layout.StartCell, Faced));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
