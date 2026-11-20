// Suzune's Night Festival - deterministic gray-box floor layout.

#include "MatsuriFloorLayout.h"
#include "Math/RandomStream.h"
#include "Misc/Optional.h"

namespace
{
	constexpr float GroundThickness = 50.0f;
	constexpr float WallThickness = 100.0f;
	constexpr float StartInset = 300.0f;
	constexpr float CoinEdgeInset = 120.0f;
	constexpr int32 AttemptsPerObstacle = 40;
	constexpr int32 AttemptsPerSpot = 200;
	constexpr float PillarChance = 0.3f;

	bool Overlaps(const FBox2D& A, const FBox2D& B)
	{
		return A.Min.X < B.Max.X && B.Min.X < A.Max.X && A.Min.Y < B.Max.Y && B.Min.Y < A.Max.Y;
	}

	bool OverlapsAny(const FBox2D& Box, const TArray<FBox2D>& Others)
	{
		return Others.ContainsByPredicate([&Box](const FBox2D& Other) { return Overlaps(Box, Other); });
	}

	FBox2D MakeFootprint(const FVector& Center, const FVector& Size)
	{
		const FVector2D Half(Size.X * 0.5f, Size.Y * 0.5f);
		const FVector2D Mid(Center.X, Center.Y);
		return FBox2D(Mid - Half, Mid + Half);
	}

	void AddBoundary(const FMatsuriFloorSpec& Spec, TArray<FMatsuriBlockPlacement>& Blocks)
	{
		const float H = Spec.HalfExtent;
		const float Span = 2.0f * (H + WallThickness);
		const float WallZ = Spec.WallHeight * 0.5f;
		const float WallOffset = H + WallThickness * 0.5f;

		Blocks.Add({ EMatsuriBlock::Ground, FVector(0, 0, -GroundThickness * 0.5f), FVector(Span, Span, GroundThickness) });
		Blocks.Add({ EMatsuriBlock::Wall, FVector(WallOffset, 0, WallZ), FVector(WallThickness, Span, Spec.WallHeight) });
		Blocks.Add({ EMatsuriBlock::Wall, FVector(-WallOffset, 0, WallZ), FVector(WallThickness, Span, Spec.WallHeight) });
		Blocks.Add({ EMatsuriBlock::Wall, FVector(0, WallOffset, WallZ), FVector(Span, WallThickness, Spec.WallHeight) });
		Blocks.Add({ EMatsuriBlock::Wall, FVector(0, -WallOffset, WallZ), FVector(Span, WallThickness, Spec.WallHeight) });
	}

	FMatsuriBlockPlacement RollObstacle(FRandomStream& Rng, float HalfExtent)
	{
		const bool bPillar = Rng.FRand() < PillarChance;
		const FVector Size = bPillar
			? FVector(Rng.FRandRange(100, 200), Rng.FRandRange(100, 200), Rng.FRandRange(300, 420))
			: FVector(Rng.FRandRange(150, 420), Rng.FRandRange(150, 420), Rng.FRandRange(100, 130));

		const float MaxX = HalfExtent - Matsuri::Layout::ObstacleGap - Size.X * 0.5f;
		const float MaxY = HalfExtent - Matsuri::Layout::ObstacleGap - Size.Y * 0.5f;
		const FVector Center(Rng.FRandRange(-MaxX, MaxX), Rng.FRandRange(-MaxY, MaxY), Size.Z * 0.5f);
		return { bPillar ? EMatsuriBlock::Pillar : EMatsuriBlock::LowCover, Center, Size };
	}

	void AddObstacles(const FMatsuriFloorSpec& Spec, const FVector2D& Start, FRandomStream& Rng,
		TArray<FMatsuriBlockPlacement>& Blocks, TArray<FBox2D>& OutFootprints)
	{
		const float SpawnRadiusSq = FMath::Square(Matsuri::Layout::SpawnClearRadius);
		const int32 MaxAttempts = Spec.ObstacleCount * AttemptsPerObstacle;

		for (int32 Attempt = 0; Attempt < MaxAttempts && OutFootprints.Num() < Spec.ObstacleCount; ++Attempt)
		{
			const FMatsuriBlockPlacement Candidate = RollObstacle(Rng, Spec.HalfExtent);
			const FBox2D Footprint = Candidate.GetFootprint();

			if (Footprint.ComputeSquaredDistanceToPoint(Start) < SpawnRadiusSq
				|| OverlapsAny(Footprint.ExpandBy(Matsuri::Layout::ObstacleGap), OutFootprints))
			{
				continue;
			}
			Blocks.Add(Candidate);
			OutFootprints.Add(Footprint);
		}
	}

