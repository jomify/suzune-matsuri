// Suzune's Night Festival - a damage number that pops off a struck yokai.

#include "MatsuriDamageNumber.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	constexpr float LifeSeconds = 0.75f;
	constexpr float PopSeconds = 0.08f;		// springs to 1.5x, then settles
	constexpr float RiseSpeed = 140.0f;		// cm/s at first, slowing
	constexpr float NormalSize = 30.0f;		// world size of the glyphs, cm
	constexpr float StrongSize = 46.0f;
	constexpr float KillSize = 52.0f;
	constexpr float StrongDamage = 2.5f;	// a full-draw arrow, a zeni coin
	const FColor NormalColor(255, 250, 235);
	const FColor StrongColor(255, 196, 64);
	const FColor KillColor(255, 70, 60);
}

AMatsuriDamageNumber::AMatsuriDamageNumber()
{
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = LifeSeconds;
	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Text->SetCastShadow(false);
	RootComponent = Text;
}

AMatsuriDamageNumber* AMatsuriDamageNumber::Pop(UWorld* World, const FVector& Location, float Damage, bool bStrong, bool bKill)
{
	if (!World)
	{
		return nullptr;
	}
	AMatsuriDamageNumber* Number = World->SpawnActor<AMatsuriDamageNumber>(Location + FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator);
	if (!Number)
	{
		return nullptr;
	}
	const bool bWhole = FMath::IsNearlyEqual(Damage, FMath::RoundToFloat(Damage), 0.05f);
	Number->Text->SetText(FText::FromString(bWhole ? FString::FromInt(FMath::RoundToInt(Damage)) : FString::Printf(TEXT("%.1f"), Damage)));
	const bool bBig = bStrong || Damage >= StrongDamage;
	Number->Text->SetTextRenderColor(bKill ? KillColor : bBig ? StrongColor : NormalColor);
	Number->BaseSize = bKill ? KillSize : bBig ? StrongSize : NormalSize;
	Number->Drift = FVector(FMath::FRandRange(-25.0f, 25.0f), FMath::FRandRange(-25.0f, 25.0f), 0.0f);	// numbers of a burst do not stack
	Number->Text->SetWorldSize(Number->BaseSize * 1.5f);
	return Number;
}

void AMatsuriDamageNumber::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / LifeSeconds, 0.0f, 1.0f);
	AddActorWorldOffset((FVector(0.0f, 0.0f, RiseSpeed) + Drift) * (1.0f - T) * DeltaSeconds);

	// Pop, hold, then shrink away over the last third.
	const float Pop = Age < PopSeconds ? FMath::Lerp(1.5f, 1.0f, Age / PopSeconds) : 1.0f;
	const float Fade = T < 0.66f ? 1.0f : FMath::Lerp(1.0f, 0.0f, (T - 0.66f) / 0.34f);
	Text->SetWorldSize(BaseSize * Pop * Fade);

	// Face the camera.
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - GetActorLocation();
		SetActorRotation(ToCamera.Rotation());
	}
}
