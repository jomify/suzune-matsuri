// Suzune's Night Festival - maze generation and path-following unit tests.
// Run: Automation RunTests SuzuneMatsuri.Maze

#include "Misc/AutomationTest.h"
#include "MatsuriMaze.h"
#include "MatsuriYokai.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags MatsuriTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int32 SeedsToSweep = 30;

	FMatsuriMaze MakeMaze(int32 Seed, float LoopChance = 0.2f, int32 Plazas = 2)
	{
		return Matsuri::Maze::Generate(8, 2400.0f, Seed, LoopChance, Plazas);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMazeConnectedTest, "SuzuneMatsuri.Maze.EveryCellIsReachable", MatsuriTestFlags)
bool FMatsuriMazeConnectedTest::RunTest(const FString&)
{
	for (int32 Seed = 1; Seed <= SeedsToSweep; ++Seed)
	{
		const FMatsuriMaze Maze = MakeMaze(Seed);
		const TArray<int32> Steps = Maze.StepsTo(FIntPoint(0, 0));
		TestFalse(FString::Printf(TEXT("seed %d: no unreachable cell"), Seed), Steps.Contains(INDEX_NONE));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMazeBorderTest, "SuzuneMatsuri.Maze.BorderStaysClosedAndWallsAgree", MatsuriTestFlags)
bool FMatsuriMazeBorderTest::RunTest(const FString&)
{
	const FMatsuriMaze Maze = MakeMaze(3);
	for (int32 i = 0; i < Maze.CellCount(); ++i)
	{
		const FIntPoint Cell = Maze.CellOf(i);
		if (Cell.X == 0) { TestTrue(TEXT("west border closed"), Maze.HasWall(Cell, FMatsuriMaze::West)); }
		if (Cell.Y == 0) { TestTrue(TEXT("south border closed"), Maze.HasWall(Cell, FMatsuriMaze::South)); }
		if (Cell.X == Maze.Size - 1) { TestTrue(TEXT("east border closed"), Maze.HasWall(Cell, FMatsuriMaze::East)); }
		if (Cell.Y == Maze.Size - 1) { TestTrue(TEXT("north border closed"), Maze.HasWall(Cell, FMatsuriMaze::North)); }
		if (Cell.X + 1 < Maze.Size)
		{
			TestEqual(TEXT("both sides of an east wall agree"), Maze.HasWall(Cell, FMatsuriMaze::East), Maze.HasWall(Cell + FIntPoint(1, 0), FMatsuriMaze::West));
		}
		if (Cell.Y + 1 < Maze.Size)
		{
			TestEqual(TEXT("both sides of a north wall agree"), Maze.HasWall(Cell, FMatsuriMaze::North), Maze.HasWall(Cell + FIntPoint(0, 1), FMatsuriMaze::South));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMazeLoopsTest, "SuzuneMatsuri.Maze.LoopsAddAlternativeRoutes", MatsuriTestFlags)
bool FMatsuriMazeLoopsTest::RunTest(const FString&)
{
	const FMatsuriMaze Perfect = MakeMaze(5, 0.0f, 0);
	TestEqual(TEXT("no loops: a perfect maze (tree)"), Perfect.CountPassages(), Perfect.CellCount() - 1);

	const FMatsuriMaze Looped = MakeMaze(5, 0.25f, 0);
	TestTrue(TEXT("loops add passages"), Looped.CountPassages() > Looped.CellCount() - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMazeDeterministicTest, "SuzuneMatsuri.Maze.SameSeedSameMaze", MatsuriTestFlags)
bool FMatsuriMazeDeterministicTest::RunTest(const FString&)
{
	TestTrue(TEXT("same seed"), MakeMaze(11).Walls == MakeMaze(11).Walls);
	TestFalse(TEXT("different seed"), MakeMaze(11).Walls == MakeMaze(12).Walls);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMazeCellMathTest, "SuzuneMatsuri.Maze.CellCoordinatesRoundTrip", MatsuriTestFlags)
bool FMatsuriMazeCellMathTest::RunTest(const FString&)
{
	const FMatsuriMaze Maze = MakeMaze(1);
	for (int32 i = 0; i < Maze.CellCount(); ++i)
	{
		const FIntPoint Cell = Maze.CellOf(i);
		TestEqual(TEXT("centre maps back to its cell"), Maze.CellAt(Maze.CellCenter(Cell)), Cell);
	}
	TestEqual(TEXT("outside clamps to the grid"), Maze.CellAt(FVector2D(-99999.0f, 99999.0f)), FIntPoint(0, Maze.Size - 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMazePathTest, "SuzuneMatsuri.Maze.WaypointsLeadToTheTarget", MatsuriTestFlags)
bool FMatsuriMazePathTest::RunTest(const FString&)
{
	for (int32 Seed = 1; Seed <= SeedsToSweep; ++Seed)
	{
		const FMatsuriMaze Maze = MakeMaze(Seed);
		const FVector2D Target = Maze.CellCenter(FIntPoint(Maze.Size - 1, Maze.Size / 2));
		FVector2D Position = Maze.CellCenter(FIntPoint(0, 0)) + FVector2D(37.0f, -21.0f);

		int32 Steps = INDEX_NONE;
		Maze.NextWaypoint(Position, Target, &Steps);
		const int32 Expected = Steps;
		int32 Hops = 0;
		while (Steps > 0 && Hops <= Maze.CellCount())
		{
			int32 Before = Steps;
			Position = Maze.NextWaypoint(Position, Target, &Steps);
			const FIntPoint Here = Maze.CellAt(Position);
			const int32 After = Maze.StepsTo(Maze.CellAt(Target))[Maze.Index(Here)];
			TestEqual(TEXT("each waypoint is one cell closer"), After, Before - 1);
			Maze.NextWaypoint(Position, Target, &Steps);
			++Hops;
		}
		TestEqual(FString::Printf(TEXT("seed %d: reached in the shortest number of hops"), Seed), Hops, Expected);
		TestEqual(TEXT("in the same cell the waypoint is the target itself"), Maze.NextWaypoint(Target + FVector2D(10.0f, 10.0f), Target), Target);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriSteerAroundTest, "SuzuneMatsuri.Maze.YokaiStepAroundCrates", MatsuriTestFlags)
bool FMatsuriSteerAroundTest::RunTest(const FString&)
{
	// Playtest / Chase: a kasa walking head-on into a crate between two doorways stalled there for good.
	const FVector Step(30.0f, 0.0f, 0.0f);			// heading +X
	const FVector Face(-1.0f, 0.0f, 0.0f);			// the crate's face it ran into
	const FVector LeftOfCentre(-80.0f, 40.0f, 0.0f);	// the yokai is a little +Y of the crate's centre

	const FVector HeadOn = Matsuri::Yokai::AroundObstacle(Step, Face, LeftOfCentre);
	TestTrue(TEXT("head-on: keeps moving at full step"), FMath::IsNearlyEqual(HeadOn.Size(), Step.Size(), 0.01f));
	TestTrue(TEXT("head-on: goes round the nearer edge (+Y)"), HeadOn.Y > 0.0f && FMath::IsNearlyZero(HeadOn.X, 0.01f));
	TestTrue(TEXT("mirror: -Y of centre goes -Y"), Matsuri::Yokai::AroundObstacle(Step, Face, FVector(-80.0f, -40.0f, 0.0f)).Y < 0.0f);

	const FVector Glancing = Matsuri::Yokai::AroundObstacle(FVector(30.0f, 30.0f, 0.0f), Face, LeftOfCentre);
	TestTrue(TEXT("glancing: slides along the face, never into it"), Glancing.X >= -0.01f && Glancing.Y > 0.0f);
	TestTrue(TEXT("stays on the ground plane"), FMath::IsNearlyZero(HeadOn.Z) && FMath::IsNearlyZero(Glancing.Z));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
