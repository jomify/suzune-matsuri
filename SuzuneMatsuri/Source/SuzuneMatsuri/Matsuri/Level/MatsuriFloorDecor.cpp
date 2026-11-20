// Suzune's Night Festival - set dressing: Tripo meshes where they fit, engine shapes otherwise.

#include "MatsuriFloorDecor.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/RandomStream.h"
#include "MatsuriArt.h"
#include "MatsuriGraybox.h"
#include "MatsuriShrineProps.h"

using Matsuri::Graybox::EShape;

namespace
{
	constexpr float ShapeSize = 100.0f;		// engine basic shapes are 1 m

	// Torii proportions (cm).
	constexpr float ToriiWidth = 560.0f;
	constexpr float ToriiHeight = 520.0f;
	constexpr float ToriiPostDiameter = 64.0f;
	constexpr float ToriiStartAhead = 260.0f;	// in front of the player start
	const FLinearColor ToriiRed(0.75f, 0.08f, 0.03f);
	const FLinearColor LacquerBlack(0.03f, 0.025f, 0.02f);

	// Lantern strings along the fence.
	constexpr float LanternSpacing = 400.0f;
	constexpr float LanternInset = 70.0f;
	constexpr float LanternHeight = 330.0f;
	constexpr float LanternLightRadius = 950.0f;
	constexpr float LampLightRadius = 750.0f;

	// Wall trims.
	constexpr float WallCapHeight = 14.0f;
	constexpr float WallCapOverhang = 8.0f;
	// The tiled roof on top of every wall (瓦屋根 of a temple wall): two slopes and a ridge.
	constexpr float RoofPitch = 24.0f;				// degrees
	constexpr float RoofEave = 32.0f;				// overhang past each face
	constexpr float RoofSlab = 7.0f;				// tile thickness
	constexpr float RoofRidge = 16.0f;
	const FLinearColor RoofTint(0.78f, 0.76f, 0.74f);
	constexpr float WainscotHeight = 110.0f;		// 腰板 above the stone footing
	constexpr float WainscotProud = 3.0f;
	constexpr float WainscotRail = 8.0f;
	const FLinearColor WainscotTint(0.55f, 0.42f, 0.34f);
	constexpr float WallBaseHeight = 38.0f;
	constexpr float WallBaseProud = 5.0f;			// how far the footing stands out from the wall face
	const FLinearColor StoneGrey(0.62f, 0.60f, 0.57f);

	// Tripo art (sizes as normalised by tools/blender/normalize_matsuri.py).
	constexpr float ToriiArtHeight = 540.0f;
	constexpr float StoneLanternArtHeight = 330.0f;
	constexpr float StoneLanternFireHeight = 0.62f;	// fire box, as a fraction of the lantern's height
	constexpr float PillarArtMinFill = 0.8f;		// a stone lantern must be >= 80 % as tall as the pillar it replaces
	constexpr float ApproachLanternScale = 0.45f;	// ~1.5 m stone lanterns flanking the start and exit
	constexpr float StallMaxAspect = 1.6f;			// longer crates keep the awning look
	constexpr float StatueMinPlinth = 110.0f;
	constexpr float StatueScale = 0.7f;
	constexpr float BambooArtSpacing = 420.0f;
	constexpr float BambooFenceGap = 60.0f;			// between the fence's outer face and a clump
	constexpr float BambooArtBand = 300.0f;

	// Placement rules (playtest: props poked through walls).
	constexpr float TouchTolerance = 6.0f;			// resting on / standing against is fine; sinking in is not
	constexpr float WallClearance = 20.0f;			// art fitted into a maze cell keeps this far from the walls
	constexpr float CoinHalfSize = 35.0f;
	constexpr float SpawnHalfSize = 60.0f;

	// The Mountain Temple pack (Fab, licensed per user; only when it is in the project).
	constexpr int32 PackSeedSalt = 0x51A6E;
	constexpr float PackWallDressChance = 0.8f;		// share of maze walls that get grass, stones or leaves at their foot
	constexpr float PackCellTreeChance = 0.15f;		// maze cells with a ginkgo in a corner (2F, 3F)
	constexpr float PackCellTreeHeight = 850.0f;	// crowns well over the 450 cm walls
	constexpr float PackCellTreeInset = 15.0f;		// trunk clearance from the walls
	constexpr float PackTrunkHalf = 45.0f;
	// Fallen ginkgo leaves (instanced, no collision): single leaves and leaf patches, per floor.
	constexpr int32 PackLeaves[3] = { 2500, 6000, 7000 };
	constexpr int32 PackLeafPatches[3] = { 80, 220, 260 };
	constexpr float PackLeafWallShare = 0.6f;		// share of leaves drifted against a wall
	constexpr float PackLeafDrift = 140.0f;			// how far from the wall face they lie
	constexpr float PackWallGap = 4.0f;				// between a wall's face and what lies against it
	constexpr float PackTreeSpacing = 1500.0f;		// ginkgo trees beyond the fence
	constexpr float PackTreeBand = 900.0f;
	constexpr int32 PackEffectsPerFloor = 4;

	/**
	 *  What a floor's set dressing must stay out of: solid layout blocks, coins, spawn points and the
	 *  art already placed. Only active while DressFloor runs (the title stage places art freely).
	 */
	struct FOccupancy
	{
		TArray<FBox> Blocks;	// index-aligned with FMatsuriFloorLayout::Blocks (ground: an empty box)
		TArray<FBox> Points;	// coins and spawns
		TArray<FBox> Art;

		bool Fits(const FBox& Box, int32 IgnoreBlock, bool bIgnorePoints) const
		{
			const FBox Inner = Box.ExpandBy(-TouchTolerance);
			auto Clear = [&Inner](const FBox& Other) { return !Other.IsValid || !Inner.Intersect(Other.ExpandBy(-TouchTolerance)); };
			for (int32 i = 0; i < Blocks.Num(); ++i)
			{
				if (i != IgnoreBlock && !Clear(Blocks[i]))
				{
					return false;
				}
			}
			for (const FBox& Point : Points)
			{
				if (!bIgnorePoints && !Clear(Point))
				{
					return false;
				}
			}
			for (const FBox& Other : Art)
			{
				if (!Clear(Other))
				{
					return false;
				}
			}
			return true;
		}
	};

	FOccupancy* GOccupancy = nullptr;	// set by DressFloor for the duration of one floor

	struct FFitRule
	{
		int32 IgnoreBlock = INDEX_NONE;	// the block this art stands in for
		bool bIgnorePoints = false;		// arches (torii) span coins without touching them
	};

