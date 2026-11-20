// Suzune's Night Festival - the 3D stage behind the title menu.

#include "MatsuriTitleStage.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "MatsuriArt.h"
#include "MatsuriToon.h"
#include "MatsuriFloorBuilder.h"
#include "MatsuriFloorDecor.h"
#include "MatsuriFloors.h"

namespace
{
	// The camera stands at the origin looking down +X; the menu column covers the left third,
	// so the heroines stand right of centre.
	const FVector CameraLocation(0.0f, 0.0f, 150.0f);
	const FRotator CameraRotation(-4.0f, 0.0f, 0.0f);
	constexpr float CameraFov = 60.0f;
	constexpr float HeroineScale = 1.6f;	// rigged heroines come out 1 m tall
	constexpr int32 StageFloor = 1;			// 2F festival night: lanterns and warm light

	/** Sakura in front in the middle, Suzune and Mio either side and a step back. */
	const FVector HeroineSpots[] = { FVector(470.0f, 150.0f, 0.0f), FVector(560.0f, 20.0f, 0.0f), FVector(560.0f, 285.0f, 0.0f) };
	const FLinearColor FallbackToriiRed(0.72f, 0.07f, 0.025f);
	const FLinearColor FallbackLacquer(0.025f, 0.018f, 0.02f);
	const FLinearColor FallbackGold(0.92f, 0.52f, 0.08f);
	const FLinearColor FallbackStone(0.15f, 0.10f, 0.13f);

	void SpawnHeroine(UWorld& World, EMatsuriHero Hero, const FVector& Spot)
	{
		USkeletalMesh* Mesh = Matsuri::Art::HeroMesh(Hero, TEXT("Idle"));
		if (!Mesh)
		{
			return;
		}
		const FRotator FaceCamera(0.0f, (CameraLocation - Spot).Rotation().Yaw, 0.0f);	// the models face +X
		ASkeletalMeshActor* Actor = World.SpawnActor<ASkeletalMeshActor>(Spot, FaceCamera);
		USkeletalMeshComponent* Component = Actor->GetSkeletalMeshComponent();
		Component->SetRelativeScale3D(FVector(HeroineScale));
		Component->SetSkeletalMesh(Mesh);
		if (Hero == EMatsuriHero::Suzune)
		{
			Matsuri::Toon::Apply(Component, false);
		}
		if (UAnimSequence* Idle = Matsuri::Art::HeroAnim(Hero, TEXT("Idle")))
		{
			Component->PlayAnimation(Idle, true);
			Component->SetPosition(FMath::FRandRange(0.0f, Idle->GetPlayLength()));	// not in lockstep
		}
	}

