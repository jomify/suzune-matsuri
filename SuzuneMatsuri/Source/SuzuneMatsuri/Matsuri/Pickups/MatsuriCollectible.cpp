// Suzune's Night Festival - koban / ginka / magatama pickup.

#include "MatsuriCollectible.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "MatsuriArt.h"
#include "MatsuriAudio.h"
#include "MatsuriCombat.h"
#include "MatsuriFx.h"
#include "MatsuriGraybox.h"
#include "MatsuriRunSubsystem.h"
#include "SuzuneMatsuri.h"

namespace
{
	constexpr float TriggerRadius = 70.0f;
	constexpr float SpinDegreesPerSecond = 120.0f;

	struct FCollectibleLook
	{
		bool bGem = false;
		FVector Scale = FVector::OneVector;
		FLinearColor Color = FLinearColor::White;
	};

	FCollectibleLook LookFor(EMatsuriCollectible Kind)
	{
		switch (Kind)
		{
		// Coins stand upright: the cylinder axis is rotated to face sideways, X/Y become the coin face.
		case EMatsuriCollectible::Koban:	return { false, FVector(0.60f, 0.38f, 0.06f), FLinearColor(1.00f, 0.66f, 0.08f) };
		case EMatsuriCollectible::Ginka:	return { false, FVector(0.42f, 0.42f, 0.06f), FLinearColor(0.80f, 0.84f, 0.92f) };
		case EMatsuriCollectible::Magatama:	return { true, FVector(0.30f, 0.30f, 0.42f), FLinearColor(0.05f, 0.85f, 0.45f) };
		}
		return {};
	}
}

AMatsuriCollectible::AMatsuriCollectible()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(TriggerRadius);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
	// The Projectile channel blocks by default and OverlapAllDynamic does not override it: kappa
	// cucumbers used to vanish on any coin or talisman lying in their path.
	Trigger->SetCollisionResponseToChannel(Matsuri::Combat::ProjectileChannel, ECR_Ignore);
	RootComponent = Trigger;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	Spinner = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spinner"));
	Spinner->RotationRate = FRotator(0.0f, SpinDegreesPerSecond, 0.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CoinMesh = CylinderFinder.Object;
	GemMesh = SphereFinder.Object;
}

void AMatsuriCollectible::SetKind(EMatsuriCollectible NewKind)
{
	Kind = NewKind;
	ApplyVisuals();
}

void AMatsuriCollectible::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyVisuals();
}

void AMatsuriCollectible::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AMatsuriCollectible::HandleOverlap);
}

void AMatsuriCollectible::ApplyVisuals()
{
	if (Matsuri::Art::Dress(Mesh, Matsuri::ArtTable::Collectible(Kind)))
	{
		return;	// the Tripo coin / gem, textured
	}
	const FCollectibleLook Look = LookFor(Kind);

	Mesh->SetStaticMesh(Look.bGem ? GemMesh.Get() : CoinMesh.Get());
	Mesh->SetRelativeRotation(Look.bGem ? FRotator::ZeroRotator : FRotator(90.0f, 0.0f, 0.0f));
	Mesh->SetRelativeScale3D(Look.Scale);

	// Collectibles glow so they read across a dark floor (falls back to plain if the glow asset is missing).
	Matsuri::Graybox::Paint(Mesh, Look.Color, true, Look.bGem ? 4.0f : 2.5f);
}

void AMatsuriCollectible::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (bCollected || !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogSuzuneMatsuri, Error, TEXT("%s: no run subsystem, pickup ignored"), *GetName());
		return;
	}

	bCollected = true;
	Matsuri::Audio::Play2D(this, Kind == EMatsuriCollectible::Magatama ? EMatsuriSfx::GemPickup : EMatsuriSfx::CoinPickup, 0.8f);
	Matsuri::Fx::Burst(GetWorld(), Mesh->GetComponentLocation(), LookFor(Kind).Color, Kind == EMatsuriCollectible::Magatama ? 18 : 10, 380.0f, 9.0f, 0.5f);
	Run->Collect(Kind);
	Destroy();
}
