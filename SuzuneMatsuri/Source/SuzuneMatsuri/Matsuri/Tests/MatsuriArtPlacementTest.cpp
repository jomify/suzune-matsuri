// Suzune's Night Festival - set dressing never clips (playtest: "props poke through walls").
// Builds every floor for several run seeds in a throwaway world, exactly as the floor game mode does,
// then checks every visible Tripo mesh against walls, visible blocks, coins and the other art, and that the
// colliders the dressing adds (torii posts, small stone lanterns) never block a yokai's way across a maze cell.
// Skips quietly when no art is imported. Run: Automation RunTests SuzuneMatsuri.Art.NoClipping

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MatsuriArt.h"
#include "MatsuriCollectible.h"
#include "MatsuriFloorBuilder.h"
#include "MatsuriFloorDecor.h"
#include "MatsuriFloorLayout.h"
#include "MatsuriFloors.h"

namespace
{
	constexpr float TouchTolerance = 6.0f;	// resting on / standing against is fine; sinking in is not
	constexpr float YokaiRadius = 50.0f;	// kasa / kappa body
	const int32 RunSeeds[] = { 11, 2024, 777777, 31337, 900001 };

	struct FPiece
	{
		FString Name;
		FBox Box;
		bool bArt = false;
	};

	bool IsArt(const AStaticMeshActor& Actor)
	{
		const UStaticMesh* Mesh = Actor.GetStaticMeshComponent()->GetStaticMesh();
		return Mesh && Mesh->GetPathName().Contains(TEXT("/Tripo/"));
	}

	bool IsBamboo(const FPiece& Piece)
	{
		return Piece.Name.Contains(TEXT("Bamboo"));
	}

	/** A torii is an arch: coins float under it (the posts stand in the 120 cm cell margin coins never use). */
	bool IsArchOverCoin(const FPiece& Art, const FPiece& Other)
	{
		return Art.Name.Contains(TEXT("Torii")) && Other.Name.StartsWith(TEXT("coin"));
	}

	/** Everything a prop must stay out of: visible art, visible colliding blocks (not the ground), coins. */
	TArray<FPiece> Gather(UWorld& World)
	{
		TArray<FPiece> Pieces;
		for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
		{
			UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
			if (It->IsHidden() || !Mesh->GetStaticMesh())
			{
				continue;
			}
			const FBox Box = Mesh->Bounds.GetBox();
			const bool bArt = IsArt(**It);
			const bool bSolid = Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision && Box.Max.Z > 1.0f;	// skip the ground slab
			if (bArt || bSolid)
			{
				Pieces.Add({ bArt ? Mesh->GetStaticMesh()->GetName() : FString::Printf(TEXT("block at %s"), *Box.GetCenter().ToCompactString()), Box, bArt });
			}
		}
		for (TActorIterator<AMatsuriCollectible> It(&World); It; ++It)
		{
			const UStaticMeshComponent* Coin = It->FindComponentByClass<UStaticMeshComponent>();	// not the pickup trigger
			Pieces.Add({ FString::Printf(TEXT("coin at %s"), *It->GetActorLocation().ToCompactString()), Coin->Bounds.GetBox(), false });
		}
		return Pieces;
	}
}

namespace
{
	/** Colliders added by the set dressing: solid actors that are not one of the layout's blocks. */
	TArray<FBox2D> DecorColliders(UWorld& World, const FMatsuriFloorLayout& Layout)
	{
		TArray<FBox2D> Result;
		for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
		{
			UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
			if (Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}
			const FVector Center = Mesh->Bounds.Origin;
			const bool bLayoutBlock = Layout.Blocks.ContainsByPredicate([&Center](const FMatsuriBlockPlacement& Block) { return Block.Center.Equals(Center, 2.0f); });
			if (!bLayoutBlock)
			{
				const FBox Box = Mesh->Bounds.GetBox();
				Result.Add(FBox2D(FVector2D(Box.Min), FVector2D(Box.Max)).ExpandBy(YokaiRadius));
			}
		}
		return Result;
	}

	bool SegmentHitsBox(const FVector2D& From, const FVector2D& To, const FBox2D& Box)
	{
		constexpr int32 Samples = 40;
		for (int32 i = 0; i <= Samples; ++i)
		{
			if (Box.IsInside(FMath::Lerp(From, To, float(i) / Samples)))
			{
				return true;
			}
		}
		return false;
	}

