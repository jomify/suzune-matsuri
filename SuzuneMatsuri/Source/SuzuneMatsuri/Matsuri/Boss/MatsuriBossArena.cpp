// Suzune's Night Festival - 本殿・月下: the boss courtyard, as data (layout) and as actors (build).

#include "MatsuriBossArena.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "MatsuriAmmoPickup.h"
#include "MatsuriArenaPieces.h"
#include "MatsuriBossDressing.h"
#include "MatsuriFloorBuilder.h"
#include "MatsuriFloors.h"
#include "MatsuriGraybox.h"

namespace
{
	constexpr float CubeSize = 100.0f;
	constexpr int32 AmmoPerSpot = 5;
	constexpr float ZoneHeightTolerance = 250.0f;
	constexpr float RampWidth = 320.0f;
	constexpr float RampThickness = 40.0f;
	const FLinearColor WallColor(1.0f, 0.84f, 0.60f);	// ochre earthen plaster
	const FLinearColor StoneColor(0.55f, 0.54f, 0.52f);
	const FLinearColor HallColor(0.62f, 0.12f, 0.06f);
	const FLinearColor LanternLight(1.0f, 0.55f, 0.2f);

	FMatsuriArenaBox Box(FName Name, EMatsuriArenaPiece Piece, const FVector& Center, const FVector& Extent)
	{
		FMatsuriArenaBox Out;
		Out.Name = Name;
		Out.Piece = Piece;
		Out.Center = Center;
		Out.Extent = Extent;
		return Out;
	}

	FMatsuriArenaZone Zone(FName Name, const FVector& Point, float Radius, bool bHigh)
	{
		FMatsuriArenaZone Out;
		Out.Name = Name;
		Out.Point = Point;
		Out.Radius = Radius;
		Out.bHigh = bHigh;
		return Out;
	}

	/** A walkable ramp from a platform's edge (Top, at its height) down to the ground at Bottom. */
	void SpawnRamp(UWorld& World, const FVector& Top, const FVector& Bottom)
	{
		const FVector Span = Top - Bottom;
		const FVector Mid = (Top + Bottom) * 0.5f - FVector(0.0f, 0.0f, RampThickness * 0.5f);
		const FRotator Slope = Span.Rotation();
		const FTransform Transform(Slope, Mid, FVector(Span.Size(), RampWidth, RampThickness) / CubeSize);
		AStaticMeshActor* Ramp = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		UStaticMeshComponent* Mesh = Ramp->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cube));
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Ramp->FinishSpawning(Transform);
		Matsuri::Graybox::PaintSurface(Mesh, Matsuri::Graybox::ESurface::Stone, StoneColor);
	}

	void SpawnSolid(UWorld& World, const FMatsuriArenaBox& Solid)
	{
		const FTransform Transform(FRotator::ZeroRotator, Solid.Center, Solid.Extent * 2.0f / CubeSize);
		if (Solid.Piece == EMatsuriArenaPiece::Fence)
		{
			AMatsuriBreakableCover* Fence = World.SpawnActorDeferred<AMatsuriBreakableCover>(AMatsuriBreakableCover::StaticClass(), FTransform(Solid.Center));
			Fence->SetExtent(Solid.Extent);
			Fence->FinishSpawning(FTransform(Solid.Center));
			return;
		}
		AStaticMeshActor* Actor = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cube));
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Actor->Tags.Add(Solid.Name);
		Actor->FinishSpawning(Transform);
		// The honden box is the hall's stone foundation (the hall itself is set dressing on top of it).
		const bool bWood = Solid.Piece == EMatsuriArenaPiece::Wall || Solid.Name.ToString().StartsWith(TEXT("torii"));
		const Matsuri::Graybox::ESurface Surface = Solid.Piece == EMatsuriArenaPiece::Wall ? Matsuri::Graybox::ESurface::Plaster
			: bWood ? Matsuri::Graybox::ESurface::Wood : Matsuri::Graybox::ESurface::Stone;
		Matsuri::Graybox::PaintSurface(Mesh, Surface,
			Solid.Piece == EMatsuriArenaPiece::Wall ? WallColor : bWood ? HallColor : StoneColor);
	}

	void SpawnLight(UWorld& World, const FVector& At, float Intensity, float Radius)
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
		Component->SetLightColor(LanternLight);
		Component->SetCastShadows(false);
	}
}