	/** Free-space sampler shared by coins and spawns. Tracks everything placed so far. */
	struct FSpotSampler
	{
		const TArray<FBox2D>& Footprints;
		FVector2D Start;
		float Reach = 0.0f;
		TArray<FVector2D> Occupied;

		bool IsFree(const FVector2D& Spot, float StartClearance, float Spacing) const
		{
			if (FVector2D::DistSquared(Spot, Start) < FMath::Square(StartClearance))
			{
				return false;
			}
			const bool bBlocked = Footprints.ContainsByPredicate([&Spot](const FBox2D& Box)
			{
				return Box.ExpandBy(Matsuri::Layout::CoinClearance).IsInside(Spot);
			});
			const bool bCrowded = Occupied.ContainsByPredicate([&Spot, Spacing](const FVector2D& Other)
			{
				return FVector2D::DistSquared(Spot, Other) < FMath::Square(Spacing);
			});
			return !bBlocked && !bCrowded;
		}

		TOptional<FVector2D> Take(FRandomStream& Rng, float StartClearance, float Spacing)
		{
			return TakeIn(Rng, FBox2D(FVector2D(-Reach), FVector2D(Reach)), StartClearance, Spacing);
		}

		TOptional<FVector2D> TakeIn(FRandomStream& Rng, const FBox2D& Region, float StartClearance, float Spacing)
		{
			for (int32 Attempt = 0; Attempt < AttemptsPerSpot; ++Attempt)
			{
				const FVector2D Spot(Rng.FRandRange(Region.Min.X, Region.Max.X), Rng.FRandRange(Region.Min.Y, Region.Max.Y));
				if (IsFree(Spot, StartClearance, Spacing))
				{
					Occupied.Add(Spot);
					return Spot;
				}
			}
			return {};
		}
	};

	void AddCoins(const FMatsuriFloorSpec& Spec, FRandomStream& Rng, FSpotSampler& Sampler, TArray<FMatsuriCoinPlacement>& Coins)
	{
		// Rarest first so the valuable pieces never lose out on space.
		TArray<EMatsuriCollectible> Queue;
		Queue.Reserve(Spec.MagatamaCount + Spec.GinkaCount + Spec.KobanCount);
		for (int32 i = 0; i < Spec.MagatamaCount; ++i) { Queue.Add(EMatsuriCollectible::Magatama); }
		for (int32 i = 0; i < Spec.GinkaCount; ++i) { Queue.Add(EMatsuriCollectible::Ginka); }
		for (int32 i = 0; i < Spec.KobanCount; ++i) { Queue.Add(EMatsuriCollectible::Koban); }

		for (const EMatsuriCollectible Kind : Queue)
		{
			if (const TOptional<FVector2D> Spot = Sampler.Take(Rng, Matsuri::Layout::SpawnClearRadius, Matsuri::Layout::CoinSpacing))
			{
				Coins.Add({ Kind, FVector(*Spot, Matsuri::Layout::CoinHeight) });
			}
		}
	}

	void AddSpawns(const FMatsuriFloorSpec& Spec, FRandomStream& Rng, FSpotSampler& Sampler, TArray<FMatsuriSpawnPlacement>& Spawns)
	{
		const TPair<EMatsuriSpawn, int32> Counts[] = {
			{ EMatsuriSpawn::Kappa, Spec.KappaCount },
			{ EMatsuriSpawn::Kasa, Spec.KasaCount },
			{ EMatsuriSpawn::Chochin, Spec.ChochinCount },
			{ EMatsuriSpawn::Foxfire, Spec.FoxfireCount },
			{ EMatsuriSpawn::Spike, Spec.SpikeCount },
			{ EMatsuriSpawn::Ammo, Spec.AmmoSpotCount },
		};
		for (const TPair<EMatsuriSpawn, int32>& Entry : Counts)
		{
			// Ammo may sit near the start; anything that hurts must not.
			const bool bThreat = Entry.Key != EMatsuriSpawn::Ammo;
			const float Clearance = bThreat ? Matsuri::Layout::ThreatClearRadius : Matsuri::Layout::SpawnClearRadius;
			for (int32 i = 0; i < Entry.Value; ++i)
			{
				if (const TOptional<FVector2D> Spot = Sampler.Take(Rng, Clearance, Matsuri::Layout::SpawnSpacing))
				{
					Spawns.Add({ Entry.Key, FVector(*Spot, 0.0f) });
				}
			}
		}
	}
}

