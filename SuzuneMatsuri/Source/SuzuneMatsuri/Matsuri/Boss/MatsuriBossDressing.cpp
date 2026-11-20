// Suzune's Night Festival - 本殿・月下 dressed with the Mountain Temple pack (when it is in the project).

#include "MatsuriBossDressing.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/RandomStream.h"
#include "MatsuriArt.h"
#include "MatsuriBossArena.h"
#include "MatsuriFloorDecor.h"
#include "MatsuriShrineProps.h"
#include "SuzuneMatsuri.h"

namespace
{
	using namespace Matsuri::ShrineProps;

	constexpr int32 DressingSeed = 20261105;		// the courtyard is fixed, so is its dressing
	constexpr float LanternHeight = 300.0f;			// a little taller than its 280 cm cover box
	constexpr float StatueHeight = 420.0f;
	constexpr float HallYaw = 90.0f;				// the prefab faces -X; the honden faces the courtyard (-Y)
	constexpr float HallLampAcross = 212.0f;		// the prefab's two hanging lamps, in the honden's frame
	constexpr float HallLampOut = 504.0f;
	constexpr float HallLampUp = 330.0f;
	constexpr float HallLampIntensity = 3500.0f;
	constexpr float HallLampRadius = 1600.0f;
	constexpr float HallFootSpacing = 160.0f;
	constexpr float WallSkirtHeight = 300.0f;		// stone skirt along the bottom of the outer walls
	constexpr float WallSkirtProud = 16.0f;
	constexpr float WallBeamBand = 30.0f;			// dark beam bands
	constexpr float WallBandProud = 10.0f;
	constexpr float WallColumnSpacing = 600.0f;
	constexpr float WallColumnProud = 40.0f;		// the column's centre, in front of the wall face
	constexpr int32 GroundLeaves = 9000;			// fallen single ginkgo leaves, instanced
	constexpr int32 GroundLeafPatches = 330;
	const FLinearColor LampColor(1.0f, 0.62f, 0.32f);
	const FLinearColor SkirtTint(0.85f, 0.85f, 0.82f);
	const FLinearColor BandTint(0.05f, 0.035f, 0.025f);
	constexpr int32 SujiLines = 5;
	constexpr float SujiSpacing = 34.0f;
	constexpr float SujiThickness = 9.0f;
	const FLinearColor SujiWhite(0.92f, 0.90f, 0.84f);
	constexpr float OuterTreeDistance = 350.0f;		// beyond the outer wall: close, so the canopies hang over it
	constexpr int32 TreesPerSide = 4;
	constexpr int32 GroundScatter = 140;
	constexpr float ScatterKeepClear = 180.0f;		// from ammo, hooks' landing spots, the start
	constexpr float WallBand = 220.0f;				// grass and stones gather along the walls

	float FaceCentre(const FVector& At, const FVector& Centre)
	{
		return (Centre - At).Rotation().Yaw;
	}

	/** Hides the gray box of the named solid; its collision stays. */
	void HideSolid(UWorld& World, FName Name)
	{
		for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
		{
			if (It->Tags.Contains(Name))
			{
				It->SetActorHiddenInGame(true);
			}
		}
	}

	int32 DressSolids(UWorld& World, const FMatsuriBossArena& Arena)
	{
		int32 Placed = 0;
		for (const FMatsuriArenaBox& Solid : Arena.Solids)
		{
			const FString Name = Solid.Name.ToString();
			const FVector Base(Solid.Center.X, Solid.Center.Y, Solid.Center.Z - Solid.Extent.Z);
			const float Yaw = FaceCentre(Base, Arena.Center);
			if (Name.StartsWith(TEXT("lantern")) && Place(World, TEXT("SM_Lantern_01"), Base, Yaw, FVector(ScaleToHeight(TEXT("SM_Lantern_01"), LanternHeight))))
			{
				HideSolid(World, Solid.Name);
				++Placed;
			}
			else if (Name.StartsWith(TEXT("statue")) && Place(World, TEXT("SM_Statue_01"), Base, Yaw, FVector(ScaleToHeight(TEXT("SM_Statue_01"), StatueHeight))))
			{
				HideSolid(World, Solid.Name);
				++Placed;
			}
		}
		return Placed;
	}