FMatsuriBossArena Matsuri::BossArena::Layout()
{
	FMatsuriBossArena Arena;
	const float H = Arena.HalfExtent;
	TArray<FMatsuriArenaBox>& S = Arena.Solids;

	// Outer fence: bounds the fight (bullets past it are gone anyway).
	S.Add(Box(TEXT("wall_n"), EMatsuriArenaPiece::Wall, FVector(0, H + 50, 600), FVector(H + 100, 50, 600)));
	S.Add(Box(TEXT("wall_s"), EMatsuriArenaPiece::Wall, FVector(0, -H - 50, 600), FVector(H + 100, 50, 600)));
	S.Add(Box(TEXT("wall_e"), EMatsuriArenaPiece::Wall, FVector(H + 50, 0, 600), FVector(50, H + 100, 600)));
	S.Add(Box(TEXT("wall_w"), EMatsuriArenaPiece::Wall, FVector(-H - 50, 0, 600), FVector(50, H + 100, 600)));

	// 本殿: tall cover at the north; its roof is the highest perch.
	S.Add(Box(TEXT("honden"), EMatsuriArenaPiece::Cover, FVector(0, 2450, 450), FVector(1000, 350, 450)));
	// Stone platforms (4 m) with ramps toward the middle.
	S.Add(Box(TEXT("platform_w"), EMatsuriArenaPiece::Platform, FVector(-1700, 300, 200), FVector(500, 500, 200)));
	S.Add(Box(TEXT("platform_e"), EMatsuriArenaPiece::Platform, FVector(1700, 300, 200), FVector(500, 500, 200)));
	// 櫓 towers (7 m) in the south corners: grapple only.
	S.Add(Box(TEXT("tower_w"), EMatsuriArenaPiece::Platform, FVector(-2400, -2400, 350), FVector(220, 220, 350)));
	S.Add(Box(TEXT("tower_e"), EMatsuriArenaPiece::Platform, FVector(2400, -2400, 350), FVector(220, 220, 350)));
	// The great torii: posts are cover, the beam is a perch.
	S.Add(Box(TEXT("torii_post_w"), EMatsuriArenaPiece::Cover, FVector(-650, -1900, 700), FVector(60, 60, 700)));
	S.Add(Box(TEXT("torii_post_e"), EMatsuriArenaPiece::Cover, FVector(650, -1900, 700), FVector(60, 60, 700)));
	S.Add(Box(TEXT("torii_beam"), EMatsuriArenaPiece::Platform, FVector(0, -1900, 1450), FVector(850, 90, 50)));
	// Stone lanterns and fox statues: cover that never breaks.
	S.Add(Box(TEXT("lantern_sw"), EMatsuriArenaPiece::Cover, FVector(-900, -700, 140), FVector(60, 60, 140)));
	S.Add(Box(TEXT("lantern_se"), EMatsuriArenaPiece::Cover, FVector(900, -700, 140), FVector(60, 60, 140)));
	S.Add(Box(TEXT("lantern_nw"), EMatsuriArenaPiece::Cover, FVector(-900, 1300, 140), FVector(60, 60, 140)));
	S.Add(Box(TEXT("lantern_ne"), EMatsuriArenaPiece::Cover, FVector(900, 1300, 140), FVector(60, 60, 140)));
	S.Add(Box(TEXT("statue_w"), EMatsuriArenaPiece::Cover, FVector(-2300, 1700, 160), FVector(140, 140, 160)));
	S.Add(Box(TEXT("statue_e"), EMatsuriArenaPiece::Cover, FVector(2300, 1700, 160), FVector(140, 140, 160)));
	// 玉垣 fences: break after enough bullets, grow back.
	S.Add(Box(TEXT("fence_s"), EMatsuriArenaPiece::Fence, FVector(0, -1000, 90), FVector(320, 25, 90)));
	S.Add(Box(TEXT("fence_n"), EMatsuriArenaPiece::Fence, FVector(0, 800, 90), FVector(320, 25, 90)));
	S.Add(Box(TEXT("fence_w"), EMatsuriArenaPiece::Fence, FVector(-1900, -1200, 90), FVector(25, 320, 90)));
	S.Add(Box(TEXT("fence_e"), EMatsuriArenaPiece::Fence, FVector(1900, -1200, 90), FVector(25, 320, 90)));

	// Zones: where the player can be, and where her rain can fall. Cover zones sit on the ground behind it.
	TArray<FMatsuriArenaZone>& Z = Arena.Zones;
	Z.Add(Zone(TEXT("center"), FVector(0, 0, 0), 800.0f, false));
	Z.Add(Zone(TEXT("honden_front"), FVector(0, 1750, 0), 600.0f, false));
	Z.Add(Zone(TEXT("lantern_sw"), FVector(-900, -700, 0), 450.0f, false));
	Z.Add(Zone(TEXT("lantern_se"), FVector(900, -700, 0), 450.0f, false));
	Z.Add(Zone(TEXT("lantern_nw"), FVector(-900, 1300, 0), 450.0f, false));
	Z.Add(Zone(TEXT("lantern_ne"), FVector(900, 1300, 0), 450.0f, false));
	Z.Add(Zone(TEXT("fence_s"), FVector(0, -1000, 0), 450.0f, false));
	Z.Add(Zone(TEXT("fence_n"), FVector(0, 800, 0), 450.0f, false));
	Z.Add(Zone(TEXT("fence_w"), FVector(-1900, -1200, 0), 450.0f, false));
	Z.Add(Zone(TEXT("fence_e"), FVector(1900, -1200, 0), 450.0f, false));
	Z.Add(Zone(TEXT("statue_w"), FVector(-2300, 1700, 0), 500.0f, false));
	Z.Add(Zone(TEXT("statue_e"), FVector(2300, 1700, 0), 500.0f, false));
	Z.Add(Zone(TEXT("platform_w"), FVector(-1700, 300, 400), 550.0f, true));
	Z.Add(Zone(TEXT("platform_e"), FVector(1700, 300, 400), 550.0f, true));
	Z.Add(Zone(TEXT("tower_w"), FVector(-2400, -2400, 700), 320.0f, true));
	Z.Add(Zone(TEXT("tower_e"), FVector(2400, -2400, 700), 320.0f, true));
	Z.Add(Zone(TEXT("torii_beam"), FVector(0, -1900, 1500), 650.0f, true));
	Z.Add(Zone(TEXT("honden_roof"), FVector(0, 2450, 900), 800.0f, true));

	// Grapple rings above every perch.
	Arena.HookPoints = {
		FVector(-1700, 300, 750), FVector(1700, 300, 750),
		FVector(-2400, -2400, 950), FVector(2400, -2400, 950),
		FVector(0, -1900, 1700), FVector(0, 2450, 1150) };

	Arena.AmmoSpots = {
		FVector(0, 0, 0), FVector(-500, -1500, 0), FVector(500, -1500, 0),
		FVector(-1700, 300, 400), FVector(1700, 300, 400),
		FVector(0, -1900, 1500), FVector(-2400, -2400, 700) };

	Arena.Anchors = {
		FVector(0, 1600, 750), FVector(-1500, 1700, 850), FVector(1500, 1700, 850),
		FVector(0, 200, 1150), FVector(-2200, -200, 950), FVector(2200, -200, 950) };

	Arena.PlayerStart = FVector(0, -2600, 110);
	Arena.PlayerStartRotation = FRotator(0.0f, 90.0f, 0.0f);
	return Arena;
}

