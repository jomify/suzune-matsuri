// Suzune's Night Festival - turns a floor layout into actors in the world.

#include "MatsuriFloorBuilder.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "MatsuriAmmoPickup.h"
#include "MatsuriCollectible.h"
#include "MatsuriGraybox.h"
#include "MatsuriHazards.h"
#include "MatsuriArt.h"
#include "MatsuriYokaiKinds.h"

namespace
{
	constexpr float CubeSize = 100.0f;	// /Engine/BasicShapes/Cube is 1 m
	constexpr int32 AmmoPerSpot = 5;
	const TCHAR* const GroundMaterialPath = TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray");

	FLinearColor BlockColor(EMatsuriBlock Type, const FMatsuriFloorTheme& Theme)
	{
		switch (Type)
		{
		case EMatsuriBlock::Wall:		return Theme.WallColor;
		case EMatsuriBlock::LowCover:	return Theme.CoverColor;
		case EMatsuriBlock::Pillar:		return Theme.PillarColor;
		case EMatsuriBlock::Ground:		break;
		}
		return FLinearColor::Gray;
	}

	/** Walls are earthen plaster; grey pillars are stone lanterns; coloured ones (chochin poles, torii posts) are painted wood. */
	Matsuri::Graybox::ESurface SurfaceFor(EMatsuriBlock Type, const FMatsuriFloorTheme& Theme)
	{
		if (Type == EMatsuriBlock::Wall)
		{
			return Matsuri::Graybox::ESurface::Plaster;
		}
		const FLinearColor Pillar = Theme.PillarColor;
		const bool bGreyPillar = FMath::Max3(Pillar.R, Pillar.G, Pillar.B) - FMath::Min3(Pillar.R, Pillar.G, Pillar.B) < 0.1f;
		return Type == EMatsuriBlock::Pillar && bGreyPillar ? Matsuri::Graybox::ESurface::Stone : Matsuri::Graybox::ESurface::Wood;
	}

	UClass* ClassFor(EMatsuriSpawn Type)
	{
		switch (Type)
		{
		case EMatsuriSpawn::Chochin:	return AMatsuriChochin::StaticClass();
		case EMatsuriSpawn::Kasa:		return AMatsuriKasa::StaticClass();
		case EMatsuriSpawn::Kappa:		return AMatsuriKappa::StaticClass();
		case EMatsuriSpawn::Foxfire:	return AMatsuriFoxfire::StaticClass();
		case EMatsuriSpawn::Spike:		return AMatsuriSpikeTrap::StaticClass();
		case EMatsuriSpawn::Ammo:		return AMatsuriAmmoPickup::StaticClass();
		}
		return nullptr;
	}
}

void Matsuri::Builder::SpawnEnvironment(UWorld& World, const FMatsuriFloorTheme& Theme)
{
	const FTransform SunTransform(Theme.SunRotation);
	ADirectionalLight* Sun = World.SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunTransform);
	UDirectionalLightComponent* SunLight = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->SetIntensity(Theme.SunLux);
	SunLight->SetLightColor(Theme.SunColor);
	SunLight->SetAtmosphereSunLight(Theme.bSkyAtmosphere);
	Sun->FinishSpawning(SunTransform);

	if (Theme.bSkyAtmosphere)
	{
		World.SpawnActor<ASkyAtmosphere>();
	}

	ASkyLight* Sky = World.SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity);
	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	// Real-time capture needs something to capture (a sky atmosphere): without one the engine warns on
	// screen and the light goes black. Night floors capture the scene once instead.
	Sky->GetLightComponent()->bRealTimeCapture = Theme.bSkyAtmosphere;
	Sky->GetLightComponent()->SetIntensity(Theme.SkyLightIntensity);
	Sky->FinishSpawning(FTransform::Identity);
	if (!Theme.bSkyAtmosphere)
	{
		Sky->GetLightComponent()->RecaptureSky();
	}

	AExponentialHeightFog* Fog = World.SpawnActor<AExponentialHeightFog>();
	Fog->GetComponent()->SetFogDensity(Theme.FogDensity);
	Fog->GetComponent()->SetFogInscatteringColor(Theme.FogColor);

	// One unbound grade for the whole floor: exposure per mood, glow that blooms, a soft vignette.
	APostProcessVolume* Grade = World.SpawnActor<APostProcessVolume>();
	Grade->bUnbound = true;
	FPostProcessSettings& Settings = Grade->Settings;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = Theme.ExposureBias;
	Settings.bOverride_AutoExposureSpeedUp = true;
	Settings.AutoExposureSpeedUp = 10.0f;		// no slow fade-in at floor start
	Settings.bOverride_AutoExposureSpeedDown = true;
	Settings.AutoExposureSpeedDown = 10.0f;
	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = 0.9f;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = 0.45f;
	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(1.12f, 1.12f, 1.12f, 1.0f);
	Settings.bOverride_ColorContrast = true;
	Settings.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.0f);
	// Playtest ("not smooth"): camera motion blur smeared the whole view on every turn, which reads as
	// judder in a first-person shooter, worse at a low frame rate. Shooters turn it off; so does this one.
	Settings.bOverride_MotionBlurAmount = true;
	Settings.MotionBlurAmount = 0.0f;
}