	void SpawnKeyLight(UWorld& World, const FVector& Location, const FLinearColor& Color, float Intensity, float Radius)
	{
		APointLight* Light = World.SpawnActor<APointLight>(Location, FRotator::ZeroRotator);
		UPointLightComponent* Component = CastChecked<UPointLightComponent>(Light->GetLightComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetIntensity(Intensity);
		Component->SetLightColor(Color);
		Component->SetAttenuationRadius(Radius);
		Component->SetCastShadows(true);
	}

	void SpawnFallbackTorii(UWorld& World, const FVector& Base)
	{
		// Frame the whole gate behind the lanterns, with a low platform and stepping stones leading to it.
		Matsuri::Decor::PlaceTrim(World, FVector(1500.0f, 0.0f, -25.0f), FVector(5000.0f, 5000.0f, 50.0f), {}, FallbackLacquer);
		Matsuri::Decor::PlaceTrim(World, Base - FVector(0.0f, 0.0f, 7.0f), FVector(420.0f, 850.0f, 14.0f), {}, FallbackStone);
		for (int32 Stone = 0; Stone < 6; ++Stone)
		{
			const FVector Center(460.0f + Stone * 190.0f, Base.Y, 2.0f);
			Matsuri::Decor::PlaceTrim(World, Center, FVector(174.0f, 340.0f, 4.0f), {}, FallbackStone);
		}
		const FVector PostSize(56.0f, 72.0f, 520.0f);
		constexpr float HalfWidth = 300.0f;
		Matsuri::Decor::PlaceTrim(World, Base + FVector(0.0f, -HalfWidth, 260.0f), PostSize, {}, FallbackToriiRed);
		Matsuri::Decor::PlaceTrim(World, Base + FVector(0.0f, HalfWidth, 260.0f), PostSize, {}, FallbackToriiRed);
		Matsuri::Decor::PlaceTrim(World, Base + FVector(0.0f, 0.0f, 405.0f), FVector(56.0f, 720.0f, 48.0f), {}, FallbackToriiRed);
		Matsuri::Decor::PlaceTrim(World, Base + FVector(0.0f, 0.0f, 525.0f), FVector(70.0f, 800.0f, 54.0f), {}, FallbackToriiRed);
		Matsuri::Decor::PlaceTrim(World, Base + FVector(0.0f, 0.0f, 480.0f), FVector(62.0f, 160.0f, 68.0f), {}, FallbackLacquer);
		Matsuri::Decor::PlaceTrim(World, Base + FVector(-32.0f, 0.0f, 480.0f), FVector(3.0f, 108.0f, 8.0f), {}, FallbackGold);
	}
}

ACameraActor* Matsuri::TitleStage::Build(UWorld& World)
{
	const FMatsuriFloorTheme Theme = Matsuri::Floors::ThemeFor(StageFloor);
	Matsuri::Builder::SpawnEnvironment(World, Theme);

	// The approach: a torii behind the heroines, stone lanterns either side, chochin overhead.
	const FVector ToriiBase(1150.0f, 150.0f, 0.0f);
	if (Matsuri::Decor::PlaceArt(World, TEXT("SM_Prop_Torii"), ToriiBase, FRotator::ZeroRotator, 1.0f))
	{
		FMatsuriFloorLayout Ground;
		Ground.Blocks.Add({ EMatsuriBlock::Ground, FVector(1500.0f, 0.0f, -25.0f), FVector(5000.0f, 5000.0f, 50.0f) });
		Matsuri::Builder::SpawnBlocks(World, Ground, Theme);
	}
	else
	{
		SpawnFallbackTorii(World, FVector(1820.0f, 300.0f, 14.0f));
	}
	Matsuri::Decor::PlaceArt(World, TEXT("SM_Prop_StoneLantern"), FVector(850.0f, -330.0f, 0.0f), FRotator(0.0f, 20.0f, 0.0f), 0.8f);
	Matsuri::Decor::PlaceArt(World, TEXT("SM_Prop_StoneLantern"), FVector(850.0f, 640.0f, 0.0f), FRotator(0.0f, -20.0f, 0.0f), 0.8f);
	Matsuri::Decor::PlaceArt(World, TEXT("SM_Prop_Yatai"), FVector(1500.0f, -700.0f, 0.0f), FRotator(0.0f, 70.0f, 0.0f), 1.0f);
	Matsuri::Decor::PlaceArt(World, TEXT("SM_Prop_KitsuneStatue"), FVector(1000.0f, 900.0f, 0.0f), FRotator(0.0f, 200.0f, 0.0f), 0.9f);
	for (int32 i = 0; i < 7; ++i)
	{
		Matsuri::Decor::PlaceLantern(World, FVector(700.0f + i * 140.0f, -260.0f + i * 25.0f, 330.0f), Theme.LanternColor, i % 2 == 0 ? Theme.LanternIntensity : 0.0f);
		Matsuri::Decor::PlaceLantern(World, FVector(700.0f + i * 140.0f, 560.0f - i * 25.0f, 330.0f), Theme.LanternColor, i % 2 == 1 ? Theme.LanternIntensity : 0.0f);
	}
	for (int32 i = 0; i < 9; ++i)
	{
		Matsuri::Decor::PlaceArt(World, TEXT("SM_Prop_Bamboo"), FVector(2300.0f + (i % 3) * 300.0f, -1400.0f + i * 380.0f, 0.0f), FRotator(0.0f, i * 47.0f, 0.0f), 1.2f + (i % 3) * 0.15f);
	}

	SpawnHeroine(World, EMatsuriHero::Sakura, HeroineSpots[0]);
	SpawnHeroine(World, EMatsuriHero::Suzune, HeroineSpots[1]);
	SpawnHeroine(World, EMatsuriHero::Mio, HeroineSpots[2]);
	// Warm key light from the camera side so faces read against the night.
	SpawnKeyLight(World, FVector(250.0f, 60.0f, 260.0f), FLinearColor(1.0f, 0.82f, 0.65f), 9000.0f, 900.0f);

	ACameraActor* Camera = World.SpawnActor<ACameraActor>(CameraLocation, CameraRotation);
	Camera->GetCameraComponent()->SetFieldOfView(CameraFov);
	Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
	return Camera;
}
