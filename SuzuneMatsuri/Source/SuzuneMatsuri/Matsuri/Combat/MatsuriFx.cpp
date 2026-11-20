// Suzune's Night Festival - code-built visual effects.

#include "MatsuriFx.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

using Matsuri::Graybox::EShape;

namespace
{
	constexpr float ShapeSize = 100.0f;		// engine basic shapes are 1 m
	const FName GlowParameter(TEXT("Glow"));
	constexpr int32 RingPieces = 24;
	constexpr int32 ArcPieces = 9;
	constexpr int32 HaloStars = 5;
	constexpr int32 MaxPooledPerWorld = 256;	// more than a busy fight has alive at once; the rest are destroyed

	/** Retired pieces per world, waiting to be reused. */
	TMap<TObjectKey<UWorld>, TArray<TWeakObjectPtr<AMatsuriFx>>>& Pool()
	{
		static TMap<TObjectKey<UWorld>, TArray<TWeakObjectPtr<AMatsuriFx>>> Retired;
		return Retired;
	}

	AMatsuriFx* TakeRetired(UWorld* World)
	{
		TArray<TWeakObjectPtr<AMatsuriFx>>* Free = Pool().Find(World);
		while (Free && Free->Num() > 0)
		{
			AMatsuriFx* Piece = Free->Pop(EAllowShrinking::No).Get();
			if (IsValid(Piece) && !Piece->IsActive())
			{
				return Piece;
			}
		}
		return nullptr;
	}

	void Release(AMatsuriFx* Piece)
	{
		TArray<TWeakObjectPtr<AMatsuriFx>>& Free = Pool().FindOrAdd(Piece->GetWorld());
		if (Free.Num() >= MaxPooledPerWorld)
		{
			Piece->Destroy();
			return;
		}
		Piece->Retire();
		Free.Add(Piece);
	}
}

AMatsuriFx::AMatsuriFx()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);

	Shape = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shape"));
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shape->SetGenerateOverlapEvents(false);
	Shape->SetCastShadow(false);
	Shape->SetCanEverAffectNavigation(false);
	RootComponent = Shape;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Shape);
	Light->SetCastShadows(false);
	Light->SetIntensity(0.0f);
}

void AMatsuriFx::Configure(const FMatsuriFxSpec& InSpec)
{
	Spec = InSpec;
	Spec.Lifetime = FMath::Max(0.05f, Spec.Lifetime);
	Shape->SetStaticMesh(Matsuri::Graybox::Mesh(Spec.Shape));
	Shape->SetRelativeScale3D(Spec.StartSize / ShapeSize);
	Shape->SetVisibility(Spec.bVisible);
	SetActorRotation(Spec.Rotation);
	Light->SetLightColor(Spec.Color);
	Light->SetIntensity(Spec.LightIntensity);
	Light->SetAttenuationRadius(Spec.LightRadius);
	Light->SetVisibility(Spec.LightIntensity > 0.0f);
}

void AMatsuriFx::BeginPlay()
{
	Super::BeginPlay();
	Matsuri::Graybox::Paint(Shape, Spec.Color, true, Spec.Glow);
	Material = Cast<UMaterialInstanceDynamic>(Shape->GetMaterial(0));
}

void AMatsuriFx::Reuse(const FVector& Location, const FMatsuriFxSpec& InSpec)
{
	bActive = true;
	Age = 0.0f;
	Configure(InSpec);
	SetActorLocation(Location);
	Matsuri::Graybox::Paint(Shape, Spec.Color, true, Spec.Glow);	// reuses its material instance: same parent
	Material = Cast<UMaterialInstanceDynamic>(Shape->GetMaterial(0));
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
}

void AMatsuriFx::Retire()
{
	bActive = false;
	TArray<AActor*> Riders;
	GetAttachedActors(Riders);
	for (AActor* Rider : Riders)
	{
		Rider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);	// as Destroy would: they live on
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Light->SetVisibility(false);
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
}

void AMatsuriFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / Spec.Lifetime, 0.0f, 1.0f);
	const float Fade = 1.0f - Alpha;

	Shape->SetRelativeScale3D(FMath::Lerp(Spec.StartSize, Spec.EndSize, Alpha) / ShapeSize);
	if (Material)
	{
		Material->SetScalarParameterValue(GlowParameter, Spec.Glow * Fade);
	}
	if (Spec.LightIntensity > 0.0f)
	{
		Light->SetIntensity(Spec.LightIntensity * Fade);
	}
	if (!Spec.Velocity.IsNearlyZero() || Spec.Gravity > 0.0f)
	{
		Spec.Velocity.Z -= Spec.Gravity * DeltaSeconds;
		AddActorWorldOffset(Spec.Velocity * DeltaSeconds);
	}
	if (!Spec.Spin.IsZero())
	{
		AddActorLocalRotation(Spec.Spin * DeltaSeconds);
	}
	if (Age >= Spec.Lifetime)
	{
		Release(this);
	}
}

// ---------------------------------------------------------------- factory

AMatsuriFx* Matsuri::Fx::Spawn(UWorld* World, const FVector& Location, const FMatsuriFxSpec& Spec, AActor* Attach)
{
	if (!World)
	{
		return nullptr;
	}
	AMatsuriFx* Piece = TakeRetired(World);
	if (Piece)
	{
		Piece->Reuse(Location, Spec);
	}
	else
	{
		const FTransform Transform(Spec.Rotation, Location);
		Piece = World->SpawnActorDeferred<AMatsuriFx>(AMatsuriFx::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Piece->Configure(Spec);
		Piece->FinishSpawning(Transform);
	}
	if (Attach)
	{
		Piece->AttachToActor(Attach, FAttachmentTransformRules::KeepWorldTransform);
	}
	return Piece;
}

void Matsuri::Fx::Burst(UWorld* World, const FVector& Location, const FLinearColor& Color, int32 Count, float Speed, float Size,
	float Lifetime, const FVector& Direction)
{
	const bool bAimed = !Direction.IsNearlyZero();
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Out = bAimed ? FMath::VRandCone(Direction.GetSafeNormal(), FMath::DegreesToRadians(35.0f)) : FMath::VRand();
		FMatsuriFxSpec Spec;
		Spec.Color = Color;
		Spec.Glow = 10.0f;
		Spec.StartSize = FVector(Size);
		Spec.EndSize = FVector(1.0f);
		Spec.Velocity = Out * Speed * FMath::FRandRange(0.5f, 1.0f) + FVector(0.0f, 0.0f, Speed * 0.35f);
		Spec.Gravity = 900.0f;
		Spec.Lifetime = Lifetime * FMath::FRandRange(0.7f, 1.1f);
		Spec.LightIntensity = i == 0 ? 3000.0f : 0.0f;	// one light per burst keeps the floor lit without cost
		Spawn(World, Location, Spec);
	}
}