FBox2D FMatsuriBlockPlacement::GetFootprint() const
{
	return MakeFootprint(Center, Size);
}

int32 FMatsuriFloorLayout::CountCoins(EMatsuriCollectible Kind) const
{
	return Coins.FilterByPredicate([Kind](const FMatsuriCoinPlacement& Coin) { return Coin.Kind == Kind; }).Num();
}

int32 FMatsuriFloorLayout::CountSpawns(EMatsuriSpawn Type) const
{
	return Spawns.FilterByPredicate([Type](const FMatsuriSpawnPlacement& Spawn) { return Spawn.Type == Type; }).Num();
}

int32 FMatsuriFloorLayout::TotalPoints() const
{
	int32 Total = 0;
	for (const FMatsuriCoinPlacement& Coin : Coins)
	{
		Total += Matsuri::PointsFor(Coin.Kind);
	}
	return Total;
}

namespace
{
	FMatsuriFloorLayout BuildArenaLayout(const FMatsuriFloorSpec& Spec)
	{
	FMatsuriFloorLayout Layout;
	FRandomStream Rng(Spec.Seed);

	Layout.PlayerStart = FVector(-Spec.HalfExtent + StartInset, 0.0f, 100.0f);
	Layout.PlayerStartRotation = FRotator::ZeroRotator;	// facing +X, into the floor
	const FVector2D Start(Layout.PlayerStart);

	AddBoundary(Spec, Layout.Blocks);

	TArray<FBox2D> Footprints;
	AddObstacles(Spec, Start, Rng, Layout.Blocks, Footprints);

	FSpotSampler Sampler{ Footprints, Start, Spec.HalfExtent - CoinEdgeInset };
	AddCoins(Spec, Rng, Sampler, Layout.Coins);
	AddSpawns(Spec, Rng, Sampler, Layout.Spawns);
	return Layout;
	}

	// ---------------------------------------------------------------- maze floors

	FBox2D CellInterior(const FMatsuriMaze& Maze, const FIntPoint& Cell)
	{
		const FVector2D Center = Maze.CellCenter(Cell);
		const float Half = Maze.CellSize() * 0.5f - Matsuri::Layout::CellMargin;
		return FBox2D(Center - FVector2D(Half), Center + FVector2D(Half));
	}

	void AddMazeWalls(const FMatsuriFloorSpec& Spec, const FMatsuriMaze& Maze, TArray<FMatsuriBlockPlacement>& Blocks, TArray<FBox2D>& Footprints)
	{
		const float Cell = Maze.CellSize();
		const float T = Matsuri::Layout::MazeWallThickness;
		const float Z = Spec.WallHeight * 0.5f;
		for (int32 i = 0; i < Maze.CellCount(); ++i)
		{
			const FIntPoint C = Maze.CellOf(i);
			const FVector2D Center = Maze.CellCenter(C);
			if (C.X + 1 < Maze.Size && Maze.HasWall(C, FMatsuriMaze::East))
			{
				Blocks.Add({ EMatsuriBlock::Wall, FVector(Center.X + Cell * 0.5f, Center.Y, Z), FVector(T, Cell + T, Spec.WallHeight) });
				Footprints.Add(Blocks.Last().GetFootprint());
			}
			if (C.Y + 1 < Maze.Size && Maze.HasWall(C, FMatsuriMaze::North))
			{
				Blocks.Add({ EMatsuriBlock::Wall, FVector(Center.X, Center.Y + Cell * 0.5f, Z), FVector(Cell + T, T, Spec.WallHeight) });
				Footprints.Add(Blocks.Last().GetFootprint());
			}
		}
	}

