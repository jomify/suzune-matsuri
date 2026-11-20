// Suzune's Night Festival - square-cell maze: generation and path-following.

#include "MatsuriMaze.h"
#include "Containers/Queue.h"
#include "Math/RandomStream.h"

namespace
{
	struct FDirection
	{
		FIntPoint Delta;
		FMatsuriMaze::EWall Wall;
		FMatsuriMaze::EWall Opposite;
	};

	const FDirection Directions[4] = {
		{ FIntPoint(1, 0), FMatsuriMaze::East, FMatsuriMaze::West },
		{ FIntPoint(0, 1), FMatsuriMaze::North, FMatsuriMaze::South },
		{ FIntPoint(-1, 0), FMatsuriMaze::West, FMatsuriMaze::East },
		{ FIntPoint(0, -1), FMatsuriMaze::South, FMatsuriMaze::North },
	};

	constexpr uint8 AllWalls = FMatsuriMaze::East | FMatsuriMaze::North | FMatsuriMaze::West | FMatsuriMaze::South;

	void Open(FMatsuriMaze& Maze, const FIntPoint& Cell, const FDirection& Direction)
	{
		Maze.Walls[Maze.Index(Cell)] &= ~Direction.Wall;
		Maze.Walls[Maze.Index(Cell + Direction.Delta)] &= ~Direction.Opposite;
	}

	void CarvePerfectMaze(FMatsuriMaze& Maze, FRandomStream& Rng)
	{
		TArray<bool> Visited;
		Visited.Init(false, Maze.CellCount());
		TArray<FIntPoint> Stack = { FIntPoint(0, 0) };
		Visited[0] = true;

		while (Stack.Num() > 0)
		{
			const FIntPoint Cell = Stack.Last();
			TArray<const FDirection*, TInlineAllocator<4>> Options;
			for (const FDirection& Direction : Directions)
			{
				const FIntPoint Next = Cell + Direction.Delta;
				if (Maze.InBounds(Next) && !Visited[Maze.Index(Next)])
				{
					Options.Add(&Direction);
				}
			}
			if (Options.Num() == 0)
			{
				Stack.Pop();
				continue;
			}
			const FDirection& Chosen = *Options[Rng.RandRange(0, Options.Num() - 1)];
			Open(Maze, Cell, Chosen);
			Visited[Maze.Index(Cell + Chosen.Delta)] = true;
			Stack.Push(Cell + Chosen.Delta);
		}
	}

	void AddLoops(FMatsuriMaze& Maze, FRandomStream& Rng, float LoopChance)
	{
		for (int32 i = 0; i < Maze.CellCount(); ++i)
		{
			const FIntPoint Cell = Maze.CellOf(i);
			for (const FDirection& Direction : { Directions[0], Directions[1] })	// each inner wall once
			{
				if (Maze.InBounds(Cell + Direction.Delta) && Maze.HasWall(Cell, Direction.Wall) && Rng.FRand() < LoopChance)
				{
					Open(Maze, Cell, Direction);
				}
			}
		}
	}

	void AddPlazas(FMatsuriMaze& Maze, FRandomStream& Rng, int32 Plazas)
	{
		for (int32 i = 0; i < Plazas && Maze.Size >= 3; ++i)
		{
			const FIntPoint Corner(Rng.RandRange(1, Maze.Size - 2), Rng.RandRange(0, Maze.Size - 2));
			Open(Maze, Corner, Directions[0]);
			Open(Maze, Corner, Directions[1]);
			Open(Maze, Corner + FIntPoint(1, 0), Directions[1]);
			Open(Maze, Corner + FIntPoint(0, 1), Directions[0]);
		}
	}
}

FIntPoint FMatsuriMaze::CellAt(const FVector2D& World) const
{
	const float Cell = CellSize();
	const int32 X = FMath::Clamp(FMath::FloorToInt32((World.X + HalfExtent) / Cell), 0, Size - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt32((World.Y + HalfExtent) / Cell), 0, Size - 1);
	return FIntPoint(X, Y);
}

FVector2D FMatsuriMaze::CellCenter(const FIntPoint& Cell) const
{
	const float Size2 = CellSize();
	return FVector2D(-HalfExtent + (Cell.X + 0.5f) * Size2, -HalfExtent + (Cell.Y + 0.5f) * Size2);
}

TArray<FIntPoint> FMatsuriMaze::OpenNeighbours(const FIntPoint& Cell) const
{
	TArray<FIntPoint> Result;
	for (const FDirection& Direction : Directions)
	{
		if (!HasWall(Cell, Direction.Wall) && InBounds(Cell + Direction.Delta))
		{
			Result.Add(Cell + Direction.Delta);
		}
	}
	return Result;
}

int32 FMatsuriMaze::CountPassages() const
{
	int32 Count = 0;
	for (int32 i = 0; i < CellCount(); ++i)
	{
		const FIntPoint Cell = CellOf(i);
		Count += (Cell.X + 1 < Size && !HasWall(Cell, East)) ? 1 : 0;
		Count += (Cell.Y + 1 < Size && !HasWall(Cell, North)) ? 1 : 0;
	}
	return Count;
}

TArray<int32> FMatsuriMaze::StepsTo(const FIntPoint& Target) const
{
	TArray<int32> Steps;
	Steps.Init(INDEX_NONE, CellCount());
	if (!InBounds(Target))
	{
		return Steps;
	}
	TQueue<FIntPoint> Frontier;
	Steps[Index(Target)] = 0;
	Frontier.Enqueue(Target);
	FIntPoint Cell;
	while (Frontier.Dequeue(Cell))
	{
		for (const FIntPoint& Next : OpenNeighbours(Cell))
		{
			if (Steps[Index(Next)] == INDEX_NONE)
			{
				Steps[Index(Next)] = Steps[Index(Cell)] + 1;
				Frontier.Enqueue(Next);
			}
		}
	}
	return Steps;
}

FVector2D FMatsuriMaze::NextWaypoint(const FVector2D& From, const FVector2D& Target, int32* OutSteps) const
{
	const FIntPoint FromCell = CellAt(From);
	const FIntPoint TargetCell = CellAt(Target);
	const TArray<int32> Steps = StepsTo(TargetCell);
	const int32 Remaining = Steps[Index(FromCell)];
	if (OutSteps)
	{
		*OutSteps = Remaining;
	}
	if (FromCell == TargetCell || Remaining == INDEX_NONE)
	{
		return Target;
	}
	for (const FIntPoint& Next : OpenNeighbours(FromCell))
	{
		if (Steps[Index(Next)] == Remaining - 1)
		{
			const FVector2D Here = CellCenter(FromCell);
			const FVector2D There = CellCenter(Next);
			const FVector2D Doorway = (Here + There) * 0.5f;
			return Doorway + (There - Here).GetSafeNormal() * (CellSize() * DoorLead);
		}
	}
	return Target;
}

FMatsuriMaze Matsuri::Maze::Generate(int32 Size, float HalfExtent, int32 Seed, float LoopChance, int32 Plazas)
{
	FMatsuriMaze Maze;
	Maze.Size = FMath::Max(1, Size);
	Maze.HalfExtent = HalfExtent;
	Maze.Walls.Init(AllWalls, Maze.CellCount());

	FRandomStream Rng(Seed);
	CarvePerfectMaze(Maze, Rng);
	AddLoops(Maze, Rng, LoopChance);
	AddPlazas(Maze, Rng, Plazas);
	return Maze;
}