	enum class EArt : uint8
	{
		Placed,
		Missing,	// no art imported: use the gray-box shape
		Blocked		// would clip: leave it out
	};

	struct FShape
	{
		EShape Shape = EShape::Cube;
		FVector Center = FVector::ZeroVector;
		FVector Size = FVector(100.0f);	// cm
		FRotator Rotation = FRotator::ZeroRotator;
		FLinearColor Color = FLinearColor::Gray;
		float Glow = 0.0f;				// > 0 uses the emissive material
		bool bCollides = false;
		bool bVisible = true;			// false: an invisible collider standing in for an art mesh
		TOptional<Matsuri::Graybox::ESurface> Surface;	// a textured surface instead of a flat colour
		bool bCastShadow = true;		// off for trims flush with a wall (long non-Nanite boxes overflow the VSM marking queue)
	};

	void Spawn(UWorld& World, const FShape& Part)
	{
		const FTransform Transform(Part.Rotation, Part.Center, Part.Size / ShapeSize);
		AStaticMeshActor* Actor = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Matsuri::Graybox::Mesh(Part.Shape));
		Mesh->SetCollisionProfileName(Part.bCollides ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		Mesh->SetCastShadow(Part.bCastShadow && Part.Glow <= 0.0f);
		Actor->FinishSpawning(Transform);
		if (Part.Surface.IsSet())
		{
			Matsuri::Graybox::PaintSurface(Mesh, Part.Surface.GetValue(), Part.Color);
		}
		else
		{
			Matsuri::Graybox::Paint(Mesh, Part.Color, Part.Glow > 0.0f, Part.Glow);
		}
		Actor->SetActorHiddenInGame(!Part.bVisible);
	}

	/** World bounds the art mesh would take up at this transform. */
	FBox ArtBounds(const UStaticMesh& Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
	{
		return Mesh.GetBoundingBox().TransformBy(FTransform(Rotation, Location, Scale));
	}

	/** Places a Tripo mesh (pivot at its base or centre, as normalised) if it fits; see EArt. */
	EArt SpawnArt(UWorld& World, const TCHAR* Name, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FFitRule& Rule = {})
	{
		UStaticMesh* Mesh = Matsuri::Art::Mesh(Name);
		if (!Mesh)
		{
			return EArt::Missing;
		}
		const FBox Bounds = ArtBounds(*Mesh, Location, Rotation, Scale);
		if (GOccupancy && !GOccupancy->Fits(Bounds, Rule.IgnoreBlock, Rule.bIgnorePoints))
		{
			return EArt::Blocked;
		}
		const FTransform Transform(Rotation, Location, Scale);
		AStaticMeshActor* Actor = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);	// gameplay keeps its own blocks
		Actor->FinishSpawning(Transform);
		if (GOccupancy)
		{
			GOccupancy->Art.Add(Bounds);
		}
		return EArt::Placed;
	}

	/** SpawnArt for the Mountain Temple pack: the piece's bounds must fit like any other art. */
	EArt SpawnPack(UWorld& World, const TCHAR* Name, const FVector& Base, float Yaw, const FVector& Scale, const FFitRule& Rule = {}, bool bCastShadow = true)
	{
		FBox Bounds;
		if (!Matsuri::ShrineProps::Bounds(Name, Base, Yaw, Scale, Bounds))
		{
			return EArt::Missing;
		}
		if (GOccupancy && !GOccupancy->Fits(Bounds, Rule.IgnoreBlock, Rule.bIgnorePoints))
		{
			return EArt::Blocked;
		}
		Matsuri::ShrineProps::Place(World, Name, Base, Yaw, Scale, bCastShadow);
		if (GOccupancy)
		{
			GOccupancy->Art.Add(Bounds);
		}
		return EArt::Placed;
	}

	/** Hides the gray-box block an art mesh now stands for; its collision stays. */
	void HideBlockAt(UWorld& World, const FMatsuriBlockPlacement& Block)
	{
		for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
		{
			if (It->GetActorLocation().Equals(Block.Center, 1.0f) && It->GetActorScale3D().Equals(Block.Size / ShapeSize, 0.01f))
			{
				It->SetActorHiddenInGame(true);
				return;
			}
		}
	}

	void SpawnLight(UWorld& World, const FVector& Location, const FLinearColor& Color, float Intensity, float Radius)
	{
		APointLight* Light = World.SpawnActorDeferred<APointLight>(APointLight::StaticClass(), FTransform(Location));
		UPointLightComponent* Component = CastChecked<UPointLightComponent>(Light->GetLightComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetIntensity(Intensity);
		Component->SetLightColor(Color);
		Component->SetAttenuationRadius(Radius);
		Component->SetCastShadows(false);
		Light->FinishSpawning(FTransform(Location));
	}

	/**
	 *  A torii standing at Base, its opening facing along X. MaxWidth (if > 0) shrinks the Tripo torii
	 *  to fit between maze walls; the colliding posts follow the art's width.
	 */
	void SpawnTorii(UWorld& World, const FVector& Base, float Scale, float MaxWidth = 0.0f)
	{
		float ArtScale = Scale * ToriiHeight / ToriiArtHeight;
		if (const UStaticMesh* Mesh = Matsuri::Art::Mesh(TEXT("SM_Prop_Torii")); Mesh && MaxWidth > 0.0f)
		{
			ArtScale = FMath::Min(ArtScale, MaxWidth / Mesh->GetBoundingBox().GetSize().Y);
		}
		const EArt Art = SpawnArt(World, TEXT("SM_Prop_Torii"), Base, FRotator::ZeroRotator, FVector(ArtScale), { INDEX_NONE, true });
		const bool bArt = Art == EArt::Placed;
		if (Art == EArt::Blocked)
		{
			return;	// no room for a gate here
		}
		const float FitScale = bArt ? ArtScale * ToriiArtHeight / ToriiHeight : Scale;
		const float Width = ToriiWidth * FitScale * (bArt ? 0.82f : 1.0f);	// the art's posts stand inside its overhanging beams
		const float Height = ToriiHeight * FitScale;
		const float Post = ToriiPostDiameter * FitScale;
		for (const float Side : { -0.5f, 0.5f })
		{
			Spawn(World, { EShape::Cylinder, Base + FVector(0.0f, Side * Width, Height * 0.5f), FVector(Post, Post, Height), FRotator::ZeroRotator, ToriiRed, 0.0f, true, !bArt });
		}
		if (bArt)
		{
			return;
		}
		Spawn(World, { EShape::Cube, Base + FVector(0.0f, 0.0f, Height * 0.80f), FVector(Post * 0.7f, Width + Post * 1.6f, Post * 0.7f), FRotator::ZeroRotator, ToriiRed });		// nuki
		Spawn(World, { EShape::Cube, Base + FVector(0.0f, 0.0f, Height * 1.00f), FVector(Post * 1.2f, Width + Post * 4.0f, Post * 0.8f), FRotator::ZeroRotator, ToriiRed });		// shimaki
		Spawn(World, { EShape::Cube, Base + FVector(0.0f, 0.0f, Height * 1.09f), FVector(Post * 1.4f, Width + Post * 5.0f, Post * 0.6f), FRotator::ZeroRotator, LacquerBlack });	// kasagi
		Spawn(World, { EShape::Cube, Base + FVector(0.0f, 0.0f, Height * 0.90f), FVector(Post * 0.5f, Post * 1.2f, Height * 0.14f), FRotator::ZeroRotator, LacquerBlack });		// gakuzuka plaque
	}

	/** A paper chochin: glowing body with dark caps, optionally with a real light. Left out where it would clip. */
	void SpawnLantern(UWorld& World, const FVector& Center, const FLinearColor& Color, float LightIntensity)
	{
		const EArt Art = SpawnArt(World, TEXT("SM_Prop_Chochin"), Center, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), FVector(1.0f));
		if (Art == EArt::Blocked)
		{
			return;
		}
		if (Art == EArt::Missing)
		{
			Spawn(World, { EShape::Sphere, Center, FVector(36.0f, 36.0f, 46.0f), FRotator::ZeroRotator, Color, 9.0f });
			Spawn(World, { EShape::Cylinder, Center + FVector(0.0f, 0.0f, 25.0f), FVector(22.0f, 22.0f, 6.0f), FRotator::ZeroRotator, LacquerBlack });
			Spawn(World, { EShape::Cylinder, Center - FVector(0.0f, 0.0f, 25.0f), FVector(22.0f, 22.0f, 6.0f), FRotator::ZeroRotator, LacquerBlack });
		}
		if (LightIntensity > 0.0f)
		{
			SpawnLight(World, Center - FVector(0.0f, 0.0f, 40.0f), Color, LightIntensity, LanternLightRadius);
		}
	}