	/** Crates in some cells (leaving >= ObstacleGap around them) and stone lanterns on some wall junctions. */
	void AddMazeCover(const FMatsuriFloorSpec& Spec, const FMatsuriMaze& Maze, const TArray<int32>& StepsFromStart, const FIntPoint& Start,
		FRandomStream& Rng, TArray<FMatsuriBlockPlacement>& Blocks, TArray<FBox2D>& Footprints)
	{
		const float Cell = Maze.CellSize();
		const float MaxCover = FMath::Max(80.0f, Cell - Matsuri::Layout::MazeWallThickness - 2.0f * Matsuri::Layout::ObstacleGap);
		for (int32 i = 0; i < Maze.CellCount(); ++i)
		{
			if (StepsFromStart[i] < 2 || Rng.FRand() >= Spec.CoverChance)
			{
				continue;
			}
			const FVector2D Center = Maze.CellCenter(Maze.CellOf(i));
			const FVector Size(Rng.FRandRange(80.0f, MaxCover), Rng.FRandRange(80.0f, MaxCover), Rng.FRandRange(100.0f, 130.0f));
			Blocks.Add({ EMatsuriBlock::LowCover, FVector(Center, Size.Z * 0.5f), Size });
			Footprints.Add(Blocks.Last().GetFootprint());
		}

		for (int32 X = 1; X < Maze.Size; ++X)
		{
			for (int32 Y = 1; Y < Maze.Size; ++Y)
			{
				// Junction shared by cells (X-1..X, Y-1..Y): only where a wall ends, never next to the start.
				const bool bWalled = Maze.HasWall(FIntPoint(X - 1, Y - 1), FMatsuriMaze::East) || Maze.HasWall(FIntPoint(X - 1, Y - 1), FMatsuriMaze::North)
					|| Maze.HasWall(FIntPoint(X, Y), FMatsuriMaze::West) || Maze.HasWall(FIntPoint(X, Y), FMatsuriMaze::South);
				const bool bNearStart = FMath::Abs(X - Start.X) <= 1 && FMath::Abs(Y - Start.Y) <= 1;
				if (!bWalled || bNearStart || Rng.FRand() >= Spec.PillarChance)
				{
					continue;
				}
				const FVector2D Point(-Spec.HalfExtent + X * Cell, -Spec.HalfExtent + Y * Cell);
				const float Height = Rng.FRandRange(320.0f, 380.0f);
				Blocks.Add({ EMatsuriBlock::Pillar, FVector(Point, Height * 0.5f), FVector(110.0f, 110.0f, Height) });
				Footprints.Add(Blocks.Last().GetFootprint());
			}
		}
	}

	/** Eligible cells for an item, shuffled; never the start cell. */
	TArray<FIntPoint> ShuffledCells(const FMatsuriMaze& Maze, const TArray<int32>& StepsFromStart, int32 MinSteps, FRandomStream& Rng)
	{
		TArray<FIntPoint> Cells;
		for (int32 i = 0; i < Maze.CellCount(); ++i)
		{
			if (StepsFromStart[i] >= FMath::Max(1, MinSteps))
			{
				Cells.Add(Maze.CellOf(i));
			}
		}
		for (int32 i = Cells.Num() - 1; i > 0; --i)
		{
			Cells.Swap(i, Rng.RandRange(0, i));
		}
		return Cells;
	}

	/** Places one item in the next eligible cell (cycling the shuffled list) that has a free spot. */
	TOptional<FVector2D> TakeInCells(const FMatsuriMaze& Maze, const TArray<FIntPoint>& Cells, int32& Cursor, FRandomStream& Rng,
		FSpotSampler& Sampler, float StartClearance, float Spacing)
	{
		for (int32 Tries = 0; Tries < Cells.Num(); ++Tries)
		{
			const FIntPoint Cell = Cells[Cursor++ % Cells.Num()];
			if (const TOptional<FVector2D> Spot = Sampler.TakeIn(Rng, CellInterior(Maze, Cell), StartClearance, Spacing))
			{
				return Spot;
			}
		}
		return {};
	}

