// Suzune's Night Festival - the heroine's weapon held in view.

#include "MatsuriViewModel.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MatsuriArt.h"
#include "MatsuriAudio.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriGraybox.h"
#include "MatsuriRunSubsystem.h"
#include "SuzuneMatsuriCharacter.h"
#include "Components/CapsuleComponent.h"

namespace
{
	// The camera itself stays steady; only the weapon in her hand moves (playtest: the view swayed).
	constexpr float SwayPerDegree = 0.18f;		// degrees of lag per degree turned this frame
	constexpr float SwayMax = 2.5f;
	constexpr float SwayReturn = 16.0f;			// 1/s
	constexpr float BobCyclesPerSecond = 1.6f;	// steps per second at full speed (one bob per step)
	constexpr float BobSide = 1.8f;				// cm: the hand rocks with her stride
	constexpr float BobDrop = 1.4f;				// cm
	constexpr float BobRoll = 2.5f;				// degrees
	constexpr float KickBack = 3.5f;			// cm
	constexpr float KickPitch = 7.0f;			// degrees
	constexpr float KickRecover = 16.0f;		// 1/s
	constexpr float SwingSeconds = 0.28f;
	constexpr float SwingYaw = 55.0f;			// degrees across the view
	constexpr float SwingReach = 8.0f;			// cm forward at the middle of the swing
	constexpr float DrawBack = 3.0f;			// cm the bow itself comes back toward her at full draw
	constexpr float StringPull = 12.0f;			// cm the string and arrow are drawn back at full draw
	// The draw's tension is felt in the view, not by filling it (after Hanzo): the view narrows a little,
	// its edges darken, the bow trembles at full draw and the arrowhead gathers a gold glow.
	constexpr float DrawZoomDegrees = 8.0f;
	constexpr float VignetteRest = 0.4f;		// the engine's default
	constexpr float VignetteFullDraw = 1.15f;
	constexpr float TrembleCm = 0.18f;
	constexpr float TipGlowIntensity = 1800.0f;
	const FLinearColor TipGlowColor(1.0f, 0.78f, 0.3f);
	constexpr float NockedArrowScale = 0.45f;	// the arrow on the string is drawn shorter than the one in flight: it stays clear of the view
	constexpr float StringThickness = 0.5f;		// cm
}

UMatsuriViewModelComponent::UMatsuriViewModelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