	/** Each cell's open doorways joined to each other and to the centre: the straight lines yokai take. */
	TArray<TPair<FVector2D, FVector2D>> YokaiPaths(const FMatsuriMaze& Maze)
	{
		TArray<TPair<FVector2D, FVector2D>> Paths;
		const float Half = Maze.CellSize() * 0.5f;
		const TPair<FMatsuriMaze::EWall, FVector2D> Sides[] = {
			MakeTuple(FMatsuriMaze::East, FVector2D(1, 0)), MakeTuple(FMatsuriMaze::North, FVector2D(0, 1)),
			MakeTuple(FMatsuriMaze::West, FVector2D(-1, 0)), MakeTuple(FMatsuriMaze::South, FVector2D(0, -1)) };
		for (int32 i = 0; i < Maze.CellCount(); ++i)
		{
			const FIntPoint Cell = Maze.CellOf(i);
			const FVector2D Center = Maze.CellCenter(Cell);
			TArray<FVector2D> Doors;
			for (const auto& Side : Sides)
			{
				if (!Maze.HasWall(Cell, Side.Key))
				{
					Doors.Add(Center + Side.Value * Half);
				}
			}
			for (int32 A = 0; A < Doors.Num(); ++A)
			{
				Paths.Add({ Doors[A], Center });
				for (int32 B = A + 1; B < Doors.Num(); ++B)
				{
					Paths.Add({ Doors[A], Doors[B] });
				}
			}
		}
		return Paths;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriArtPlacementTest, "SuzuneMatsuri.Art.NoClipping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriArtPlacementTest::RunTest(const FString&)
{
	if (!Matsuri::Art::Mesh(TEXT("SM_Prop_Chochin")))
	{
		AddInfo(TEXT("no Tripo art imported: nothing to check"));
		return true;
	}
	int32 Checked = 0;
	for (const int32 Seed : RunSeeds)
	{
		for (int32 FloorIndex = 0; FloorIndex < Matsuri::Floors::Count; ++FloorIndex)
		{
			UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);

			const FMatsuriFloorSpec Spec = Matsuri::Floors::SpecFor(FloorIndex, Seed);
			const FMatsuriFloorTheme Theme = Matsuri::Floors::ThemeFor(FloorIndex);
			const FMatsuriFloorLayout Layout = Matsuri::Layout::BuildFloorLayout(Spec);
			Matsuri::Builder::SpawnBlocks(*World, Layout, Theme);
			Matsuri::Builder::SpawnCollectibles(*World, Layout);
			Matsuri::Decor::DressFloor(*World, Layout, Spec, Theme, FloorIndex);

			const TArray<FPiece> Pieces = Gather(*World);
			for (int32 i = 0; i < Pieces.Num(); ++i)
			{
				if (!Pieces[i].bArt)
				{
					continue;
				}
				++Checked;
				const FBox Art = Pieces[i].Box.ExpandBy(-TouchTolerance);
				for (int32 j = 0; j < Pieces.Num(); ++j)
				{
					const bool bArtPair = Pieces[j].bArt;
					if (i == j || (bArtPair && j < i) || (IsBamboo(Pieces[i]) && IsBamboo(Pieces[j])) || IsArchOverCoin(Pieces[i], Pieces[j]))
					{
						continue;	// each art pair once; a bamboo grove is meant to crowd together
					}
					if (Art.Intersect(Pieces[j].Box.ExpandBy(-TouchTolerance)))
					{
						AddError(FString::Printf(TEXT("%dF seed %d: %s at %s clips into %s"), FloorIndex + 1, Seed,
							*Pieces[i].Name, *Pieces[i].Box.GetCenter().ToCompactString(), *Pieces[j].Name));
					}
				}
			}

			if (Layout.Maze.IsValid())
			{
				const TArray<FBox2D> Colliders = DecorColliders(*World, Layout);
				for (const TPair<FVector2D, FVector2D>& Path : YokaiPaths(Layout.Maze))
				{
					for (const FBox2D& Collider : Colliders)
					{
						if (SegmentHitsBox(Path.Key, Path.Value, Collider))
						{
							AddError(FString::Printf(TEXT("%dF seed %d: a dressing collider at %s blocks the yokai path %s -> %s"), FloorIndex + 1, Seed,
								*Collider.GetCenter().ToString(), *Path.Key.ToString(), *Path.Value.ToString()));
						}
					}
				}
			}

			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	}
	AddInfo(FString::Printf(TEXT("%d art placements checked"), Checked));
	TestTrue(TEXT("art was placed"), Checked > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