	/** Clear of ammo and the start; bSolids: also outside every solid (with a margin). */
	bool KeepsClear(const FMatsuriBossArena& Arena, const FVector& Spot, bool bSolids = true)
	{
		auto Near = [&Spot](const FVector& Other) { return FVector::Dist2D(Spot, Other) < ScatterKeepClear; };
		if (Arena.AmmoSpots.ContainsByPredicate(Near) || Near(Arena.PlayerStart))
		{
			return false;
		}
		for (const FMatsuriArenaBox& Solid : Arena.Solids)
		{
			if (bSolids && Solid.Bounds().ExpandBy(FVector(40.0f, 40.0f, 0.0f)).IsInsideXY(Spot))
			{
				return false;
			}
		}
		return true;
	}

	void SpawnLamp(UWorld& World, const FVector& At, float Intensity, float Radius)
	{
		APointLight* Light = World.SpawnActor<APointLight>(At, FRotator::ZeroRotator);
		if (!Light)
		{
			return;
		}
		UPointLightComponent* Component = CastChecked<UPointLightComponent>(Light->GetLightComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetIntensity(Intensity);
		Component->SetAttenuationRadius(Radius);
		Component->SetLightColor(LampColor);
		Component->SetCastShadows(false);
	}

	/**
	 *  The pack's own temple hall on the honden, whose box becomes its stone foundation: the hall's floor
	 *  is the honden top, so the perch up there is its veranda. Rocks and grass gather at the foundation's foot.
	 */
	int32 DressHall(UWorld& World, const FMatsuriBossArena& Arena, FRandomStream& Random)
	{
		const FMatsuriArenaBox* Hall = Arena.Solids.FindByPredicate([](const FMatsuriArenaBox& Box) { return Box.Name == TEXT("honden"); });
		if (!Hall)
		{
			return 0;
		}
		const FVector Floor(Hall->Center.X, Hall->Center.Y, Hall->Center.Z + Hall->Extent.Z);
		if (!PlaceTempleHall(World, Floor, HallYaw))
		{
			return 0;
		}
		int32 Placed = 1;
		// The two lamps hanging at the hall's front carry real light.
		for (const float Side : { -1.0f, 1.0f })
		{
			SpawnLamp(World, Floor + FVector(Side * HallLampAcross, -HallLampOut, HallLampUp), HallLampIntensity, HallLampRadius);
		}
		const float Front = Hall->Center.Y - Hall->Extent.Y;
		const TCHAR* const Foot[] = { TEXT("SM_Rock_02"), TEXT("SM_Stone_01"), TEXT("SM_Grass_03"), TEXT("SM_Grass_01") };
		for (float X = -Hall->Extent.X; X <= Hall->Extent.X; X += HallFootSpacing)
		{
			const TCHAR* Piece = Foot[Random.RandRange(0, UE_ARRAY_COUNT(Foot) - 1)];
			const FVector Spot(Hall->Center.X + X + Random.FRandRange(-60.0f, 60.0f), Front - Random.FRandRange(20.0f, 70.0f), Arena.GroundZ);
			Placed += KeepsClear(Arena, Spot, false) && Place(World, Piece, Spot, Random.FRandRange(0.0f, 360.0f), FVector(Random.FRandRange(0.9f, 1.5f))) ? 1 : 0;
		}
		return Placed;
	}

	/**
	 *  The courtyard's outer walls as a temple's: a stone skirt, carved columns at a steady rhythm, dark
	 *  beam bands and a tiled roof along the top. Visual only; the wall boxes keep the collision.
	 */
	int32 DressWalls(UWorld& World, const FMatsuriBossArena& Arena)
	{
		int32 Placed = 0;
		const float ColumnHeight = FMath::Max(Size(TEXT("SM_Support_Column_01")).Z, 1.0f);
		for (const FMatsuriArenaBox& Wall : Arena.Solids)
		{
			if (Wall.Piece != EMatsuriArenaPiece::Wall)
			{
				continue;
			}
			const bool bAlongX = Wall.Extent.X >= Wall.Extent.Y;
			const float Length = (bAlongX ? Wall.Extent.X : Wall.Extent.Y) * 2.0f;
			const float Thick = (bAlongX ? Wall.Extent.Y : Wall.Extent.X) * 2.0f;
			const float Top = Wall.Center.Z + Wall.Extent.Z;
			// Unit vector from the wall into the courtyard, and the inner face at ground level.
			const FVector Inward = bAlongX ? FVector(0.0f, -FMath::Sign(Wall.Center.Y - Arena.Center.Y), 0.0f)
				: FVector(-FMath::Sign(Wall.Center.X - Arena.Center.X), 0.0f, 0.0f);
			const FVector Face = FVector(Wall.Center.X, Wall.Center.Y, Arena.GroundZ) + Inward * Thick * 0.5f;
			auto Along = [bAlongX](float Long, float Across, float Up) { return bAlongX ? FVector(Long, Across, Up) : FVector(Across, Long, Up); };

			Matsuri::Decor::PlaceWallRoof(World, FVector2D(Wall.Center.X, Wall.Center.Y), Length, Thick, Top, bAlongX);
			Matsuri::Decor::PlaceTrim(World, Face + Inward * WallSkirtProud * 0.5f + FVector(0.0f, 0.0f, WallSkirtHeight * 0.5f),
				Along(Length, WallSkirtProud, WallSkirtHeight), Matsuri::Graybox::ESurface::Stone, SkirtTint);
			for (const float BandZ : { WallSkirtHeight + WallBeamBand * 0.5f, Top - WallBeamBand * 1.5f })
			{
				Matsuri::Decor::PlaceTrim(World, Face + Inward * WallBandProud * 0.5f + FVector(0.0f, 0.0f, BandZ),
					Along(Length, WallBandProud, WallBeamBand), {}, BandTint);
			}
			// 筋塀: the five white lines of a temple of the highest rank, under the roof.
			for (int32 Line = 0; Line < SujiLines; ++Line)
			{
				const float LineZ = Top - WallBeamBand * 3.0f - Line * SujiSpacing;
				Matsuri::Decor::PlaceTrim(World, Face + Inward * WallBandProud * 0.5f + FVector(0.0f, 0.0f, LineZ),
					Along(Length, WallBandProud, SujiThickness), {}, SujiWhite);
			}
			const int32 Columns = FMath::FloorToInt32(Length / WallColumnSpacing);
			for (int32 i = 1; i < Columns; ++i)
			{
				const float Offset = -Length * 0.5f + i * (Length / Columns);
				const FVector Base = Face + Inward * WallColumnProud + (bAlongX ? FVector(Offset, 0.0f, 0.0f) : FVector(0.0f, Offset, 0.0f));
				Placed += Place(World, TEXT("SM_Support_Column_01"), Base, 0.0f, FVector(1.0f, 1.0f, (Top - Arena.GroundZ) / ColumnHeight)) ? 1 : 0;
			}
		}
		return Placed;
	}

	/** The shimenawa hanging from the torii beam, a signboard by the entrance. */
	int32 DressTorii(UWorld& World, const FMatsuriBossArena& Arena)
	{
		const FMatsuriArenaBox* Beam = Arena.Solids.FindByPredicate([](const FMatsuriArenaBox& Box) { return Box.Name == TEXT("torii_beam"); });
		if (!Beam)
		{
			return 0;
		}
		int32 Placed = 0;
		const float Under = Beam->Center.Z - Beam->Extent.Z;
		for (float X = -500.0f; X <= 500.0f; X += 500.0f)
		{
			const float RopeHeight = Size(TEXT("SM_Rope_Decor_01")).Z * 1.6f;
			Placed += Place(World, TEXT("SM_Rope_Decor_01"), FVector(Beam->Center.X + X, Beam->Center.Y, Under - RopeHeight), 0.0f, FVector(1.6f)) ? 1 : 0;
		}
		Placed += Place(World, TEXT("SM_Sign_01"), FVector(Arena.PlayerStart.X + 520.0f, Arena.PlayerStart.Y + 150.0f, 0.0f), 200.0f, FVector(1.0f)) ? 1 : 0;
		return Placed;
	}

	/** Ginkgo trees and rocks beyond the outer wall: the silhouette above the fence. */
	int32 DressBeyond(UWorld& World, const FMatsuriBossArena& Arena, FRandomStream& Random)
	{
		const TCHAR* const Trees[] = { TEXT("SM_Ginkgo_Tree_01"), TEXT("SM_Ginkgo_Tree_02") };
		int32 Placed = 0;
		const float Out = Arena.HalfExtent + OuterTreeDistance;
		for (int32 Side = 0; Side < 4; ++Side)
		{
			for (int32 i = 0; i < TreesPerSide; ++i)
			{
				const float Along = FMath::Lerp(-Arena.HalfExtent, Arena.HalfExtent, (i + 0.5f) / TreesPerSide) + Random.FRandRange(-300.0f, 300.0f);
				const float Away = Out + Random.FRandRange(0.0f, 600.0f);
				const FVector2D Spot = Side == 0 ? FVector2D(Along, Away) : Side == 1 ? FVector2D(Along, -Away) : Side == 2 ? FVector2D(Away, Along) : FVector2D(-Away, Along);
				const TCHAR* Tree = Trees[Random.RandRange(0, 1)];
				Placed += Place(World, Tree, FVector(Arena.Center.X + Spot.X, Arena.Center.Y + Spot.Y, Arena.GroundZ), Random.FRandRange(0.0f, 360.0f),
					FVector(Random.FRandRange(1.4f, 1.9f))) ? 1 : 0;	// first screenshots: at 1x they barely topped the walls
			}
			// One big rock per side, half hidden by the wall.
			const FVector2D Rock = Side == 0 ? FVector2D(-1800.0f, Out) : Side == 1 ? FVector2D(1600.0f, -Out) : Side == 2 ? FVector2D(Out, 1500.0f) : FVector2D(-Out, -1700.0f);
			Placed += Place(World, TEXT("SM_Rock_01"), FVector(Rock.X, Rock.Y, Arena.GroundZ - 150.0f), Random.FRandRange(0.0f, 360.0f), FVector(0.8f)) ? 1 : 0;
		}
		return Placed;
	}

	/** Leaf piles across the paving; grass and stones along the walls and platform feet. */
	int32 DressGround(UWorld& World, const FMatsuriBossArena& Arena, FRandomStream& Random)
	{
		const TCHAR* const Leaves[] = { TEXT("SM_Ginkgo_Leaves_01"), TEXT("SM_Ginkgo_Leaves_02"), TEXT("SM_Ginkgo_Leaves_03") };
		const TCHAR* const Edge[] = { TEXT("SM_Grass_01"), TEXT("SM_Grass_03"), TEXT("SM_Stone_01"), TEXT("SM_Stone_02"), TEXT("SM_Rock_02"), TEXT("SM_Flower_01") };
		int32 Placed = 0;
		for (int32 i = 0; i < GroundScatter; ++i)
		{
			const bool bEdge = i % 2 != 0;
			FVector Spot;
			if (bEdge)
			{
				// Along a random outer wall, inside the courtyard.
				const float Along = Random.FRandRange(-Arena.HalfExtent, Arena.HalfExtent);
				const float In = Arena.HalfExtent - Random.FRandRange(30.0f, WallBand);
				const int32 Side = Random.RandRange(0, 3);
				Spot = Side == 0 ? FVector(Along, In, 0) : Side == 1 ? FVector(Along, -In, 0) : Side == 2 ? FVector(In, Along, 0) : FVector(-In, Along, 0);
			}
			else
			{
				Spot = FVector(Random.FRandRange(-Arena.HalfExtent, Arena.HalfExtent), Random.FRandRange(-Arena.HalfExtent, Arena.HalfExtent), 0.0f);
			}
			Spot += FVector(Arena.Center.X, Arena.Center.Y, Arena.GroundZ);
			if (!KeepsClear(Arena, Spot))
			{
				continue;
			}
			const TCHAR* Piece = bEdge ? Edge[Random.RandRange(0, UE_ARRAY_COUNT(Edge) - 1)] : Leaves[Random.RandRange(0, UE_ARRAY_COUNT(Leaves) - 1)];
			const float Scale = bEdge ? Random.FRandRange(0.8f, 1.3f) : Random.FRandRange(1.6f, 2.6f);	// leaves as carpets, not scraps
			Placed += Place(World, Piece, Spot, Random.FRandRange(0.0f, 360.0f), FVector(Scale), bEdge) ? 1 : 0;	// flat leaves cast no shadow
		}
		// A carpet of single fallen leaves and leaf patches over the whole paving (instanced: cheap).
		const float H = Arena.HalfExtent;
		TArray<FTransform> Singles;
		for (int32 i = 0; i < GroundLeaves; ++i)
		{
			// Denser toward the walls: half the leaves lie in the outer quarter.
			const float Depth = Random.FRand() < 0.5f ? Random.FRandRange(0.75f, 1.0f) : Random.FRandRange(0.0f, 1.0f);
			const float Along = Random.FRandRange(-H, H);
			const float Across = Depth * H * (Random.FRand() < 0.5f ? -1.0f : 1.0f);
			const FVector2D Spot = Random.FRand() < 0.5f ? FVector2D(Along, Across) : FVector2D(Across, Along);
			Singles.Add(FTransform(FRotator(Random.FRandRange(-12.0f, 12.0f), Random.FRandRange(0.0f, 360.0f), Random.FRandRange(-12.0f, 12.0f)),
				FVector(Arena.Center.X + Spot.X, Arena.Center.Y + Spot.Y, Arena.GroundZ + 1.0f), FVector(Random.FRandRange(0.8f, 1.5f))));
		}
		Placed += Scatter(World, TEXT("SM_Ginkgo_Leaf_Single_01"), Singles) ? 1 : 0;
		for (const TCHAR* Patch : Leaves)
		{
			TArray<FTransform> Copies;
			for (int32 i = 0; i < GroundLeafPatches / int32(UE_ARRAY_COUNT(Leaves)); ++i)
			{
				Copies.Add(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
					FVector(Arena.Center.X + Random.FRandRange(-H, H), Arena.Center.Y + Random.FRandRange(-H, H), Arena.GroundZ + 2.0f),
					FVector(Random.FRandRange(1.0f, 2.0f))));
			}
			Placed += Scatter(World, Patch, Copies) ? 1 : 0;
		}
		return Placed;
	}