void Matsuri::Fx::Ring(UWorld* World, const FVector& Location, const FLinearColor& Color, float StartRadius, float EndRadius,
	float Lifetime, AActor* Attach, float Spin)
{
	// A pivot carries the pieces so an attached ring can spin around its owner.
	FMatsuriFxSpec PivotSpec;
	PivotSpec.StartSize = PivotSpec.EndSize = FVector(ShapeSize);	// unit scale: attached pieces keep their own size
	PivotSpec.bVisible = false;
	PivotSpec.Glow = 0.0f;
	PivotSpec.Spin = FRotator(0.0f, Spin, 0.0f);
	PivotSpec.Lifetime = Lifetime;
	AMatsuriFx* Pivot = Spawn(World, Location, PivotSpec, Attach);

	const float Speed = (EndRadius - StartRadius) / Lifetime;
	for (int32 i = 0; i < RingPieces; ++i)
	{
		const float Angle = 360.0f * i / RingPieces;
		const FVector Out = FRotator(0.0f, Angle, 0.0f).Vector();
		FMatsuriFxSpec Spec;
		Spec.Shape = EShape::Cube;
		Spec.Color = Color;
		Spec.Glow = 12.0f;
		Spec.Rotation = FRotator(0.0f, Angle + 90.0f, 0.0f);	// tangent to the circle
		Spec.StartSize = FVector(2.0f * PI * StartRadius / RingPieces * 0.8f, 10.0f, 6.0f);
		Spec.EndSize = FVector(2.0f * PI * EndRadius / RingPieces * 0.8f, 4.0f, 2.0f);
		Spec.Velocity = Attach ? FVector::ZeroVector : Out * Speed;
		Spec.Lifetime = Lifetime;
		AMatsuriFx* Piece = Spawn(World, Location + Out * StartRadius, Spec);
		if (Pivot && Piece)
		{
			Piece->AttachToActor(Pivot, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
}

void Matsuri::Fx::Dome(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Lifetime, AActor* Attach)
{
	FMatsuriFxSpec Spec;
	Spec.Color = Color;
	Spec.Glow = 2.5f;
	Spec.StartSize = FVector(Radius * 1.6f);
	Spec.EndSize = FVector(Radius * 2.1f);
	Spec.Lifetime = Lifetime;
	Spec.LightIntensity = 5000.0f;
	Spec.LightRadius = Radius * 3.0f;
	Spawn(World, Location, Spec, Attach);
}

void Matsuri::Fx::Pillar(UWorld* World, const FVector& Location, const FLinearColor& Color, float Height, float Lifetime)
{
	FMatsuriFxSpec Spec;
	Spec.Shape = EShape::Cylinder;
	Spec.Color = Color;
	Spec.Glow = 14.0f;
	Spec.StartSize = FVector(70.0f, 70.0f, Height);
	Spec.EndSize = FVector(8.0f, 8.0f, Height * 1.3f);
	Spec.Lifetime = Lifetime;
	Spec.LightIntensity = 6000.0f;
	Spec.LightRadius = 600.0f;
	Spawn(World, Location + FVector(0.0f, 0.0f, Height * 0.5f), Spec);
}

void Matsuri::Fx::AfterImage(UWorld* World, const FVector& Location, const FLinearColor& Color, float Lifetime)
{
	FMatsuriFxSpec Body;
	Body.Shape = EShape::Cylinder;
	Body.Color = Color;
	Body.Glow = 4.0f;
	Body.StartSize = FVector(50.0f, 50.0f, 140.0f);
	Body.EndSize = FVector(30.0f, 30.0f, 150.0f);
	Body.Lifetime = Lifetime;
	Spawn(World, Location, Body);

	FMatsuriFxSpec Head = Body;
	Head.Shape = EShape::Sphere;
	Head.StartSize = FVector(42.0f);
	Head.EndSize = FVector(30.0f);
	Spawn(World, Location + FVector(0.0f, 0.0f, 90.0f), Head);
}

void Matsuri::Fx::Streak(UWorld* World, const FVector& From, const FVector& To, const FLinearColor& Color, float Lifetime)
{
	const FVector Span = To - From;
	FMatsuriFxSpec Spec;
	Spec.Shape = EShape::Cube;
	Spec.Color = Color;
	Spec.Glow = 12.0f;
	Spec.Rotation = Span.Rotation();
	Spec.StartSize = FVector(Span.Size(), 10.0f, 10.0f);
	Spec.EndSize = FVector(Span.Size(), 1.0f, 1.0f);
	Spec.Lifetime = Lifetime;
	Spawn(World, (From + To) * 0.5f, Spec);
}

void Matsuri::Fx::Smoke(UWorld* World, const FVector& Location, float Size)
{
	constexpr int32 Puffs = 8;
	for (int32 i = 0; i < Puffs; ++i)
	{
		FMatsuriFxSpec Spec;
		Spec.Color = FLinearColor(0.55f, 0.55f, 0.6f);
		Spec.Glow = 0.4f;
		Spec.StartSize = FVector(Size * 0.4f);
		Spec.EndSize = FVector(Size * FMath::FRandRange(0.8f, 1.2f));
		Spec.Velocity = FVector(FMath::RandPointInCircle(80.0f), FMath::FRandRange(40.0f, 120.0f));
		Spec.Lifetime = FMath::FRandRange(0.5f, 0.8f);
		Spawn(World, Location + FVector(FMath::RandPointInCircle(Size * 0.3f), FMath::FRandRange(0.0f, Size * 0.5f)), Spec);
	}
}

void Matsuri::Fx::SwingArc(UWorld* World, const FVector& Eye, const FVector& Facing, const FLinearColor& Color, float Radius, float HalfAngleDegrees)
{
	const FRotator Base = Facing.GetSafeNormal2D().Rotation();
	for (int32 i = 0; i < ArcPieces; ++i)
	{
		const float Yaw = FMath::Lerp(-HalfAngleDegrees, HalfAngleDegrees, static_cast<float>(i) / (ArcPieces - 1));
		const FRotator Around(0.0f, Base.Yaw + Yaw, 0.0f);
		FMatsuriFxSpec Spec;
		Spec.Shape = EShape::Cube;
		Spec.Color = Color;
		Spec.Glow = 14.0f;
		Spec.Rotation = FRotator(-12.0f + i * 3.0f, Around.Yaw + 90.0f, 0.0f);	// slight diagonal slash
		Spec.StartSize = FVector(2.0f * PI * Radius / ArcPieces * 0.9f, 6.0f, 4.0f);
		Spec.EndSize = FVector(2.0f * PI * Radius / ArcPieces * 0.6f, 1.0f, 1.0f);
		Spec.Velocity = Around.Vector() * 400.0f;
		Spec.Lifetime = 0.18f + i * 0.01f;
		Spawn(World, Eye - FVector(0.0f, 0.0f, 30.0f) + Around.Vector() * Radius, Spec);
	}
}

void Matsuri::Fx::StunHalo(UWorld* World, AActor* Target, float Seconds)
{
	if (!Target)
	{
		return;
	}
	const FVector Above = Target->GetActorLocation() + FVector(0.0f, 0.0f, 85.0f);
	FMatsuriFxSpec PivotSpec;
	PivotSpec.StartSize = PivotSpec.EndSize = FVector(ShapeSize);	// unit scale: attached pieces keep their own size
	PivotSpec.bVisible = false;
	PivotSpec.Glow = 0.0f;
	PivotSpec.Spin = FRotator(0.0f, 300.0f, 0.0f);
	PivotSpec.Lifetime = Seconds;
	AMatsuriFx* Pivot = Spawn(World, Above, PivotSpec, Target);

	for (int32 i = 0; i < HaloStars; ++i)
	{
		const FVector Out = FRotator(0.0f, 360.0f * i / HaloStars, 0.0f).Vector();
		FMatsuriFxSpec Star;
		Star.Shape = EShape::Cube;
		Star.Color = FLinearColor(1.0f, 0.9f, 0.3f);
		Star.Glow = 10.0f;
		Star.Rotation = FRotator(45.0f, 0.0f, 45.0f);
		Star.StartSize = Star.EndSize = FVector(12.0f);
		Star.Lifetime = Seconds;
		AMatsuriFx* Piece = Spawn(World, Above + Out * 45.0f, Star);
		if (Pivot && Piece)
		{
			Piece->AttachToActor(Pivot, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
}

void Matsuri::Fx::MuzzleFlash(UWorld* World, const FVector& Location, const FVector& Direction, const FLinearColor& Color)
{
	const FVector Dir = Direction.GetSafeNormal();
	FMatsuriFxSpec Pop;
	Pop.Color = Color;
	Pop.Glow = 20.0f;
	Pop.StartSize = FVector(9.0f);
	Pop.EndSize = FVector(2.0f);
	Pop.Lifetime = 0.06f;
	Pop.LightIntensity = 2500.0f;
	Pop.LightRadius = 250.0f;
	Spawn(World, Location, Pop);

	FMatsuriFxSpec Tongue;
	Tongue.Shape = EShape::Cone;
	Tongue.Color = Color;
	Tongue.Glow = 16.0f;
	Tongue.Rotation = FRotationMatrix::MakeFromZ(Dir).Rotator();	// the cone's tip points along the shot
	Tongue.StartSize = FVector(5.0f, 5.0f, 14.0f);
	Tongue.EndSize = FVector(1.0f, 1.0f, 24.0f);
	Tongue.Velocity = Dir * 300.0f;
	Tongue.Lifetime = 0.05f;
	Spawn(World, Location + Dir * 8.0f, Tongue);
}

namespace
{
	constexpr float YokaiHitScale = 0.35f;	// the templates are sized for big explosions
	constexpr float WallHitScale = 0.3f;
}

void Matsuri::Fx::Impact(UWorld* World, const FVector& Location, const FVector& Normal, const FLinearColor& Color, bool bOnYokai, bool bScorch)
{
	const FVector Out = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	FMatsuriFxSpec Flash;
	Flash.Color = Color;
	Flash.Glow = 18.0f;
	Flash.StartSize = FVector(bOnYokai ? 38.0f : 22.0f);
	Flash.EndSize = FVector(4.0f);
	Flash.Lifetime = 0.09f;
	Flash.LightIntensity = bOnYokai ? 5000.0f : 2500.0f;
	Flash.LightRadius = bOnYokai ? 450.0f : 300.0f;
	Spawn(World, Location, Flash);

	Burst(World, Location, Color, bOnYokai ? 8 : 5, bOnYokai ? 520.0f : 380.0f, bOnYokai ? 8.0f : 5.0f, 0.25f, Out);
	Template(World, bOnYokai ? ETemplate::RadialBurst : ETemplate::Directional, Location, Out, bOnYokai ? YokaiHitScale : WallHitScale);
	if (!bOnYokai && bScorch)
	{
		FMatsuriFxSpec Scorch;
		Scorch.Shape = EShape::Cylinder;
		Scorch.Color = FLinearColor(0.03f, 0.02f, 0.02f);
		Scorch.Glow = 0.0f;
		Scorch.Rotation = FRotationMatrix::MakeFromZ(Out).Rotator();	// flat against the surface
		Scorch.StartSize = FVector(22.0f, 22.0f, 1.0f);
		Scorch.EndSize = FVector(16.0f, 16.0f, 1.0f);
		Scorch.Lifetime = 1.2f;
		Spawn(World, Location + Out * 1.0f, Scorch);
	}
}

namespace
{
	TAutoConsoleVariable<bool> CVarNiagara(TEXT("matsuri.Niagara"), true,
		TEXT("Layer the engine's Niagara template effects over the code-built ones (0 = code-built only)."));
	int32 TemplatesSpawned = 0;

	const TCHAR* TemplatePath(Matsuri::Fx::ETemplate Which)
	{
		switch (Which)
		{
		case Matsuri::Fx::ETemplate::Explosion:		return TEXT("/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion.SimpleExplosion");
		case Matsuri::Fx::ETemplate::RadialBurst:	return TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst");
		case Matsuri::Fx::ETemplate::Directional:	return TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst");
		}
		return TEXT("");
	}

	UNiagaraSystem* LoadTemplate(Matsuri::Fx::ETemplate Which)
	{
		static TMap<Matsuri::Fx::ETemplate, TWeakObjectPtr<UNiagaraSystem>> Loaded;
		UNiagaraSystem* System = Loaded.FindRef(Which).Get();
		if (!System)
		{
			System = LoadObject<UNiagaraSystem>(nullptr, TemplatePath(Which), nullptr, LOAD_Quiet | LOAD_NoWarn);
			Loaded.Add(Which, System);
		}
		return System;
	}
}

void Matsuri::Fx::Prewarm(UWorld* World, int32 Pieces)
{
	if (!World)
	{
		return;
	}
	FMatsuriFxSpec Hidden;
	Hidden.bVisible = false;
	for (int32 i = 0; i < Pieces; ++i)
	{
		Release(Spawn(World, FVector::ZeroVector, Hidden));
	}
}

void Matsuri::Fx::PreloadTemplates()
{
	if (!CVarNiagara.GetValueOnGameThread())
	{
		return;
	}
	for (ETemplate Which : { ETemplate::Explosion, ETemplate::RadialBurst, ETemplate::Directional })
	{
		if (UNiagaraSystem* System = LoadTemplate(Which))
		{
#if WITH_EDITOR
			System->WaitForCompilationComplete(true, false);
#endif
		}
	}
#if WITH_EDITOR
	FStaticMeshCompilingManager::Get().FinishAllCompilation();	// SimpleExplosion's debris mesh
#endif
}

void Matsuri::Fx::Template(UWorld* World, ETemplate Which, const FVector& Location, const FVector& Direction, float Scale)
{
	if (!World || !CVarNiagara.GetValueOnGameThread())
	{
		return;
	}
	if (UNiagaraSystem* System = LoadTemplate(Which))
	{
		const FRotator Aim = Direction.IsNearlyZero() ? FRotator::ZeroRotator : FRotationMatrix::MakeFromZ(Direction.GetSafeNormal()).Rotator();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, Aim, FVector(Scale));
		++TemplatesSpawned;
	}
}

int32 Matsuri::Fx::TemplateCount()
{
	return TemplatesSpawned;
}