UMatsuriViewModelComponent* UMatsuriViewModelComponent::Equip(ACharacter& Hero, EMatsuriHero Heroine)
{
	if (UMatsuriViewModelComponent* Existing = Hero.FindComponentByClass<UMatsuriViewModelComponent>())
	{
		return Existing;
	}
	UCameraComponent* Camera = Hero.FindComponentByClass<UCameraComponent>();
	if (!Camera)
	{
		return nullptr;
	}
	// A steady eye: the template hangs the camera on its mannequin's head bone, which bobs with every step.
	Camera->AttachToComponent(Hero.GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Camera->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, EyeHeight), FRotator::ZeroRotator);

	const FMatsuriArtPose Pose = Matsuri::ArtTable::ViewWeapon(Heroine);
	UStaticMesh* Mesh = Matsuri::Art::Mesh(Pose.Mesh);
	if (!Mesh)
	{
		return nullptr;
	}

	UMatsuriViewModelComponent* View = NewObject<UMatsuriViewModelComponent>(&Hero, TEXT("MatsuriViewModel"));
	View->Pose = Pose;
	View->SetupAttachment(Camera);
	View->SetRelativeLocation(Pose.Offset);

	UStaticMeshComponent* Weapon = NewObject<UStaticMeshComponent>(&Hero, TEXT("MatsuriViewWeapon"));
	Weapon->SetStaticMesh(Mesh);
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Weapon->SetCastShadow(false);
	Weapon->SetOnlyOwnerSee(true);
	Weapon->SetupAttachment(View);
	// Centre the mesh on the grip point, then pose it.
	const FBox Box = Mesh->GetBoundingBox();
	const FTransform Grip(Pose.Rotation, FVector::ZeroVector, FVector(Pose.Scale));
	Weapon->SetRelativeTransform(FTransform(Pose.Rotation, -Grip.TransformVector(Box.GetCenter()), FVector(Pose.Scale)));
	View->Weapon = Weapon;

	// The tip: the posed mesh's furthest point forward, level with its middle.
	const FBox Posed = Box.TransformBy(Weapon->GetRelativeTransform());
	View->MuzzleLocal = FVector(Posed.Max.X, Posed.GetCenter().Y, Posed.GetCenter().Z);

	View->RegisterComponent();
	Weapon->RegisterComponent();

	if (Matsuri::ArtTable::IsBow(Heroine))
	{
		View->bBow = true;
		View->NockLocal = FVector(Box.Min.X + 1.0f, Box.GetCenter().Y, Box.GetCenter().Z);
		View->TopTipLocal = FVector(Box.Min.X + 1.0f, Box.GetCenter().Y, Box.Max.Z - Box.GetSize().Z * 0.03f);
		View->BottomTipLocal = FVector(Box.Min.X + 1.0f, Box.GetCenter().Y, Box.Min.Z + Box.GetSize().Z * 0.03f);
		const FMatsuriArtPose ArrowPose = Matsuri::ArtTable::Shot(Heroine);
		auto MakePiece = [&Hero, View](const TCHAR* Name, UStaticMesh* PieceMesh)
		{
			UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(&Hero, Name);
			Piece->SetStaticMesh(PieceMesh);
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Piece->SetCastShadow(false);
			Piece->SetOnlyOwnerSee(true);
			Piece->SetupAttachment(View);
			Piece->SetUsingAbsoluteLocation(true);	// posed in world space every frame
			Piece->SetUsingAbsoluteRotation(true);
			Piece->SetUsingAbsoluteScale(true);
			Piece->RegisterComponent();
			return Piece;
		};
		if (UStaticMesh* ArrowMesh = Matsuri::Art::Mesh(ArrowPose.Mesh))
		{
			View->ArrowLength = ArrowMesh->GetBoundingBox().GetSize().X * NockedArrowScale;
			View->Arrow = MakePiece(TEXT("MatsuriNockedArrow"), ArrowMesh);
			View->Arrow->SetWorldScale3D(FVector(NockedArrowScale));
		}
		View->TipGlow = NewObject<UPointLightComponent>(&Hero, TEXT("MatsuriArrowGlow"));
		View->TipGlow->SetupAttachment(View);
		View->TipGlow->SetUsingAbsoluteLocation(true);
		View->TipGlow->SetIntensity(0.0f);
		View->TipGlow->SetLightColor(TipGlowColor);
		View->TipGlow->SetAttenuationRadius(120.0f);
		View->TipGlow->SetCastShadows(false);
		View->TipGlow->RegisterComponent();
		View->RestFieldOfView = Camera->FieldOfView;
		View->StringTop = MakePiece(TEXT("MatsuriStringTop"), Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cylinder));
		View->StringBottom = MakePiece(TEXT("MatsuriStringBottom"), Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cylinder));
		Matsuri::Graybox::Paint(View->StringTop, FLinearColor(0.95f, 0.93f, 0.88f));
		Matsuri::Graybox::Paint(View->StringBottom, FLinearColor(0.95f, 0.93f, 0.88f));
	}

	// The weapon is shown on its own: the template's grey mannequin arms go away.
	if (ASuzuneMatsuriCharacter* Template = Cast<ASuzuneMatsuriCharacter>(&Hero))
	{
		Template->GetFirstPersonMesh()->SetVisibility(false);
		Template->GetFirstPersonMesh()->SetComponentTickEnabled(false);
	}
	return View;
}

void UMatsuriViewModelComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UMatsuriCombatComponent* Found = GetOwner()->FindComponentByClass<UMatsuriCombatComponent>())
	{
		Combat = Found;
		ShotHandle = Found->OnShot.AddUObject(this, &UMatsuriViewModelComponent::Kick);
		SwingHandle = Found->OnSwing.AddUObject(this, &UMatsuriViewModelComponent::Swing);
	}
}

void UMatsuriViewModelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMatsuriCombatComponent* Found = Combat.Get())
	{
		Found->OnShot.Remove(ShotHandle);
		Found->OnSwing.Remove(SwingHandle);
	}
	Super::EndPlay(EndPlayReason);
}

FVector UMatsuriViewModelComponent::GetMuzzleLocation() const
{
	if (bBow && ArrowLength > 0.0f)
	{
		return NockWorld + GetForwardVector() * ArrowLength;	// the nocked arrow's head
	}
	return GetComponentTransform().TransformPosition(MuzzleLocal);
}

bool UMatsuriViewModelComponent::IsArrowNocked() const
{
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const float Interval = Run ? Run->GetRunState().GetHero().Weapon.FireInterval : 0.0f;
	return Arrow && Run && Run->GetRunState().Ammo > 0 && GetWorld()->GetTimeSeconds() - LastShotTime >= Interval;
}

void UMatsuriViewModelComponent::PoseBow(float Draw)
{
	// The string's middle comes back toward her with the draw; the arrow lies along the view from it.
	const FTransform& BowToWorld = Weapon->GetComponentTransform();
	const FVector Forward = GetForwardVector();
	NockWorld = BowToWorld.TransformPosition(NockLocal) - Forward * (StringPull * Draw);
	auto Span = [this](UStaticMeshComponent* Piece, const FVector& From, const FVector& To)
	{
		const FVector Along = To - From;
		Piece->SetWorldLocationAndRotation((From + To) * 0.5f, FQuat::FindBetweenNormals(FVector::UpVector, Along.GetSafeNormal()));
		Piece->SetWorldScale3D(FVector(StringThickness, StringThickness, Along.Size()) / 100.0f);	// engine cylinder: 1 m
	};
	Span(StringTop, BowToWorld.TransformPosition(TopTipLocal), NockWorld);
	Span(StringBottom, BowToWorld.TransformPosition(BottomTipLocal), NockWorld);
	if (Arrow)
	{
		Arrow->SetVisibility(IsArrowNocked());
		Arrow->SetWorldLocationAndRotation(NockWorld + Forward * (ArrowLength * 0.5f), GetComponentRotation());
	}

	// Tension in the view itself.
	if (TipGlow)
	{
		TipGlow->SetWorldLocation(NockWorld + Forward * ArrowLength);
		TipGlow->SetIntensity(IsArrowNocked() ? TipGlowIntensity * Draw * Draw : 0.0f);
	}
	if (UCameraComponent* Camera = Cast<UCameraComponent>(GetAttachParent()))
	{
		Camera->SetFieldOfView(RestFieldOfView - DrawZoomDegrees * Draw);
		Camera->PostProcessBlendWeight = 1.0f;
		Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
		Camera->PostProcessSettings.VignetteIntensity = FMath::Lerp(VignetteRest, VignetteFullDraw, Draw);
	}
}

float UMatsuriViewModelComponent::GetDrawZoom() const
{
	const UCameraComponent* Camera = Cast<UCameraComponent>(GetAttachParent());
	return Camera && RestFieldOfView > 0.0f ? RestFieldOfView - Camera->FieldOfView : 0.0f;
}



void UMatsuriViewModelComponent::Kick()
{
	LastShotTime = GetWorld()->GetTimeSeconds();
	KickAlpha = 1.0f;
}

void UMatsuriViewModelComponent::Swing()
{
	SwingAge = 0.0f;
}

void UMatsuriViewModelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ApplyPose(DeltaTime);
}