void Matsuri::Builder::SpawnBlocks(UWorld& World, const FMatsuriFloorLayout& Layout, const FMatsuriFloorTheme& Theme)
{
	UMaterialInterface* GroundMaterial = LoadObject<UMaterialInterface>(nullptr, GroundMaterialPath);
	for (const FMatsuriBlockPlacement& Block : Layout.Blocks)
	{
		const FTransform Transform(FRotator::ZeroRotator, Block.Center, Block.Size / CubeSize);
		AStaticMeshActor* Actor = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetStaticMesh(Graybox::Mesh(Graybox::EShape::Cube));
		MeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Actor->FinishSpawning(Transform);

		if (Block.Type == EMatsuriBlock::Ground)
		{
			if (Matsuri::Art::Texture(TEXT("T_Stone_Paving")))
			{
				Graybox::PaintSurface(MeshComponent, Graybox::ESurface::Paving, FLinearColor(0.9f, 0.88f, 0.85f));
			}
			else if (GroundMaterial)
			{
				MeshComponent->SetMaterial(0, GroundMaterial);
			}
		}
		else
		{
			Graybox::PaintSurface(MeshComponent, SurfaceFor(Block.Type, Theme), BlockColor(Block.Type, Theme));
		}
	}
}

void Matsuri::Builder::SpawnCollectibles(UWorld& World, const FMatsuriFloorLayout& Layout)
{
	for (const FMatsuriCoinPlacement& Coin : Layout.Coins)
	{
		const FTransform Transform(Coin.Location);
		AMatsuriCollectible* Collectible = World.SpawnActorDeferred<AMatsuriCollectible>(AMatsuriCollectible::StaticClass(), Transform);
		Collectible->SetKind(Coin.Kind);
		Collectible->FinishSpawning(Transform);
	}
}

void Matsuri::Builder::SpawnSpawns(UWorld& World, const FMatsuriFloorLayout& Layout, float ArenaHalfExtent, bool bThreats)
{
	for (const FMatsuriSpawnPlacement& Spawn : Layout.Spawns)
	{
		if (!bThreats && Spawn.Type != EMatsuriSpawn::Ammo)
		{
			continue;
		}
		const FTransform Transform(Spawn.Location + FVector(0.0f, 0.0f, 70.0f));
		AActor* Actor = World.SpawnActorDeferred<AActor>(ClassFor(Spawn.Type), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (AMatsuriYokai* Yokai = Cast<AMatsuriYokai>(Actor))
		{
			Yokai->SetArenaHalfExtent(ArenaHalfExtent);
		}
		else if (AMatsuriAmmoPickup* Ammo = Cast<AMatsuriAmmoPickup>(Actor))
		{
			Ammo->Configure(AmmoPerSpot, true);
		}
		Actor->FinishSpawning(Transform);
	}
}
