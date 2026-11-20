// Suzune's Night Festival - deterministic gray-box floor layout (pure, unit tested).

#pragma once

#include "CoreMinimal.h"
#include "MatsuriMaze.h"
#include "MatsuriTypes.h"
#include "MatsuriFloorLayout.generated.h"

/** Tunables for one generated floor. All distances in cm. */
USTRUCT(BlueprintType)
struct SUZUNEMATSURI_API FMatsuriFloorSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Layout")
	int32 Seed = 20261028;

	/** Half the side length of the square play area. */
	UPROPERTY(EditAnywhere, Category="Layout", meta=(ClampMin=1000))
	float HalfExtent = 2400.0f;

	UPROPERTY(EditAnywhere, Category="Layout", meta=(ClampMin=0))
	int32 ObstacleCount = 22;

	UPROPERTY(EditAnywhere, Category="Layout", meta=(ClampMin=0))
	int32 KobanCount = 14;

	UPROPERTY(EditAnywhere, Category="Layout", meta=(ClampMin=0))
	int32 GinkaCount = 6;

	UPROPERTY(EditAnywhere, Category="Layout", meta=(ClampMin=0))
	int32 MagatamaCount = 2;

	/** Score needed to clear the floor. */
	UPROPERTY(EditAnywhere, Category="Layout", meta=(ClampMin=1))
	int32 TargetScore = 15;

	UPROPERTY(EditAnywhere, Category="Layout")
	float WallHeight = 450.0f;

	/** Countdown for the floor; reaching zero ends the run. */
	UPROPERTY(EditAnywhere, Category="Rules", meta=(ClampMin=10))
	float TimeLimitSeconds = 120.0f;

	UPROPERTY(EditAnywhere, Category="Threats", meta=(ClampMin=0))
	int32 ChochinCount = 3;

	UPROPERTY(EditAnywhere, Category="Threats", meta=(ClampMin=0))
	int32 KasaCount = 2;

	UPROPERTY(EditAnywhere, Category="Threats", meta=(ClampMin=0))
	int32 KappaCount = 1;

	UPROPERTY(EditAnywhere, Category="Threats", meta=(ClampMin=0))
	int32 FoxfireCount = 1;

	UPROPERTY(EditAnywhere, Category="Threats", meta=(ClampMin=0))
	int32 SpikeCount = 2;

	UPROPERTY(EditAnywhere, Category="Pickups", meta=(ClampMin=0))
	int32 AmmoSpotCount = 6;

	/** Maze floor (corridors, crates, lanterns) instead of the open test arena with scattered obstacles. */
	UPROPERTY(EditAnywhere, Category="Maze")
	bool bMaze = false;

	UPROPERTY(EditAnywhere, Category="Maze", meta=(ClampMin=3, EditCondition="bMaze"))
	int32 MazeCells = 8;

	/** Chance to knock out each extra inner wall: higher = more loops to escape around. */
	UPROPERTY(EditAnywhere, Category="Maze", meta=(ClampMin=0, ClampMax=1, EditCondition="bMaze"))
	float LoopChance = 0.2f;

	/** 2x2 open rooms for fights. */
	UPROPERTY(EditAnywhere, Category="Maze", meta=(ClampMin=0, EditCondition="bMaze"))
	int32 Plazas = 2;

	UPROPERTY(EditAnywhere, Category="Maze", meta=(ClampMin=0, ClampMax=1, EditCondition="bMaze"))
	float CoverChance = 0.3f;

	UPROPERTY(EditAnywhere, Category="Maze", meta=(ClampMin=0, ClampMax=1, EditCondition="bMaze"))
	float PillarChance = 0.3f;
};

/** Things placed on free floor space besides collectibles. */
enum class EMatsuriSpawn : uint8
{
	Chochin,	// lantern ghost, patrols and drifts toward the player
	Kasa,		// umbrella yokai, hops after the player
	Kappa,		// ranged thrower
	Foxfire,	// invulnerable floating flame
	Spike,		// invulnerable timed floor spikes
	Ammo		// respawning ammo talisman
};

struct FMatsuriSpawnPlacement
{
	EMatsuriSpawn Type = EMatsuriSpawn::Ammo;
	FVector Location = FVector::ZeroVector;
};

enum class EMatsuriBlock : uint8
{
	Ground,
	Wall,
	LowCover,	// waist-high crates / stalls, can be jumped on
	Pillar		// tall stone lanterns / pillars
};

struct FMatsuriBlockPlacement
{
	EMatsuriBlock Type = EMatsuriBlock::Ground;
	FVector Center = FVector::ZeroVector;
	FVector Size = FVector::ZeroVector;

	FBox2D GetFootprint() const;
};

struct FMatsuriCoinPlacement
{
	EMatsuriCollectible Kind = EMatsuriCollectible::Koban;
	FVector Location = FVector::ZeroVector;
};

struct FMatsuriFloorLayout
{
	TArray<FMatsuriBlockPlacement> Blocks;
	TArray<FMatsuriCoinPlacement> Coins;
	TArray<FMatsuriSpawnPlacement> Spawns;
	FVector PlayerStart = FVector::ZeroVector;
	FRotator PlayerStartRotation = FRotator::ZeroRotator;

	/** Valid only on maze floors; yokai path along it. */
	FMatsuriMaze Maze;
	FIntPoint StartCell = FIntPoint::ZeroValue;
	FIntPoint ExitCell = FIntPoint::ZeroValue;

	int32 CountCoins(EMatsuriCollectible Kind) const;
	int32 CountSpawns(EMatsuriSpawn Type) const;
	int32 TotalPoints() const;
};

namespace Matsuri::Layout
{
	/** Minimum clear gap between obstacles and between obstacles and walls (always walkable). */
	constexpr float ObstacleGap = 160.0f;
	/** Coins keep this far from any obstacle footprint. */
	constexpr float CoinClearance = 70.0f;
	/** Coins keep this far from each other. */
	constexpr float CoinSpacing = 150.0f;
	/** Nothing but the player spawns within this radius of the start. */
	constexpr float SpawnClearRadius = 400.0f;
	/** Height of coin centers above the ground. */
	constexpr float CoinHeight = 90.0f;
	/** Enemies and hazards start at least this far from the player (no spawn hits). */
	constexpr float ThreatClearRadius = 1000.0f;
	/** Spawns keep this far from each other and from coins. */
	constexpr float SpawnSpacing = 200.0f;
	/** Maze floors: threats start at least this many cells away along the corridors. */
	constexpr int32 ThreatMinCells = 3;
	/** Maze walls (cm). */
	constexpr float MazeWallThickness = 60.0f;
	/** Maze floors: coins and spawns keep this far inside a cell's walls. */
	constexpr float CellMargin = 120.0f;

	/**
	 *  Builds a floor from the spec. Same seed -> same layout.
	 *  Obstacles are axis-aligned and never touch each other or the walls,
	 *  so the free space stays connected and every coin is reachable.
	 */
	SUZUNEMATSURI_API FMatsuriFloorLayout BuildFloorLayout(const FMatsuriFloorSpec& Spec);
}
