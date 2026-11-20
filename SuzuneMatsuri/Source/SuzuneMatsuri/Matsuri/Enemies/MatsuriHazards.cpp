// Suzune's Night Festival - invulnerable hazards.

#include "MatsuriHazards.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MatsuriCombat.h"
#include "MatsuriGraybox.h"

using Matsuri::Graybox::EShape;

namespace
{
	void ConfigureTrigger(UPrimitiveComponent& Trigger)
	{
		Trigger.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Trigger.SetCollisionObjectType(ECC_WorldDynamic);
		Trigger.SetCollisionResponseToAllChannels(ECR_Ignore);
		Trigger.SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);	// shots pass straight through
		Trigger.SetGenerateOverlapEvents(true);
		Trigger.SetCanEverAffectNavigation(false);
	}
}

AMatsuriHazard::AMatsuriHazard()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMatsuriHazard::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AMatsuriHazard::HandleOverlap);
}

void AMatsuriHazard::SetArmed(bool bNewArmed)
{
	if (bArmed == bNewArmed)
	{
		return;
	}
	bArmed = bNewArmed;
	if (!bArmed)
	{
		return;
	}
	TArray<AActor*> Inside;
	Trigger->GetOverlappingActors(Inside);
	if (Inside.ContainsByPredicate([](const AActor* Actor) { return Matsuri::Combat::IsPlayerPawn(Actor); }))
	{
		Matsuri::Combat::HurtPlayer(this, this);
	}
}

void AMatsuriHazard::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bArmed && Matsuri::Combat::IsPlayerPawn(OtherActor))
	{
		Matsuri::Combat::HurtPlayer(this, this);
	}
}

// ---------------------------------------------------------------- Foxfire

namespace FoxfireTuning
{
	constexpr float Radius = 40.0f;
	constexpr float Height = 110.0f;
	constexpr float SweepHalfLength = 420.0f;
	constexpr float AngularSpeed = 0.9f;	// rad/s along the sweep
	const FLinearColor FlameColor(0.45f, 0.30f, 1.0f);	// violet-blue = danger
}

AMatsuriFoxfire::AMatsuriFoxfire()
{
	USphereComponent* Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Sphere->InitSphereRadius(FoxfireTuning::Radius);
	ConfigureTrigger(*Sphere);
	RootComponent = Sphere;
	Trigger = Sphere;

	Flame = Matsuri::Graybox::MakePart(*this, Sphere, TEXT("Flame"), EShape::Sphere, FVector::ZeroVector, FVector(0.6f, 0.6f, 0.9f));
	Flame->SetCastShadow(false);
}

void AMatsuriFoxfire::BeginPlay()
{
	Super::BeginPlay();
	Matsuri::Graybox::Paint(Flame, FoxfireTuning::FlameColor, true, 12.0f);
	Origin = FVector(GetActorLocation().X, GetActorLocation().Y, FoxfireTuning::Height);
	FRandomStream Rng(GetTypeHash(Origin.ToString()));
	Axis = FVector(Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal();
	Phase = Rng.FRandRange(0.0f, 2.0f * PI);
}

void AMatsuriFoxfire::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Phase += DeltaSeconds * FoxfireTuning::AngularSpeed;
	const FVector Bob(0.0f, 0.0f, FMath::Sin(Phase * 3.0f) * 15.0f);
	SetActorLocation(Origin + Axis * FMath::Sin(Phase) * FoxfireTuning::SweepHalfLength + Bob);
}

// ---------------------------------------------------------------- Spikes

namespace SpikeTuning
{
	constexpr float CycleSeconds = 3.0f;
	constexpr float WarnAt = 1.4f;		// plate turns red
	constexpr float RiseAt = 2.0f;		// spikes up and armed
	constexpr float PlateSize = 1.6f;	// m
	constexpr float SpikeUp = 0.0f;		// cone (0.8 m tall) centred mid-trigger: tips at ~85 cm
	constexpr float SpikeDown = -82.0f;	// fully below the floor
	const FLinearColor PlateIdle(0.25f, 0.24f, 0.22f);
	const FLinearColor PlateWarn(0.85f, 0.10f, 0.05f);
	const FLinearColor SpikeColor(0.75f, 0.75f, 0.78f);
}

AMatsuriSpikeTrap::AMatsuriSpikeTrap()
{
	UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Box->InitBoxExtent(FVector(70.0f, 70.0f, 45.0f));
	ConfigureTrigger(*Box);
	RootComponent = Box;
	Trigger = Box;
	bArmed = false;

	Plate = Matsuri::Graybox::MakePart(*this, Box, TEXT("Plate"), EShape::Cube, FVector(0.0f, 0.0f, -43.0f),
		FVector(SpikeTuning::PlateSize, SpikeTuning::PlateSize, 0.04f));
	const FVector2D Corners[] = { { -35.0f, -35.0f }, { 35.0f, -35.0f }, { -35.0f, 35.0f }, { 35.0f, 35.0f } };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Corners); ++i)
	{
		Spikes.Add(Matsuri::Graybox::MakePart(*this, Box, *FString::Printf(TEXT("Spike%d"), i), EShape::Cone,
			FVector(Corners[i], SpikeTuning::SpikeDown), FVector(0.35f, 0.35f, 0.8f)));
	}
}

void AMatsuriSpikeTrap::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, 45.0f));
	Matsuri::Graybox::Paint(Plate, SpikeTuning::PlateIdle);
	for (UStaticMeshComponent* Spike : Spikes)
	{
		Matsuri::Graybox::Paint(Spike, SpikeTuning::SpikeColor);
	}
	CycleTime = FRandomStream(GetTypeHash(GetActorLocation().ToString())).FRandRange(0.0f, SpikeTuning::CycleSeconds);
}

void AMatsuriSpikeTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CycleTime = FMath::Fmod(CycleTime + DeltaSeconds, SpikeTuning::CycleSeconds);

	const bool bShouldWarn = CycleTime >= SpikeTuning::WarnAt;
	if (bShouldWarn != bWarning)
	{
		bWarning = bShouldWarn;
		Matsuri::Graybox::Paint(Plate, bWarning ? SpikeTuning::PlateWarn : SpikeTuning::PlateIdle);
	}

	const bool bUp = CycleTime >= SpikeTuning::RiseAt;
	for (UStaticMeshComponent* Spike : Spikes)
	{
		const FVector Offset = Spike->GetRelativeLocation();
		Spike->SetRelativeLocation(FVector(Offset.X, Offset.Y, bUp ? SpikeTuning::SpikeUp : SpikeTuning::SpikeDown));
	}
	SetArmed(bUp);
}