	void Atmosphere(UWorld& World, const FMatsuriBossArena& Arena)
	{
		const float H = Arena.HalfExtent * 0.6f;
		const FVector Spots[] = { FVector(-H, H, 0), FVector(H, H, 0), FVector(-H, -H, 0), FVector(H, -H, 0) };
		for (const FVector& Spot : Spots)
		{
			Effect(World, TEXT("NS_Fireflies"), Arena.Center + Spot * 0.8f + FVector(0, 0, 150.0f));
		}
		for (const FMatsuriArenaBox& Solid : Arena.Solids)
		{
			if (Solid.Name.ToString().StartsWith(TEXT("lantern")) || Solid.Name == TEXT("honden"))
			{
				Effect(World, TEXT("NS_Falling_Leaves"), FVector(Solid.Center.X, Solid.Center.Y - Solid.Extent.Y - 150.0f, 650.0f));
			}
		}
	}
}

int32 Matsuri::BossArena::Dress(UWorld& World, const FMatsuriBossArena& Arena)
{
	if (!Matsuri::Art::HasShrinePack())
	{
		return 0;
	}
	FRandomStream Random(DressingSeed);
	int32 Placed = DressSolids(World, Arena);
	Placed += DressHall(World, Arena, Random);
	Placed += DressWalls(World, Arena);
	Placed += DressTorii(World, Arena);
	Placed += DressBeyond(World, Arena, Random);
	Placed += DressGround(World, Arena, Random);
	Atmosphere(World, Arena);
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Boss courtyard dressed with the Mountain Temple pack: %d pieces"), Placed);
	return Placed;
}
