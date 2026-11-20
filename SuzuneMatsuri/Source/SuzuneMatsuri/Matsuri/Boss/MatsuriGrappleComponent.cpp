// Suzune's Night Festival - 鉤縄, the grapple: F pulls the player to the hook ring under the crosshair.

#include "MatsuriGrappleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "MatsuriArenaPieces.h"
#include "MatsuriAudio.h"
#include "MatsuriGraybox.h"

namespace
{
	constexpr int32 MappingPriority = 2;
	constexpr float RopeThickness = 0.035f;		// of the 1 m cylinder
	constexpr float HandDrop = 35.0f;			// the rope leaves from below the eye, a little right
	constexpr float HandRight = 25.0f;
	constexpr float ReleaseUp = 350.0f;			// the pop onto the perch
	constexpr float ReleaseForward = 400.0f;
	constexpr float JumpOffUp = 300.0f;
	const FLinearColor RopeColor(1.0f, 0.85f, 0.45f);
}

UMatsuriGrappleComponent::UMatsuriGrappleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMatsuriGrappleComponent::BeginPlay()
{
	Super::BeginPlay();
	BindInput();
	Rope = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("MatsuriGrappleRope"));
	Rope->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cylinder));
	Rope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rope->SetCastShadow(false);
	Rope->SetMobility(EComponentMobility::Movable);
	Rope->SetUsingAbsoluteLocation(true);
	Rope->SetUsingAbsoluteRotation(true);
	Rope->SetUsingAbsoluteScale(true);
	Rope->SetupAttachment(GetOwner()->GetRootComponent());
	Rope->RegisterComponent();
	Rope->SetVisibility(false);
	Matsuri::Graybox::Paint(Rope, RopeColor, true, 6.0f);
}

void UMatsuriGrappleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindInput();
	Super::EndPlay(EndPlayReason);
}

void UMatsuriGrappleComponent::BindInput()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Input || !Subsystem)
	{
		return;	// not a local player (tests drive TryGrapple directly)
	}
	GrappleAction = NewObject<UInputAction>(this, TEXT("IA_MatsuriGrapple"));
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_MatsuriGrapple"));
	Mapping->MapKey(GrappleAction, EKeys::F);
	Mapping->MapKey(GrappleAction, EKeys::Gamepad_LeftShoulder);
	Subsystem->AddMappingContext(Mapping, MappingPriority);
	BindingHandles.Add(Input->BindAction(GrappleAction, ETriggerEvent::Started, this, &UMatsuriGrappleComponent::HandleGrappleInput).GetHandle());
}

void UMatsuriGrappleComponent::UnbindInput()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		for (const uint32 Handle : BindingHandles)
		{
			Input->RemoveBindingByHandle(Handle);
		}
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(Mapping);
	}
	BindingHandles.Reset();
}

AMatsuriHookPoint* UMatsuriGrappleComponent::FindTarget() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return nullptr;
	}
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	const FVector Forward = View.Vector();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(AimConeDegrees));

	FCollisionQueryParams Params(TEXT("MatsuriGrapple"), false, Pawn);
	AMatsuriHookPoint* Best = nullptr;
	float BestDot = MinDot;
	for (TActorIterator<AMatsuriHookPoint> It(GetWorld()); It; ++It)
	{
		const FVector ToHook = It->GetActorLocation() - Eye;
		const float Distance = ToHook.Size();
		const float Dot = Distance > KINDA_SMALL_NUMBER ? FVector::DotProduct(ToHook / Distance, Forward) : -1.0f;
		if (Distance > Range || Dot < BestDot)
		{
			continue;
		}
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, It->GetActorLocation(), ECC_Visibility, Params))
		{
			continue;	// something in between (the ring itself has no collision)
		}
		Best = *It;
		BestDot = Dot;
	}
	return Best;
}

bool UMatsuriGrappleComponent::TryGrapple()
{
	ACharacter* Hero = Cast<ACharacter>(GetOwner());
	AMatsuriHookPoint* Hook = Target.Get() ? Target.Get() : FindTarget();
	if (!Hero || !Hook || bPulling || GetWorld()->GetTimeSeconds() < ReadyAt)
	{
		return false;
	}
	Target = Hook;
	Destination = Hook->GetActorLocation() - FVector(0.0f, 0.0f, BelowRing);
	bPulling = true;
	PullTime = 0.0f;
	++Uses;
	Hero->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	Matsuri::Audio::Play2D(this, EMatsuriSfx::GrappleFire, 0.8f);
	UpdateRope();
	return true;
}

void UMatsuriGrappleComponent::Release(bool bKeepSpeed)
{
	ACharacter* Hero = Cast<ACharacter>(GetOwner());
	if (!bPulling || !Hero)
	{
		return;
	}
	bPulling = false;
	ReadyAt = GetWorld()->GetTimeSeconds() + Cooldown;
	UCharacterMovementComponent* Movement = Hero->GetCharacterMovement();
	const FVector Heading = (Destination - Hero->GetActorLocation()).GetSafeNormal2D();
	Movement->SetMovementMode(MOVE_Falling);
	Movement->Velocity = bKeepSpeed ? Movement->Velocity + FVector(0.0f, 0.0f, JumpOffUp) : Heading * ReleaseForward + FVector(0.0f, 0.0f, ReleaseUp);
	Rope->SetVisibility(false);
}

void UMatsuriGrappleComponent::UpdateRope()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const AMatsuriHookPoint* Hook = Target.Get();
	if (!bPulling || !Hook)
	{
		Rope->SetVisibility(false);
		return;
	}
	FVector Eye = Pawn->GetActorLocation();
	FRotator View = Pawn->GetActorRotation();
	if (PC)
	{
		PC->GetPlayerViewPoint(Eye, View);
	}
	const FVector Hand = Eye - FVector(0.0f, 0.0f, HandDrop) + FRotationMatrix(View).GetUnitAxis(EAxis::Y) * HandRight;
	const FVector Span = Hook->GetActorLocation() - Hand;
	Rope->SetWorldLocationAndRotation((Hand + Hook->GetActorLocation()) * 0.5f, FRotationMatrix::MakeFromZ(Span.GetSafeNormal()).Rotator());
	Rope->SetWorldScale3D(FVector(RopeThickness, RopeThickness, Span.Size() / 100.0f));
	Rope->SetVisibility(true);
}

void UMatsuriGrappleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Hero = Cast<ACharacter>(GetOwner());
	if (!Hero)
	{
		return;
	}
	if (!bPulling)
	{
		AMatsuriHookPoint* Now = FindTarget();
		if (Now != Target.Get())
		{
			if (AMatsuriHookPoint* Old = Target.Get())
			{
				Old->SetHighlighted(false);
			}
			if (Now)
			{
				Now->SetHighlighted(true);
			}
			Target = Now;
		}
		return;
	}
	PullTime += DeltaTime;
	if (Hero->bPressedJump)
	{
		Release(true);	// let go with the speed: a slingshot
		return;
	}
	const FVector ToGo = Destination - Hero->GetActorLocation();
	if (ToGo.Size() < ArriveDistance || PullTime > MaxPullSeconds)
	{
		Release(false);
		return;
	}
	Hero->GetCharacterMovement()->Velocity = ToGo.GetSafeNormal() * PullSpeed;
	UpdateRope();
}