	/** A ~1.5 m stone lantern with its own collider, for the approach to the start and exit gates. */
	void SpawnApproachLantern(UWorld& World, const FVector& Base, const FMatsuriFloorTheme& Theme)
	{
		if (SpawnArt(World, TEXT("SM_Prop_StoneLantern"), Base, FRotator::ZeroRotator, FVector(ApproachLanternScale)) != EArt::Placed)
		{
			return;
		}
		const FBox Box = GOccupancy ? GOccupancy->Art.Last() : FBox(Base, Base);
		Spawn(World, { EShape::Cube, Box.GetCenter(), Box.GetSize(), FRotator::ZeroRotator, FLinearColor::Gray, 0.0f, true, false });
		SpawnLight(World, Base + FVector(0.0f, 0.0f, StoneLanternArtHeight * ApproachLanternScale * StoneLanternFireHeight),
			Theme.LanternColor, Theme.LanternIntensity * 0.4f, LampLightRadius);
	}

	/** Height of the tiled roof SpawnWallRoof puts on a wall Thick cm thick (eaves to ridge top). */
	float WallRoofRise(float Thick)
	{
		return (Thick * 0.5f + RoofEave) * FMath::Tan(FMath::DegreesToRadians(RoofPitch)) + RoofRidge;
	}

	/** A gabled tiled roof along a wall top: two sloped slabs from the ridge out past both faces, and the ridge. */
	void SpawnWallRoof(UWorld& World, const FVector2D& Center, float Length, float Thick, float WallTop, bool bAlongX)
	{
		const float Half = Thick * 0.5f + RoofEave;
		const float Slope = FMath::DegreesToRadians(RoofPitch);
		const float Width = Half / FMath::Cos(Slope);
		const float Rise = Half * FMath::Tan(Slope);
		for (const float Side : { -1.0f, 1.0f })
		{
			// The slab's middle sits halfway down the slope, Side of the ridge line.
			const FVector Across = bAlongX ? FVector(0.0f, Side * Half * 0.5f, 0.0f) : FVector(Side * Half * 0.5f, 0.0f, 0.0f);
			const FVector Middle = FVector(Center, WallTop + Rise * 0.5f + RoofSlab * 0.5f) + Across;
			const FRotator Tilt = bAlongX ? FRotator(0.0f, 0.0f, Side * RoofPitch) : FRotator(-Side * RoofPitch, 0.0f, 0.0f);
			FShape Slab{ EShape::Cube, Middle, bAlongX ? FVector(Length + RoofEave, Width, RoofSlab) : FVector(Width, Length + RoofEave, RoofSlab), Tilt, RoofTint };
			Slab.Surface = Matsuri::Graybox::ESurface::Roof;
			Spawn(World, Slab);
		}
		FShape Ridge{ EShape::Cube, FVector(Center, WallTop + Rise + RoofRidge * 0.5f),
			bAlongX ? FVector(Length + RoofEave, RoofRidge, RoofRidge) : FVector(RoofRidge, Length + RoofEave, RoofRidge), FRotator::ZeroRotator, RoofTint * 0.8f };
		Ridge.Surface = Matsuri::Graybox::ESurface::Roof;
		Spawn(World, Ridge);
	}