int32 Matsuri::BossArena::ZoneAt(const FMatsuriBossArena& Arena, const FVector& Point)
{
	int32 Best = -1;
	float BestDistance = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Arena.Zones.Num(); ++i)
	{
		const FMatsuriArenaZone& Candidate = Arena.Zones[i];
		const float Flat = FVector::Dist2D(Point, Candidate.Point);
		const float Rise = Point.Z - Candidate.Point.Z;	// feet height over the zone's floor
		if (Flat <= Candidate.Radius && Rise > -ZoneHeightTolerance * 0.4f && Rise < ZoneHeightTolerance && Flat < BestDistance)
		{
			Best = i;
			BestDistance = Flat;
		}
	}
	return Best;
}

bool Matsuri::BossArena::IsShielded(const FMatsuriBossArena& Arena, const FVector& From, const FVector& To)
{
	const FVector Segment = To - From;
	for (const FMatsuriArenaBox& Solid : Arena.Solids)
	{
		if (Solid.Piece != EMatsuriArenaPiece::Fence && FMath::LineBoxIntersection(Solid.Bounds(), From, To, Segment))
		{
			return true;
		}
	}
	return false;
}

void Matsuri::BossArena::Build(UWorld& World, const FMatsuriBossArena& Arena, const FMatsuriFloorTheme& Theme)
{
	Matsuri::Builder::SpawnEnvironment(World, Theme);

	// The courtyard's paving.
	FMatsuriArenaBox Ground = Box(TEXT("ground"), EMatsuriArenaPiece::Cover, Arena.Center + FVector(0, 0, Arena.GroundZ - 50.0f),
		FVector(Arena.HalfExtent + 200.0f, Arena.HalfExtent + 200.0f, 50.0f));
	{
		const FTransform Transform(FRotator::ZeroRotator, Ground.Center, Ground.Extent * 2.0f / CubeSize);
		AStaticMeshActor* Actor = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cube));
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Actor->FinishSpawning(Transform);
		Matsuri::Graybox::PaintSurface(Mesh, Matsuri::Graybox::ESurface::Paving, FLinearColor(0.8f, 0.8f, 0.85f));
	}

	for (const FMatsuriArenaBox& Solid : Arena.Solids)
	{
		SpawnSolid(World, Solid);
	}
	// Ramps from the platforms' inner edges down toward the middle.
	SpawnRamp(World, FVector(-1200, 300, 400), FVector(-400, 300, 0));
	SpawnRamp(World, FVector(1200, 300, 400), FVector(400, 300, 0));

	for (const FVector& Hook : Arena.HookPoints)
	{
		World.SpawnActor<AMatsuriHookPoint>(Hook, FRotator::ZeroRotator);
	}
	for (const FVector& Spot : Arena.AmmoSpots)
	{
		const FTransform Transform(Spot + FVector(0.0f, 0.0f, 70.0f));
		AMatsuriAmmoPickup* Ammo = World.SpawnActorDeferred<AMatsuriAmmoPickup>(AMatsuriAmmoPickup::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Ammo->Configure(AmmoPerSpot, true);
		Ammo->FinishSpawning(Transform);
	}

	// Lamps in the stone lanterns and under the hall's eaves.
	for (const FMatsuriArenaBox& Solid : Arena.Solids)
	{
		if (Solid.Name.ToString().StartsWith(TEXT("lantern")))
		{
			SpawnLight(World, Solid.Center + FVector(0, 0, Solid.Extent.Z + 40.0f), Theme.LanternIntensity, 1400.0f);
		}
	}
	SpawnLight(World, FVector(-700, 2050, 700), Theme.LanternIntensity, 1800.0f);
	SpawnLight(World, FVector(700, 2050, 700), Theme.LanternIntensity, 1800.0f);
	Matsuri::BossArena::Dress(World, Arena);	// the Mountain Temple pack, when it is in the project
}
