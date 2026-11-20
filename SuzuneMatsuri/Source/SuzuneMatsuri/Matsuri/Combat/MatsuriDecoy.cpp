// Suzune's Night Festival - 分身, the ninja's decoy.

#include "MatsuriDecoy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MatsuriGraybox.h"

namespace
{
	const FLinearColor DecoyColor(0.35f, 0.45f, 0.95f);	// Suzune's colour, glowing so it reads as a phantom
}

AMatsuriDecoy::AMatsuriDecoy()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = LifeSeconds;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Torso = Matsuri::Graybox::MakePart(*this, Root, TEXT("Torso"), Matsuri::Graybox::EShape::Cylinder,
		FVector(0.0f, 0.0f, 0.0f), FVector(0.45f, 0.45f, 1.3f));
	Head = Matsuri::Graybox::MakePart(*this, Root, TEXT("Head"), Matsuri::Graybox::EShape::Sphere,
		FVector(0.0f, 0.0f, 85.0f), FVector(0.4f));
}

void AMatsuriDecoy::BeginPlay()
{
	Super::BeginPlay();
	Matsuri::Graybox::Paint(Torso, DecoyColor, true, 2.0f);
	Matsuri::Graybox::Paint(Head, DecoyColor, true, 2.0f);
}