	void DressFence(UWorld& World, const FMatsuriFloorSpec& Spec, const FMatsuriFloorTheme& Theme)
	{
		const float H = Spec.HalfExtent;
		const float Inner = H - LanternInset;
		const int32 PerSide = FMath::FloorToInt32(2.0f * H / LanternSpacing);
		const FVector2D Axes[4][2] = {	// start corner and direction for each side
			{ { -H, -Inner }, { 1.0f, 0.0f } }, { { -H, Inner }, { 1.0f, 0.0f } },
			{ { -Inner, -H }, { 0.0f, 1.0f } }, { { Inner, -H }, { 0.0f, 1.0f } } };

		for (const auto& Side : Axes)
		{
			for (int32 i = 1; i < PerSide; ++i)
			{
				const FVector2D Spot = Side[0] + Side[1] * (i * LanternSpacing);
				const bool bLit = i % 2 == 0;	// every other lantern carries a real light
				SpawnLantern(World, FVector(Spot, LanternHeight), Theme.LanternColor, bLit ? Theme.LanternIntensity : 0.0f);
			}
		}

		// Granite footing along the inside of the fence.
		const float FootSpan = 2.0f * H;
		for (const FVector& Center : { FVector(H - WallBaseProud * 0.5f, 0, WallBaseHeight * 0.5f), FVector(-H + WallBaseProud * 0.5f, 0, WallBaseHeight * 0.5f) })
		{
			FShape Foot{ EShape::Cube, Center, FVector(WallBaseProud * 2.0f, FootSpan, WallBaseHeight), FRotator::ZeroRotator, StoneGrey };
			Foot.Surface = Matsuri::Graybox::ESurface::Stone;
			Foot.bCastShadow = false;
			Spawn(World, Foot);
		}
		for (const FVector& Center : { FVector(0, H - WallBaseProud * 0.5f, WallBaseHeight * 0.5f), FVector(0, -H + WallBaseProud * 0.5f, WallBaseHeight * 0.5f) })
		{
			FShape Foot{ EShape::Cube, Center, FVector(FootSpan, WallBaseProud * 2.0f, WallBaseHeight), FRotator::ZeroRotator, StoneGrey };
			Foot.Surface = Matsuri::Graybox::ESurface::Stone;
			Foot.bCastShadow = false;
			Spawn(World, Foot);
		}

		// Dark cap along the top of the fence.
		const float Span = 2.0f * (H + 100.0f);
		const float Top = Spec.WallHeight + 15.0f;
		const float Offset = H + 50.0f;
		for (const FVector& Center : { FVector(Offset, 0, Top), FVector(-Offset, 0, Top) })
		{
			Spawn(World, { EShape::Cube, Center, FVector(140.0f, Span, 30.0f), FRotator::ZeroRotator, LacquerBlack });
			SpawnWallRoof(World, FVector2D(Center), Span, 140.0f, Top + 15.0f, false);
		}
		for (const FVector& Center : { FVector(0, Offset, Top), FVector(0, -Offset, Top) })
		{
			Spawn(World, { EShape::Cube, Center, FVector(Span, 140.0f, 30.0f), FRotator::ZeroRotator, LacquerBlack });
			SpawnWallRoof(World, FVector2D(Center), Span, 140.0f, Top + 15.0f, true);
		}
	}

	/** A Tripo stone lantern replacing a free-standing pillar, if one fits its footprint and nearly its height. */
	bool DressPillarArt(UWorld& World, const FMatsuriBlockPlacement& Block, int32 BlockIndex, const FMatsuriFloorTheme& Theme)
	{
		// The pack's carved stone lantern, when it fills the pillar as well as the Tripo one must.
		const FVector PackSize = Matsuri::ShrineProps::Size(TEXT("SM_Lantern_01"));
		if (!PackSize.IsNearlyZero())
		{
			const float HeightScale = float(Block.Size.Z / PackSize.Z);
			const float Fit = FMath::Min3(HeightScale, float(Block.Size.X / PackSize.X), float(Block.Size.Y / PackSize.Y));
			const FVector Base(Block.Center.X, Block.Center.Y, 0.0f);
			if (Fit >= HeightScale * PillarArtMinFill && SpawnPack(World, TEXT("SM_Lantern_01"), Base, 0.0f, FVector(Fit), { BlockIndex, false }) == EArt::Placed)
			{
				SpawnLight(World, Base + FVector(0.0f, 0.0f, PackSize.Z * Fit * StoneLanternFireHeight), Theme.LanternColor, Theme.LanternIntensity * 0.6f, LampLightRadius);
				return true;
			}
		}
		const UStaticMesh* Mesh = Matsuri::Art::Mesh(TEXT("SM_Prop_StoneLantern"));
		if (!Mesh)
		{
			return false;
		}
		const FVector ArtSize = Mesh->GetBoundingBox().GetSize();
		const float HeightScale = float(Block.Size.Z / ArtSize.Z);
		const float Scale = FMath::Min3(HeightScale, float(Block.Size.X / ArtSize.X), float(Block.Size.Y / ArtSize.Y));
		if (Scale < HeightScale * PillarArtMinFill)
		{
			return false;	// too slender a pillar: a squat lantern would leave invisible collision above it
		}
		const FVector Base(Block.Center.X, Block.Center.Y, 0.0f);
		if (SpawnArt(World, TEXT("SM_Prop_StoneLantern"), Base, FRotator::ZeroRotator, FVector(Scale), { BlockIndex, false }) != EArt::Placed)
		{
			return false;
		}
		SpawnLight(World, Base + FVector(0.0f, 0.0f, ArtSize.Z * Scale * StoneLanternFireHeight), Theme.LanternColor, Theme.LanternIntensity * 0.6f, LampLightRadius);
		return true;
	}

	/**
	 *  2F: a yatai stall standing on a roughly square crate (its counter is the cover), replacing the crate.
	 *  3F: a kitsune statue on top of a crate, which stays as its plinth. Returns true when the crate was replaced.
	 */
	bool DressCoverArt(UWorld& World, const FMatsuriBlockPlacement& Block, int32 BlockIndex, int32 FloorIndex)
	{
		const float Short = FMath::Min(Block.Size.X, Block.Size.Y);
		const float Long = FMath::Max(Block.Size.X, Block.Size.Y);
		const float Top = Block.Center.Z + Block.Size.Z * 0.5f;
		const FVector Base(Block.Center.X, Block.Center.Y, 0.0f);
		if (FloorIndex == 1 && Long <= Short * StallMaxAspect)
		{
			if (const UStaticMesh* Mesh = Matsuri::Art::Mesh(TEXT("SM_Prop_Yatai")))
			{
				// The stall's long side (Y in the model) follows the crate's long side; it fits inside the crate's footprint.
				const FVector ArtSize = Mesh->GetBoundingBox().GetSize();
				const float Yaw = (Block.Size.X > Block.Size.Y ? 90.0f : 0.0f) + 180.0f * (BlockIndex % 2);
				const float Scale = FMath::Min(Short / ArtSize.X, Long / ArtSize.Y);
				return SpawnArt(World, TEXT("SM_Prop_Yatai"), Base, FRotator(0.0f, Yaw, 0.0f), FVector(Scale), { BlockIndex, false }) == EArt::Placed;
			}
		}
		if (FloorIndex == 2 && Short >= StatueMinPlinth)
		{
			SpawnArt(World, TEXT("SM_Prop_KitsuneStatue"), FVector(Base.X, Base.Y, Top), FRotator(0.0f, 90.0f * (BlockIndex % 4), 0.0f), FVector(StatueScale));
		}
		return false;
	}