	void AddMazeItems(const FMatsuriFloorSpec& Spec, const FMatsuriMaze& Maze, const TArray<int32>& StepsFromStart,
		FRandomStream& Rng, FSpotSampler& Sampler, FMatsuriFloorLayout& Layout)
	{
		using namespace Matsuri::Layout;
		const TArray<FIntPoint> AnyCell = ShuffledCells(Maze, StepsFromStart, 1, Rng);
		const TArray<FIntPoint> FarCell = ShuffledCells(Maze, StepsFromStart, ThreatMinCells, Rng);
		int32 AnyCursor = 0;
		int32 FarCursor = 0;

		TArray<EMatsuriCollectible> Queue;
		for (int32 i = 0; i < Spec.MagatamaCount; ++i) { Queue.Add(EMatsuriCollectible::Magatama); }
		for (int32 i = 0; i < Spec.GinkaCount; ++i) { Queue.Add(EMatsuriCollectible::Ginka); }
		for (int32 i = 0; i < Spec.KobanCount; ++i) { Queue.Add(EMatsuriCollectible::Koban); }
		for (const EMatsuriCollectible Kind : Queue)
		{
			if (const TOptional<FVector2D> Spot = TakeInCells(Maze, AnyCell, AnyCursor, Rng, Sampler, SpawnClearRadius, CoinSpacing))
			{
				Layout.Coins.Add({ Kind, FVector(*Spot, CoinHeight) });
			}
		}

		const TPair<EMatsuriSpawn, int32> Counts[] = {
			{ EMatsuriSpawn::Kappa, Spec.KappaCount }, { EMatsuriSpawn::Kasa, Spec.KasaCount }, { EMatsuriSpawn::Chochin, Spec.ChochinCount },
			{ EMatsuriSpawn::Foxfire, Spec.FoxfireCount }, { EMatsuriSpawn::Spike, Spec.SpikeCount }, { EMatsuriSpawn::Ammo, Spec.AmmoSpotCount } };
		for (const TPair<EMatsuriSpawn, int32>& Entry : Counts)
		{
			const bool bThreat = Entry.Key != EMatsuriSpawn::Ammo;
			for (int32 i = 0; i < Entry.Value; ++i)
			{
				const TOptional<FVector2D> Spot = bThreat
					? TakeInCells(Maze, FarCell, FarCursor, Rng, Sampler, ThreatClearRadius, SpawnSpacing)
					: TakeInCells(Maze, AnyCell, AnyCursor, Rng, Sampler, SpawnClearRadius, SpawnSpacing);
				if (Spot)
				{
					Layout.Spawns.Add({ Entry.Key, FVector(*Spot, 0.0f) });
				}
			}
		}
	}

	FMatsuriFloorLayout BuildMazeLayout(const FMatsuriFloorSpec& Spec)
	{
		FMatsuriFloorLayout Layout;
		FRandomStream Rng(Spec.Seed);
		Layout.Maze = Matsuri::Maze::Generate(Spec.MazeCells, Spec.HalfExtent, Spec.Seed ^ 0x5bd1e995, Spec.LoopChance, Spec.Plazas);
		const FMatsuriMaze& Maze = Layout.Maze;

		Layout.StartCell = FIntPoint(0, Maze.Size / 2);
		Layout.ExitCell = FIntPoint(Maze.Size - 1, Maze.Size / 2);
		Layout.PlayerStart = FVector(Maze.CellCenter(Layout.StartCell), 100.0f);
		// Face out of an open doorway (east, else north, else south; west is the shrine fence).
		Layout.PlayerStartRotation = !Maze.HasWall(Layout.StartCell, FMatsuriMaze::East) ? FRotator::ZeroRotator
			: !Maze.HasWall(Layout.StartCell, FMatsuriMaze::North) ? FRotator(0.0f, 90.0f, 0.0f)
			: FRotator(0.0f, -90.0f, 0.0f);

		const TArray<int32> StepsFromStart = Maze.StepsTo(Layout.StartCell);
		TArray<FBox2D> Footprints;
		AddBoundary(Spec, Layout.Blocks);
		AddMazeWalls(Spec, Maze, Layout.Blocks, Footprints);
		AddMazeCover(Spec, Maze, StepsFromStart, Layout.StartCell, Rng, Layout.Blocks, Footprints);

		FSpotSampler Sampler{ Footprints, FVector2D(Layout.PlayerStart), Spec.HalfExtent - CoinEdgeInset };
		AddMazeItems(Spec, Maze, StepsFromStart, Rng, Sampler, Layout);
		return Layout;
	}
}

FMatsuriFloorLayout Matsuri::Layout::BuildFloorLayout(const FMatsuriFloorSpec& Spec)
{
	return Spec.bMaze ? BuildMazeLayout(Spec) : BuildArenaLayout(Spec);
}

