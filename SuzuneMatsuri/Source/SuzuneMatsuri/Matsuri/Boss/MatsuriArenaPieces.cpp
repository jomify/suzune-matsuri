// Suzune's Night Festival - the boss courtyard's live pieces: fences that break, rings the grapple holds.

#include "MatsuriArenaPieces.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "MatsuriAudio.h"
#include "MatsuriFx.h"
#include "MatsuriGraybox.h"
#include "MatsuriShrineProps.h"

namespace
{
	constexpr float CubeSize = 100.0f;
	const FLinearColor FenceColor(0.42f, 0.2f, 0.08f);
	const FLinearColor HookColor(1.0f, 0.85f, 0.45f);
	constexpr float HookGlow = 4.0f;
	constexpr float HookGlowHighlighted = 14.0f;
	constexpr float HookLight = 1500.0f;
	constexpr float HookSpinDegrees = 40.0f;
}

AMatsuriBreakableCover::AMatsuriBreakableCover()
{
	Boards = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Boards"));
	Boards->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cube));
	Boards->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Boards->SetMobility(EComponentMobility::Movable);
	RootComponent = Boards;
}

void AMatsuriBreakableCover::SetExtent(const FVector& InExtent)
{
	Extent = InExtent;
	Boards->SetRelativeScale3D(Extent * 2.0f / CubeSize);
}

void AMatsuriBreakableCover::BeginPlay()
{
	Super::BeginPlay();
	Matsuri::Graybox::PaintSurface(Boards, Matsuri::Graybox::ESurface::Wood, FenceColor);
	float Yaw = 0.0f;
	const FVector Scale = Matsuri::ShrineProps::FitScale(TEXT("SM_Wall_01"), Extent * 2.0f, Yaw);
	Dressing = Matsuri::ShrineProps::Place(*GetWorld(), TEXT("SM_Wall_01"), GetActorLocation() - FVector(0.0f, 0.0f, Extent.Z), Yaw, Scale);
	Boards->SetVisibility(bStanding && !Dressing);	// with the pack, the wall is what you see; the boards keep the collision
}

FBox AMatsuriBreakableCover::GetBounds() const
{
	return FBox(GetActorLocation() - Extent, GetActorLocation() + Extent);
}

void AMatsuriBreakableCover::AbsorbBullet()
{
	if (!bStanding)
	{
		return;
	}
	if (++Hits < MaxHits)
	{
		return;
	}
	SetStanding(false);
	Matsuri::Fx::Burst(GetWorld(), GetActorLocation(), FenceColor, 14, 520.0f, 16.0f, 0.6f);
	Matsuri::Audio::PlayAt(this, EMatsuriSfx::ArrowThunk, GetActorLocation(), 1.0f);
	GetWorldTimerManager().SetTimer(RegrowTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		Hits = 0;
		SetStanding(true);
		Matsuri::Fx::Burst(GetWorld(), GetActorLocation(), FLinearColor(0.5f, 1.0f, 0.5f), 8, 200.0f, 8.0f, 0.5f);
	}), RegrowSeconds, false);
}

void AMatsuriBreakableCover::SetStanding(bool bNewStanding)
{
	bStanding = bNewStanding;
	Boards->SetVisibility(bStanding && !Dressing);
	if (Dressing)
	{
		Dressing->SetActorHiddenInGame(!bStanding);
	}
	Boards->SetCollisionEnabled(bStanding ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

AMatsuriHookPoint::AMatsuriHookPoint()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add(TEXT("MatsuriHook"));

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cylinder));
	Ring->SetRelativeScale3D(FVector(1.1f, 1.1f, 0.08f));
	Ring->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));	// stands upright, like a hanging wreath
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);
	RootComponent = Ring;

	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Ring);
	Core->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Sphere));
	Core->SetRelativeScale3D(FVector(0.35f / 1.1f, 0.35f / 1.1f, 0.35f / 0.08f));	// a round bead despite the ring's scale
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Core->SetCastShadow(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Ring);
	Light->SetIntensity(HookLight);
	Light->SetAttenuationRadius(600.0f);
	Light->SetLightColor(HookColor);
	Light->SetCastShadows(false);
}

void AMatsuriHookPoint::BeginPlay()
{
	Super::BeginPlay();
	Matsuri::Graybox::Paint(Ring, HookColor, true, HookGlow);
	Matsuri::Graybox::Paint(Core, FLinearColor::White, true, HookGlow);
	RingMaterial = Cast<UMaterialInstanceDynamic>(Ring->GetMaterial(0));
}

void AMatsuriHookPoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Pulse += DeltaSeconds;
	AddActorWorldRotation(FRotator(0.0f, HookSpinDegrees * DeltaSeconds, 0.0f));
	const float Glow = bHighlighted ? HookGlowHighlighted : HookGlow * (0.8f + 0.2f * FMath::Sin(Pulse * 3.0f));
	if (RingMaterial)
	{
		RingMaterial->SetScalarParameterValue(TEXT("Glow"), Glow);
	}
	Light->SetIntensity(bHighlighted ? HookLight * 2.5f : HookLight);
}