	/** Stone lanterns get a lit fire box on top; 2F stalls get a striped awning. Art replaces them where it fits. */
	void DressBlocks(UWorld& World, const FMatsuriFloorLayout& Layout, const FMatsuriFloorTheme& Theme, int32 FloorIndex)
	{
		int32 StallIndex = 0;
		for (int32 Index = 0; Index < Layout.Blocks.Num(); ++Index)
		{
			const FMatsuriBlockPlacement& Block = Layout.Blocks[Index];
			const float Top = Block.Center.Z + Block.Size.Z * 0.5f;
			if ((Block.Type == EMatsuriBlock::Pillar && DressPillarArt(World, Block, Index, Theme))
				|| (Block.Type == EMatsuriBlock::LowCover && DressCoverArt(World, Block, Index, FloorIndex)))
			{
				HideBlockAt(World, Block);
				continue;
			}
			if (Block.Type == EMatsuriBlock::Pillar)
			{
				const FVector Lamp(Block.Center.X, Block.Center.Y, Top + 22.0f);
				Spawn(World, { EShape::Cube, Lamp, FVector(Block.Size.X * 0.75f, Block.Size.Y * 0.75f, 40.0f), FRotator::ZeroRotator, Theme.LanternColor, 7.0f });
				Spawn(World, { EShape::Cone, Lamp + FVector(0, 0, 45.0f), FVector(Block.Size.X * 1.3f, Block.Size.Y * 1.3f, 50.0f), FRotator::ZeroRotator, Theme.PillarColor * 0.6f });
				SpawnLight(World, Lamp, Theme.LanternColor, Theme.LanternIntensity * 0.6f, LampLightRadius);
			}
			else if (Block.Type == EMatsuriBlock::LowCover && FloorIndex == 1)
			{
				const FLinearColor Stripe = (StallIndex++ % 2 == 0) ? FLinearColor(0.85f, 0.10f, 0.08f) : FLinearColor(0.92f, 0.90f, 0.85f);
				const FVector Awning(Block.Center.X, Block.Center.Y, Top + 110.0f);
				Spawn(World, { EShape::Cube, Awning, FVector(Block.Size.X + 50.0f, Block.Size.Y + 50.0f, 14.0f), FRotator(0.0f, 0.0f, 4.0f), Stripe });
				SpawnLantern(World, Awning - FVector(0.0f, 0.0f, 45.0f), Theme.LanternColor, 0.0f);
			}
		}
	}

	/** Maze walls carry a lantern on every other segment (a real light on every third), so deep corridors are lit. */
	/**
	 *  Every wall gets a dark lacquered beam along its top and a granite footing along its base, so walls
	 *  read as built shrine fences rather than bare boxes (visual only: the block keeps the collision).
	 */
	void TrimWall(UWorld& World, const FMatsuriBlockPlacement& Block, float WallHeight)
	{
		const bool bAlongX = Block.Size.X >= Block.Size.Y;
		const float Length = bAlongX ? Block.Size.X : Block.Size.Y;
		const float Thick = bAlongX ? Block.Size.Y : Block.Size.X;
		auto Sized = [bAlongX](float Along, float Across, float Up) { return bAlongX ? FVector(Along, Across, Up) : FVector(Across, Along, Up); };
		Spawn(World, { EShape::Cube, FVector(Block.Center.X, Block.Center.Y, WallHeight + WallCapHeight * 0.5f),
			Sized(Length + WallCapOverhang, Thick + 2.0f * WallCapOverhang, WallCapHeight), FRotator::ZeroRotator, LacquerBlack });
		SpawnWallRoof(World, FVector2D(Block.Center.X, Block.Center.Y), Length, Thick, WallHeight + WallCapHeight, bAlongX);
		FShape Base{ EShape::Cube, FVector(Block.Center.X, Block.Center.Y, WallBaseHeight * 0.5f),
			Sized(Length, Thick + 2.0f * WallBaseProud, WallBaseHeight), FRotator::ZeroRotator, StoneGrey };
		Base.Surface = Matsuri::Graybox::ESurface::Stone;
		Base.bCastShadow = false;
		Spawn(World, Base);
		// 腰板: dark wooden boards along the lower part of the earthen wall, and a rail on top of them.
		FShape Wainscot{ EShape::Cube, FVector(Block.Center.X, Block.Center.Y, WallBaseHeight + WainscotHeight * 0.5f),
			Sized(Length, Thick + 2.0f * WainscotProud, WainscotHeight), FRotator::ZeroRotator, WainscotTint };
		Wainscot.Surface = Matsuri::Graybox::ESurface::Wood;
		Wainscot.bCastShadow = false;
		Spawn(World, Wainscot);
		FShape Rail{ EShape::Cube, FVector(Block.Center.X, Block.Center.Y, WallBaseHeight + WainscotHeight + WainscotRail * 0.5f),
			Sized(Length, Thick + 2.0f * WainscotProud + 4.0f, WainscotRail), FRotator::ZeroRotator, LacquerBlack };
		Rail.bCastShadow = false;
		Spawn(World, Rail);
	}

	void DressMazeWalls(UWorld& World, const FMatsuriFloorLayout& Layout, const FMatsuriFloorSpec& Spec, const FMatsuriFloorTheme& Theme)
	{
		int32 Segment = 0;
		for (const FMatsuriBlockPlacement& Block : Layout.Blocks)
		{
			const bool bMazeWall = Block.Type == EMatsuriBlock::Wall && FMath::Min(Block.Size.X, Block.Size.Y) < 80.0f;	// fence is thicker
			if (bMazeWall)
			{
				TrimWall(World, Block, Spec.WallHeight);
			}
			if (!bMazeWall || (Segment++ % 2) != 0)
			{
				continue;
			}
			const float Ridge = WallRoofRise(FMath::Min(Block.Size.X, Block.Size.Y));
			const FVector Top(Block.Center.X, Block.Center.Y, Spec.WallHeight + WallCapHeight + Ridge + 36.0f);	// sits just clear of the ridge
			const bool bLit = (Segment % 3) == 0;
			SpawnLantern(World, Top, Theme.LanternColor, bLit ? Theme.LanternIntensity * 0.8f : 0.0f);
		}
	}