void UMatsuriViewModelComponent::ApplyPose(float DeltaTime)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || DeltaTime <= 0.0f)
	{
		return;
	}

	// Sway: the weapon trails a fast turn a little, then catches up.
	const FRotator View = Pawn->GetControlRotation();
	if (bHasLastView)
	{
		const FRotator Turn = (View - LastView).GetNormalized();
		const FRotator Target(FMath::Clamp(-Turn.Pitch * SwayPerDegree, -SwayMax, SwayMax), FMath::Clamp(-Turn.Yaw * SwayPerDegree, -SwayMax, SwayMax), 0.0f);
		Sway = FMath::RInterpTo(Sway, Target, DeltaTime, SwayReturn);
	}
	LastView = View;
	bHasLastView = true;

	// Bob: only while walking on the ground, scaled by speed.
	const ACharacter* Hero = Cast<ACharacter>(Pawn);
	const UCharacterMovementComponent* Movement = Hero ? Hero->GetCharacterMovement() : nullptr;
	const float SpeedRatio = Movement && Movement->IsMovingOnGround() && Movement->MaxWalkSpeed > 0.0f
		? FMath::Clamp(static_cast<float>(Pawn->GetVelocity().Size2D() / Movement->MaxWalkSpeed), 0.0f, 1.0f) : 0.0f;
	BobWeight = FMath::FInterpTo(BobWeight, SpeedRatio, DeltaTime, 8.0f);
	const float StepBefore = FMath::Floor(BobPhase / PI);
	BobPhase = FMath::Fmod(BobPhase + DeltaTime * BobCyclesPerSecond * PI * FMath::Max(SpeedRatio, 0.3f), 2.0f * PI);
	if (SpeedRatio > 0.2f && FMath::Floor(BobPhase / PI) != StepBefore)
	{
		Matsuri::Audio::Play2D(this, EMatsuriSfx::Footstep, 0.35f * SpeedRatio);	// a footfall at the bottom of each bob
	}
	// A figure of eight: side to side once per two steps, down on each footfall, rolling with the stride.
	const FVector Bob(0.0f, FMath::Sin(BobPhase) * BobSide * BobWeight, -FMath::Abs(FMath::Sin(BobPhase)) * BobDrop * BobWeight);
	const FRotator BobTilt(0.0f, 0.0f, FMath::Sin(BobPhase) * BobRoll * BobWeight);

	// Recoil kick per shot, recovering quickly.
	KickAlpha *= FMath::Exp(-KickRecover * DeltaTime);	// the same recovery at any frame rate

	// Melee swing: across the view and back.
	FRotator SwingRot = FRotator::ZeroRotator;
	FVector SwingOffset = FVector::ZeroVector;
	if (SwingAge >= 0.0f)
	{
		SwingAge += DeltaTime;
		const float T = FMath::Clamp(SwingAge / SwingSeconds, 0.0f, 1.0f);
		const float Arc = FMath::Sin(T * PI);
		SwingRot = FRotator(-10.0f * Arc, FMath::Lerp(SwingYaw * 0.5f, -SwingYaw * 0.5f, T) * Arc * 2.0f, -25.0f * Arc);
		SwingOffset = FVector(SwingReach * Arc, -6.0f * Arc, 2.0f * Arc);
		if (T >= 1.0f)
		{
			SwingAge = -1.0f;
		}
	}

	// Sakura's bow comes back toward the eye as the shot charges.
	const UMatsuriCombatComponent* CombatComponent = Combat.Get();
	const float Draw = CombatComponent ? CombatComponent->GetChargeAlpha() : 0.0f;

	// At full draw the held bow trembles a little: the strain of holding it.
	const float Time = GetWorld()->GetTimeSeconds();
	const FVector Tremble = Draw >= 1.0f ? FVector(0.0f, FMath::Sin(Time * 31.0f), FMath::Sin(Time * 23.0f)) * TrembleCm : FVector::ZeroVector;
	const FVector Offset = Pose.Offset + Bob + SwingOffset + Tremble + FVector(-KickBack * KickAlpha - DrawBack * Draw, 0.0f, 0.0f);
	const FRotator Rotation = Sway + SwingRot + BobTilt + FRotator(KickPitch * KickAlpha, 0.0f, 0.0f);
	SetRelativeLocationAndRotation(Offset, Rotation);
	if (bBow)
	{
		PoseBow(Draw);
	}
}
