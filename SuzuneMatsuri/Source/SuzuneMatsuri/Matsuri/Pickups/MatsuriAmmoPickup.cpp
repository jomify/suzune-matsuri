// Suzune's Night Festival - 弾薬御札, the ammo talisman.

#include "MatsuriAmmoPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "TimerManager.h"
#include "MatsuriArt.h"
#include "MatsuriAudio.h"
#include "MatsuriCombat.h"
#include "MatsuriFx.h"
#include "MatsuriGraybox.h"
#include "MatsuriRunSubsystem.h"

namespace
{
	constexpr float TriggerRadius = 70.0f;
	constexpr float RespawnSeconds = 25.0f;
	constexpr float DropLifeSeconds = 20.0f;
	constexpr float HoverHeight = 80.0f;
	const FLinearColor TalismanColor(0.10f, 0.75f, 1.00f);	// blue = ammo in the palette
}

AMatsuriAmmoPickup::AMatsuriAmmoPickup()
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

	Talisman = Matsuri::Graybox::MakePart(*this, Trigger, TEXT("Talisman"), Matsuri::Graybox::EShape::Cube,
		FVector(0.0f, 0.0f, HoverHeight - TriggerRadius), FVector(0.04f, 0.26f, 0.42f));

	Spinner = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spinner"));
	Spinner->RotationRate = FRotator(0.0f, 90.0f, 0.0f);
}

void AMatsuriAmmoPickup::Configure(int32 InAmount, bool bInRespawns)
{
	Amount = FMath::Max(1, InAmount);
	bRespawns = bInRespawns;
	InitialLifeSpan = bRespawns ? 0.0f : DropLifeSeconds;
}

void AMatsuriAmmoPickup::BeginPlay()
{
	Super::BeginPlay();
	if (!Matsuri::Art::Dress(Talisman, Matsuri::ArtTable::AmmoTalisman()))
	{
		Matsuri::Graybox::Paint(Talisman, TalismanColor, true, 3.0f);
	}
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AMatsuriAmmoPickup::HandleOverlap);
}

void AMatsuriAmmoPickup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	if (!bAvailable || !Run || !Matsuri::Combat::IsPlayerPawn(OtherActor))
	{
		return;
	}
	if (Run->AddAmmo(Amount) == 0)
	{
		return;	// already full: leave it for later
	}
	Matsuri::Fx::Burst(GetWorld(), Talisman->GetComponentLocation(), TalismanColor, 8, 300.0f, 8.0f, 0.4f);
	Matsuri::Audio::Play2D(this, EMatsuriSfx::TalismanPickup);

	if (!bRespawns)
	{
		Destroy();
		return;
	}
	SetAvailable(false);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMatsuriAmmoPickup::Respawn, RespawnSeconds);
}

void AMatsuriAmmoPickup::SetAvailable(bool bNewAvailable)
{
	bAvailable = bNewAvailable;
	Talisman->SetVisibility(bNewAvailable);
	Trigger->SetCollisionEnabled(bNewAvailable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AMatsuriAmmoPickup::Respawn()
{
	SetAvailable(true);
}