	/**
	 *  Pairs of small stone lanterns in the fence-side corners of the start cell (west) and the exit cell (east).
	 *  No corridor path enters those cells from the fence, so yokai cutting across a cell never meet them
	 *  (in any other corner they blocked the diagonal from one doorway to the next).
	 */
	void DressApproach(UWorld& World, const FMatsuriFloorLayout& Layout, const FMatsuriFloorTheme& Theme)
	{
		const UStaticMesh* Mesh = Matsuri::Art::Mesh(TEXT("SM_Prop_StoneLantern"));
		if (!Mesh || !Layout.Maze.IsValid())
		{
			return;
		}
		const FVector Half = Mesh->GetBoundingBox().GetExtent() * ApproachLanternScale;
		const float Inset = Layout.Maze.CellSize() * 0.5f - Matsuri::Layout::MazeWallThickness * 0.5f - WallClearance;
		const TPair<FIntPoint, float> Corners[] = { { Layout.StartCell, -1.0f }, { Layout.ExitCell, 1.0f } };	// -1: west corners
		for (const TPair<FIntPoint, float>& Corner : Corners)
		{
			const FVector2D Center = Layout.Maze.CellCenter(Corner.Key);
			for (const float Side : { -1.0f, 1.0f })
			{
				SpawnApproachLantern(World, FVector(Center.X + Corner.Value * (Inset - Half.X), Center.Y + Side * (Inset - Half.Y), 0.0f), Theme);
			}
		}
	}

	/** Outer face of the shrine fence (the thickest walls), from the layout. */
	float FenceOuter(const FMatsuriFloorLayout& Layout, float HalfExtent)
	{
		float Outer = HalfExtent;
		for (const FMatsuriBlockPlacement& Block : Layout.Blocks)
		{
			if (Block.Type == EMatsuriBlock::Wall && FMath::Min(Block.Size.X, Block.Size.Y) >= 80.0f)
			{
				Outer = FMath::Max(Outer, FMath::Min(FMath::Abs(Block.Center.X) + Block.Size.X * 0.5f, FMath::Abs(Block.Center.Y) + Block.Size.Y * 0.5f));
			}
		}
		return Outer;
	}

	/** Tripo bamboo clumps in a band behind the fence; each clump's own size keeps it clear of the fence. */
	void DressBambooArt(UWorld& World, const UStaticMesh& Mesh, const FMatsuriFloorLayout& Layout, const FMatsuriFloorSpec& Spec, int32 Seed)
	{
		FRandomStream Rng(Seed);
		const float Outer = FenceOuter(Layout, Spec.HalfExtent);
		const FVector Extent = Mesh.GetBoundingBox().GetExtent();
		const int32 PerSide = FMath::CeilToInt32(2.0f * (Outer + BambooArtBand) / BambooArtSpacing);
		for (int32 Side = 0; Side < 4; ++Side)
		{
			for (int32 i = 0; i <= PerSide; ++i)
			{
				const float Scale = Rng.FRandRange(0.9f, 1.5f);
				const int32 Quarter = Rng.RandRange(0, 3);	// quarter turns keep the bounds exact
				const bool bAlongX = Side < 2;	// sides 0/1 sit beyond the fence on X, sides 2/3 on Y
				const float Depth = (bAlongX == (Quarter % 2 == 0) ? Extent.X : Extent.Y) * Scale;	// half-depth towards the fence
				const float Along = -Outer - BambooArtBand + i * BambooArtSpacing + Rng.FRandRange(-120.0f, 120.0f);
				const float Out = Outer + BambooFenceGap + Depth + Rng.FRandRange(0.0f, BambooArtBand);
				const FVector2D Spot = Side == 0 ? FVector2D(Out, Along) : Side == 1 ? FVector2D(-Out, Along) : Side == 2 ? FVector2D(Along, Out) : FVector2D(Along, -Out);
				SpawnArt(World, TEXT("SM_Prop_Bamboo"), FVector(Spot, 0.0f), FRotator(0.0f, 90.0f * Quarter, 0.0f), FVector(Scale));
			}
		}
	}

	/**
	 *  The Mountain Temple pack on a maze floor: grass, stones and (from 2F) ginkgo leaves at the foot of
	 *  the walls, ginkgo trees beyond the fence on 2F and 3F (1F keeps its bamboo), fireflies, and falling
	 *  leaves on 3F. Everything goes through the occupancy rules, so nothing covers a coin or a doorway.
	 */
	void DressShrinePack(UWorld& World, const FMatsuriFloorLayout& Layout, const FMatsuriFloorSpec& Spec, int32 FloorIndex)
	{
		if (!Matsuri::Art::HasShrinePack())
		{
			return;
		}
		FRandomStream Rng(Spec.Seed ^ PackSeedSalt);
		const TCHAR* const Foot1F[] = { TEXT("SM_Grass_01"), TEXT("SM_Grass_03"), TEXT("SM_Stone_01"), TEXT("SM_Stone_03"), TEXT("SM_Flower_01"), TEXT("SM_Lamp_01") };
		const TCHAR* const FootAutumn[] = { TEXT("SM_Ginkgo_Leaves_01"), TEXT("SM_Ginkgo_Leaves_03"), TEXT("SM_Grass_01"), TEXT("SM_Stone_02"), TEXT("SM_Rock_02") };
		int32 Placed = 0;
		for (int32 Index = 0; Index < Layout.Blocks.Num(); ++Index)
		{
			const FMatsuriBlockPlacement& Block = Layout.Blocks[Index];
			const bool bMazeWall = Block.Type == EMatsuriBlock::Wall && FMath::Min(Block.Size.X, Block.Size.Y) < 80.0f;
			if (!bMazeWall || Rng.FRand() > PackWallDressChance)
			{
				continue;
			}
			const bool bAlongX = Block.Size.X >= Block.Size.Y;
			const float Along = Rng.FRandRange(-0.4f, 0.4f) * (bAlongX ? Block.Size.X : Block.Size.Y);
			const float Side = Rng.FRand() < 0.5f ? -1.0f : 1.0f;
			const TCHAR* Piece = FloorIndex == 0 ? Foot1F[Rng.RandRange(0, UE_ARRAY_COUNT(Foot1F) - 1)] : FootAutumn[Rng.RandRange(0, UE_ARRAY_COUNT(FootAutumn) - 1)];
			const FVector PieceSize = Matsuri::ShrineProps::Size(Piece) * 0.5f;
			const float Off = (bAlongX ? Block.Size.Y : Block.Size.X) * 0.5f + PackWallGap + FMath::Max(PieceSize.X, PieceSize.Y);
			const FVector Base = bAlongX ? FVector(Block.Center.X + Along, Block.Center.Y + Side * Off, 0.0f) : FVector(Block.Center.X + Side * Off, Block.Center.Y + Along, 0.0f);
			const bool bFlat = FString(Piece).Contains(TEXT("Leaves"));
			Placed += SpawnPack(World, Piece, Base, Rng.FRandRange(0.0f, 360.0f), FVector(Rng.FRandRange(0.8f, 1.1f)), {}, !bFlat) == EArt::Placed ? 1 : 0;
		}
		// Ginkgo trees in some cell corners on the autumn floors, tall enough that their crowns rise over the
		// 4.5 m corridor walls and are seen from anywhere. Only the trunk has to fit the occupancy rules: the
		// crown hangs high over corridors, coins and walls (pack pieces have no collision).
		if (FloorIndex > 0 && Layout.Maze.IsValid())
		{
			const float Reach = (Layout.Maze.CellSize() - Matsuri::Layout::MazeWallThickness) * 0.5f - PackCellTreeInset - PackTrunkHalf;
			for (int32 Y = 0; Y < Layout.Maze.Size; ++Y)
			{
				for (int32 X = 0; X < Layout.Maze.Size; ++X)
				{
					if (Rng.FRand() > PackCellTreeChance)
					{
						continue;
					}
					const TCHAR* Tree = Rng.FRand() < 0.5f ? TEXT("SM_Ginkgo_Tree_01") : TEXT("SM_Ginkgo_Tree_02");
					const float Scale = Matsuri::ShrineProps::ScaleToHeight(Tree, Rng.FRandRange(PackCellTreeHeight * 0.85f, PackCellTreeHeight * 1.15f));
					const FVector2D Corner(Rng.FRand() < 0.5f ? -Reach : Reach, Rng.FRand() < 0.5f ? -Reach : Reach);
					const FVector Base(Layout.Maze.CellCenter(FIntPoint(X, Y)) + Corner, 0.0f);
					const FBox Trunk(Base - FVector(PackTrunkHalf, PackTrunkHalf, 0.0f), Base + FVector(PackTrunkHalf, PackTrunkHalf, Spec.WallHeight));
					if (Scale <= 0.0f || (GOccupancy && !GOccupancy->Fits(Trunk, INDEX_NONE, false)))
					{
						continue;
					}
					if (Matsuri::ShrineProps::Place(World, Tree, Base, Rng.FRandRange(0.0f, 360.0f), FVector(Scale)))
					{
						if (GOccupancy)
						{
							GOccupancy->Art.Add(Trunk);
						}
						++Placed;
					}
				}
			}
		}
		// Ginkgo trees rise over the fence on the autumn floors.
		if (FloorIndex > 0)
		{
			const float Outer = FenceOuter(Layout, Spec.HalfExtent);
			const int32 PerSide = FMath::Max(1, FMath::CeilToInt32(2.0f * Outer / PackTreeSpacing));
			for (int32 Side = 0; Side < 4; ++Side)
			{
				for (int32 i = 0; i < PerSide; ++i)
				{
					const float Along = -Outer + (i + 0.5f) * (2.0f * Outer / PerSide) + Rng.FRandRange(-250.0f, 250.0f);
					const float Out = Outer + 500.0f + Rng.FRandRange(0.0f, PackTreeBand);
					const FVector2D Spot = Side == 0 ? FVector2D(Out, Along) : Side == 1 ? FVector2D(-Out, Along) : Side == 2 ? FVector2D(Along, Out) : FVector2D(Along, -Out);
					const TCHAR* Tree = Rng.FRand() < 0.5f ? TEXT("SM_Ginkgo_Tree_01") : TEXT("SM_Ginkgo_Tree_02");
					Placed += Matsuri::ShrineProps::Place(World, Tree, FVector(Spot, 0.0f), Rng.FRandRange(0.0f, 360.0f), FVector(Rng.FRandRange(0.6f, 0.9f))) ? 1 : 0;
				}
			}
		}
		// Fallen leaves: most drifted against the walls, the rest anywhere.
		{
			TArray<int32> Walls;
			for (int32 Index = 0; Index < Layout.Blocks.Num(); ++Index)
			{
				if (Layout.Blocks[Index].Type == EMatsuriBlock::Wall)
				{
					Walls.Add(Index);
				}
			}
			auto Spot = [&]() -> FVector2D
			{
				if (Walls.Num() > 0 && Rng.FRand() < PackLeafWallShare)
				{
					const FMatsuriBlockPlacement& Wall = Layout.Blocks[Walls[Rng.RandRange(0, Walls.Num() - 1)]];
					const bool bAlongX = Wall.Size.X >= Wall.Size.Y;
					const float Along = Rng.FRandRange(-0.5f, 0.5f) * (bAlongX ? Wall.Size.X : Wall.Size.Y);
					const float Out = ((bAlongX ? Wall.Size.Y : Wall.Size.X) * 0.5f + FMath::Square(Rng.FRand()) * PackLeafDrift) * (Rng.FRand() < 0.5f ? -1.0f : 1.0f);
					return bAlongX ? FVector2D(Wall.Center.X + Along, Wall.Center.Y + Out) : FVector2D(Wall.Center.X + Out, Wall.Center.Y + Along);
				}
				return FVector2D(Rng.FRandRange(-Spec.HalfExtent, Spec.HalfExtent), Rng.FRandRange(-Spec.HalfExtent, Spec.HalfExtent));
			};
			const int32 Floor = FMath::Clamp(FloorIndex, 0, 2);
			TArray<FTransform> Leaves;
			for (int32 i = 0; i < PackLeaves[Floor]; ++i)
			{
				Leaves.Add(FTransform(FRotator(Rng.FRandRange(-12.0f, 12.0f), Rng.FRandRange(0.0f, 360.0f), Rng.FRandRange(-12.0f, 12.0f)),
					FVector(Spot(), 1.0f), FVector(Rng.FRandRange(0.8f, 1.5f))));
			}
			Matsuri::ShrineProps::Scatter(World, TEXT("SM_Ginkgo_Leaf_Single_01"), Leaves);
			const TCHAR* const Patches[] = { TEXT("SM_Ginkgo_Leaves_01"), TEXT("SM_Ginkgo_Leaves_02"), TEXT("SM_Ginkgo_Leaves_03") };
			for (const TCHAR* Patch : Patches)
			{
				TArray<FTransform> Copies;
				for (int32 i = 0; i < PackLeafPatches[Floor] / UE_ARRAY_COUNT(Patches); ++i)
				{
					Copies.Add(FTransform(FRotator(0.0f, Rng.FRandRange(0.0f, 360.0f), 0.0f), FVector(Spot(), 2.0f), FVector(Rng.FRandRange(0.8f, 1.6f))));
				}
				Matsuri::ShrineProps::Scatter(World, Patch, Copies);
			}
		}
		// Fireflies over the maze; falling ginkgo over the autumn floors.
		for (int32 i = 0; i < PackEffectsPerFloor; ++i)
		{
			const FVector Spot(Rng.FRandRange(-Spec.HalfExtent, Spec.HalfExtent) * 0.7f, Rng.FRandRange(-Spec.HalfExtent, Spec.HalfExtent) * 0.7f, 0.0f);
			Matsuri::ShrineProps::Effect(World, TEXT("NS_Fireflies"), Spot + FVector(0.0f, 0.0f, 150.0f));
			if (FloorIndex > 0)
			{
				Matsuri::ShrineProps::Effect(World, TEXT("NS_Falling_Leaves"), Spot + FVector(0.0f, 0.0f, 600.0f));	// low: lit, not black specks
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Floor %d dressed with the Mountain Temple pack: %d pieces"), FloorIndex + 1, Placed);
	}

	/** 1F: tall bamboo crowding in just behind the fence, so the grove rises over the walls. */
	void DressBamboo(UWorld& World, const FMatsuriFloorLayout& Layout, const FMatsuriFloorSpec& Spec, int32 Seed)
	{
		if (const UStaticMesh* Mesh = Matsuri::Art::Mesh(TEXT("SM_Prop_Bamboo")))
		{
			DressBambooArt(World, *Mesh, Layout, Spec, Seed);
			return;
		}
		constexpr int32 Stalks = 160;
		constexpr float BandDepth = 350.0f;
		FRandomStream Rng(Seed);
		const float H = Spec.HalfExtent + 160.0f;	// just outside the fence
		for (int32 i = 0; i < Stalks; ++i)
		{
			const float Along = Rng.FRandRange(-H - BandDepth, H + BandDepth);
			const float Out = H + Rng.FRandRange(0.0f, BandDepth);
			const int32 Side = i % 4;
			const FVector2D Spot = Side == 0 ? FVector2D(Out, Along) : Side == 1 ? FVector2D(-Out, Along) : Side == 2 ? FVector2D(Along, Out) : FVector2D(Along, -Out);
			const float Height = Rng.FRandRange(900.0f, 1500.0f);
			const float Thickness = Rng.FRandRange(14.0f, 22.0f);
			const FLinearColor Green = FLinearColor(0.16f, 0.32f, 0.08f) * Rng.FRandRange(0.7f, 1.2f);
			Spawn(World, { EShape::Cylinder, FVector(Spot, Height * 0.5f), FVector(Thickness, Thickness, Height),
				FRotator(Rng.FRandRange(-3.0f, 3.0f), 0.0f, Rng.FRandRange(-3.0f, 3.0f)), Green });
		}
	}

	FOccupancy MakeOccupancy(const FMatsuriFloorLayout& Layout)
	{
		FOccupancy Occupancy;
		for (const FMatsuriBlockPlacement& Block : Layout.Blocks)
		{
			Occupancy.Blocks.Add(Block.Type == EMatsuriBlock::Ground ? FBox(ForceInit) : FBox::BuildAABB(Block.Center, Block.Size * 0.5f));
		}
		for (const FMatsuriCoinPlacement& Coin : Layout.Coins)
		{
			Occupancy.Points.Add(FBox::BuildAABB(Coin.Location, FVector(CoinHalfSize)));
		}
		for (const FMatsuriSpawnPlacement& Spawn : Layout.Spawns)
		{
			Occupancy.Points.Add(FBox::BuildAABB(Spawn.Location, FVector(SpawnHalfSize)));
		}
		Occupancy.Points.Add(FBox::BuildAABB(Layout.PlayerStart, FVector(SpawnHalfSize, SpawnHalfSize, 100.0f)));
		return Occupancy;
	}
}

void Matsuri::Decor::DressFloor(UWorld& World, const FMatsuriFloorLayout& Layout, const FMatsuriFloorSpec& Spec,
	const FMatsuriFloorTheme& Theme, int32 FloorIndex)
{
	FOccupancy Occupancy = MakeOccupancy(Layout);
	TGuardValue<FOccupancy*> ActiveOccupancy(GOccupancy, &Occupancy);

	// A torii frames the first view; the far one marks where the festival leads.
	if (Layout.Maze.IsValid())
	{
		// On a maze both gates stand inside their cells, fitted between its walls. The start gate stands
		// just behind the spawn (the player has walked through it): in front, its posts filled the first view.
		const float Cell = Layout.Maze.CellSize();
		const float Clear = Cell - Matsuri::Layout::MazeWallThickness - 2.0f * WallClearance;
		const float Ahead = FMath::Min(ToriiStartAhead, Clear * 0.5f - ToriiPostDiameter);
		SpawnTorii(World, Layout.PlayerStart * FVector(1, 1, 0) - FVector(Ahead * 0.5f, 0.0f, 0.0f), 1.0f, Clear);
		const FVector2D Exit = Layout.Maze.CellCenter(Layout.ExitCell);
		SpawnTorii(World, FVector(Exit.X + Ahead * 0.5f, Exit.Y, 0.0f), 1.0f, Clear);
		DressMazeWalls(World, Layout, Spec, Theme);
	}
	else
	{
		SpawnTorii(World, Layout.PlayerStart * FVector(1, 1, 0) + FVector(ToriiStartAhead, 0.0f, 0.0f), 1.0f);
		SpawnTorii(World, FVector(Spec.HalfExtent - 220.0f, 0.0f, 0.0f), 1.35f);
	}

	DressFence(World, Spec, Theme);
	DressBlocks(World, Layout, Theme, FloorIndex);
	DressApproach(World, Layout, Theme);
	if (FloorIndex == 0)
	{
		DressBamboo(World, Layout, Spec, Spec.Seed);
	}
	DressShrinePack(World, Layout, Spec, FloorIndex);
}

bool Matsuri::Decor::PlaceArt(UWorld& World, const TCHAR* Name, const FVector& Location, const FRotator& Rotation, float Scale)
{
	return SpawnArt(World, Name, Location, Rotation, FVector(Scale)) == EArt::Placed;
}

void Matsuri::Decor::PlaceLantern(UWorld& World, const FVector& Center, const FLinearColor& Color, float LightIntensity)
{
	SpawnLantern(World, Center, Color, LightIntensity);
}

void Matsuri::Decor::PlaceWallRoof(UWorld& World, const FVector2D& Center, float Length, float Thick, float WallTop, bool bAlongX)
{
	SpawnWallRoof(World, Center, Length, Thick, WallTop, bAlongX);
}

void Matsuri::Decor::PlaceTrim(UWorld& World, const FVector& Center, const FVector& Size, TOptional<Matsuri::Graybox::ESurface> Surface,
	const FLinearColor& Tint)
{
	FShape Trim{ EShape::Cube, Center, Size, FRotator::ZeroRotator, Tint };
	Trim.Surface = Surface;
	Trim.bCastShadow = false;
	Spawn(World, Trim);
}
