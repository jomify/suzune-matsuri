// Suzune's Night Festival - the danmaku field: moves, draws and collides every boss bullet at once.

#include "MatsuriDanmakuField.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MatsuriArenaPieces.h"
#include "MatsuriAudio.h"
#include "MatsuriBoss.h"
#include "MatsuriBossGameMode.h"
#include "MatsuriBossSkills.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriFx.h"
#include "MatsuriGraybox.h"
#include "MatsuriRunSubsystem.h"

namespace
{
	constexpr int32 Styles = static_cast<int32>(EMatsuriBulletStyle::Count);
	constexpr float ShapeSize = 100.0f;		// the engine sphere is 1 m across
	constexpr float BulletGlow = 9.0f;
	constexpr float RiceStretch = 2.4f;		// rice bullets are long along their flight
	constexpr float BoundsMargin = 300.0f;
	constexpr float CeilingHeight = 4000.0f;
	constexpr double GrazeSoundGap = 0.07;	// a stream of grazes ticks, it does not roar
	constexpr int32 PuffEvery = 6;			// one in N cleared bullets leaves a visible puff
	struct FPendingImpact
	{
		int32 BulletIndex = INDEX_NONE;
		FVector Position = FVector::ZeroVector;
		uint8 Skill = 0;
		float Damage = 0.0f;
		bool bCoverAfterPlayer = false;
		bool bOutside = false;
		TWeakObjectPtr<AMatsuriBreakableCover> Fence;
	};
}

AMatsuriDanmakuField::AMatsuriDanmakuField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;	// after the player has moved this frame
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AMatsuriDanmakuField::BeginPlay()
{
	Super::BeginPlay();
	Meshes.SetNum(Styles * Matsuri::BossSkills::Count);
	Scratch.SetNum(Meshes.Num());
	Bullets.Reserve(MaxBullets);
}

void AMatsuriDanmakuField::SetArena(const FMatsuriBossArena& InArena)
{
	Arena = InArena;
	bHasPreviousPlayer = false;
	SolidBoxes.Reset();
	for (const FMatsuriArenaBox& Solid : Arena.Solids)
	{
		if (Solid.Piece != EMatsuriArenaPiece::Fence)
		{
			SolidBoxes.Add(Solid.Bounds());
		}
	}
	Fences.Reset();
	for (TActorIterator<AMatsuriBreakableCover> It(GetWorld()); It; ++It)
	{
		Fences.Add(*It);
	}
}

void AMatsuriDanmakuField::Spawn(TArray<FMatsuriBullet>& NewBullets)
{
	const int32 Room = FMath::Max(0, MaxBullets - Bullets.Num());
	const int32 Taken = FMath::Min(Room, NewBullets.Num());
	Bullets.Append(NewBullets.GetData(), Taken);
	Stats.Fired += Taken;
	NewBullets.Reset();
}

void AMatsuriDanmakuField::ClearAll()
{
	for (int32 i = 0; i < Bullets.Num(); i += PuffEvery)
	{
		Matsuri::Fx::Burst(GetWorld(), Bullets[i].Position, Matsuri::BossSkills::Color(static_cast<EMatsuriBossSkill>(Bullets[i].Skill)), 2, 120.0f, 6.0f, 0.3f);
	}
	Bullets.Reset();
	Draw();
}

bool AMatsuriDanmakuField::GetPlayerHitSphere(FVector& OutCenter) const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return false;
	}
	OutCenter = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, PlayerHitHeight);
	return true;
}

bool AMatsuriDanmakuField::FindCoverImpact(const FMatsuriBullet& Bullet, float& OutFraction, AMatsuriBreakableCover*& OutFence) const
{
	bool bHit = false;
	OutFraction = 1.0f;
	OutFence = nullptr;
	for (const FBox& Box : SolidBoxes)
	{
		float Fraction = 0.0f;
		if (Matsuri::Danmaku::BoxImpact(Bullet, Box, Fraction) && (!bHit || Fraction < OutFraction))
		{
			bHit = true;
			OutFraction = Fraction;
			OutFence = nullptr;
		}
	}
	for (AMatsuriBreakableCover* Fence : Fences)
	{
		float Fraction = 0.0f;
		if (IsValid(Fence) && Fence->IsStanding() && Matsuri::Danmaku::BoxImpact(Bullet, Fence->GetBounds(), Fraction)
			&& (!bHit || Fraction < OutFraction))
		{
			bHit = true;
			OutFraction = Fraction;
			OutFence = Fence;
		}
	}
	return bHit;
}

void AMatsuriDanmakuField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector Player = FVector::ZeroVector;
	const bool bHasPlayer = GetPlayerHitSphere(Player);
	AMatsuriFloorGameMode* Mode = GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const FVector PlayerFrom = bHasPreviousPlayer && PreviousPlayerPawn.Get() == Pawn ? PreviousPlayerCenter : Player;
	PreviousPlayerCenter = Player;
	PreviousPlayerPawn = Pawn;
	bHasPreviousPlayer = bHasPlayer;
	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const float Limit = Arena.HalfExtent + BoundsMargin;
	TArray<FPendingImpact, TInlineAllocator<16>> Impacts;

	for (int32 Index = 0; Index < Bullets.Num(); ++Index)
	{
		FMatsuriBullet& Bullet = Bullets[Index];
		Matsuri::Danmaku::Step(Bullet, DeltaSeconds, Player);
		if (Bullet.bDead || Bullet.Age < Bullet.Delay)
		{
			continue;
		}
		const FVector Local = Bullet.Position - Arena.Center;
		const bool bOutside = FMath::Abs(Local.X) > Limit || FMath::Abs(Local.Y) > Limit || Local.Z < Arena.GroundZ - 50.0f || Local.Z > CeilingHeight;
		float CoverFraction = 1.0f;
		AMatsuriBreakableCover* Fence = nullptr;
		const bool bCover = FindCoverImpact(Bullet, CoverFraction, Fence);
		float PlayerFraction = 1.0f;
		const bool bPlayer = bHasPlayer && Matsuri::Danmaku::PlayerImpact(Bullet, PlayerFrom, Player, PlayerHitRadius, PlayerFraction);
		if (bCover && (!bPlayer || CoverFraction <= PlayerFraction))
		{
			Bullet.bDead = true;
			++Stats.CoverHits;
			if (Fence)
			{
				Fence->AbsorbBullet();
			}
			continue;
		}
		if (bPlayer)
		{
			if (!Bullet.bTouched)
			{
				Bullet.bTouched = true;
				++Stats.Touches;
			}
			FPendingImpact& Impact = Impacts.AddDefaulted_GetRef();
			Impact.BulletIndex = Index;
			Impact.Position = FMath::Lerp(Bullet.PreviousPosition, Bullet.Position, PlayerFraction);
			Impact.Skill = Bullet.Skill;
		Impact.Damage = FMath::IsFinite(Bullet.Damage) && Bullet.Damage > 0.0f ? Bullet.Damage : 0.0f;
			Impact.bCoverAfterPlayer = bCover;
			Impact.bOutside = bOutside;
			Impact.Fence = Fence;
			continue;
		}
		float GrazeFraction = 0.0f;
		if (bHasPlayer && !Bullet.bGrazed && Matsuri::Danmaku::PlayerImpact(Bullet, PlayerFrom, Player, GrazeRadius, GrazeFraction))
		{
			Bullet.bGrazed = true;
			++Stats.Grazes;
			if (Run)
			{
				Run->AddUltCharge(GrazeUltCharge);
			}
			const double Now = GetWorld()->GetTimeSeconds();
			if (Now - LastGrazeSound > GrazeSoundGap)
			{
				LastGrazeSound = Now;
				Matsuri::Audio::Play2D(this, EMatsuriSfx::Graze, 0.35f);
			}
		}
		Bullet.bDead |= bOutside;
	}

	// A fatal hit clears the field and starts the replay. No bullet reference or iterator may
	// survive that callback; leave the fatal frame available for the replay recorder to capture.
	const AMatsuriBossGameMode* BossMode = Cast<AMatsuriBossGameMode>(Mode);
	const AActor* Source = BossMode && BossMode->GetBoss() ? static_cast<const AActor*>(BossMode->GetBoss()) : this;
	for (const FPendingImpact& Impact : Impacts)
	{
		if (!Mode || !Mode->IsFloorActive() || !Bullets.IsValidIndex(Impact.BulletIndex))
		{
			break;
		}
		const bool bAccepted = Mode->HurtPlayer(Source, Impact.Damage,
			Matsuri::BossSkills::DisplayName(static_cast<EMatsuriBossSkill>(Impact.Skill)), Impact.Skill,
			FSimpleDelegate::CreateLambda([this]() { ++Stats.Hits; }));
		if (!Mode->IsFloorActive())
		{
			return;
		}
		if (bAccepted)
		{
			Bullets[Impact.BulletIndex].bDead = true;
			Matsuri::Fx::Burst(GetWorld(), Impact.Position, FLinearColor(1.0f, 0.2f, 0.2f), 10, 380.0f, 10.0f, 0.4f);
		}
		else if (Impact.bCoverAfterPlayer)
		{
			Bullets[Impact.BulletIndex].bDead = true;
			++Stats.CoverHits;
			if (AMatsuriBreakableCover* HitFence = Impact.Fence.Get())
			{
				HitFence->AbsorbBullet();
			}
		}
		else
		{
			Bullets[Impact.BulletIndex].bDead |= Impact.bOutside;
		}
	}
	Bullets.RemoveAllSwap([](const FMatsuriBullet& Bullet) { return Bullet.bDead; }, EAllowShrinking::No);
	Draw();
}

UInstancedStaticMeshComponent* AMatsuriDanmakuField::MeshFor(EMatsuriBulletStyle Style, uint8 Skill)
{
	const int32 Index = static_cast<int32>(Style) * Matsuri::BossSkills::Count + FMath::Min<int32>(Skill, Matsuri::BossSkills::Count - 1);
	if (!Meshes[Index])
	{
		UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(this);
		Mesh->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Sphere));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetupAttachment(Root);
		Mesh->RegisterComponent();
		Matsuri::Graybox::Paint(Mesh, Matsuri::BossSkills::Color(static_cast<EMatsuriBossSkill>(Skill)), true, BulletGlow);
		Meshes[Index] = Mesh;
	}
	return Meshes[Index];
}

void AMatsuriDanmakuField::Draw()
{
	Draw(Bullets);
}

void AMatsuriDanmakuField::ShowReplayBullets(const TArray<FMatsuriBullet>& RecordedBullets)
{
	Draw(RecordedBullets);
}

void AMatsuriDanmakuField::Draw(const TArray<FMatsuriBullet>& VisibleBullets)
{
	for (TArray<FTransform>& Transforms : Scratch)
	{
		Transforms.Reset();
	}
	for (const FMatsuriBullet& Bullet : VisibleBullets)
	{
		if (Bullet.bDead)
		{
			continue;
		}
		const int32 Index = static_cast<int32>(Bullet.Style) * Matsuri::BossSkills::Count + FMath::Min<int32>(Bullet.Skill, Matsuri::BossSkills::Count - 1);
		const float Diameter = Bullet.Radius * 2.0f / ShapeSize;
		if (Bullet.Style == EMatsuriBulletStyle::Rice)
		{
			const FRotator Facing = Bullet.Velocity.IsNearlyZero() ? FRotator::ZeroRotator : Bullet.Velocity.Rotation();
			Scratch[Index].Emplace(Facing, Bullet.Position, FVector(Diameter * RiceStretch, Diameter, Diameter));
		}
		else
		{
			Scratch[Index].Emplace(FRotator::ZeroRotator, Bullet.Position, FVector(Diameter));
		}
	}
	for (int32 Index = 0; Index < Scratch.Num(); ++Index)
	{
		const TArray<FTransform>& Transforms = Scratch[Index];
		UInstancedStaticMeshComponent* Mesh = Meshes[Index];
		if (!Mesh)
		{
			if (Transforms.Num() == 0)
			{
				continue;
			}
			Mesh = MeshFor(static_cast<EMatsuriBulletStyle>(Index / Matsuri::BossSkills::Count), static_cast<uint8>(Index % Matsuri::BossSkills::Count));
		}
		const int32 Have = Mesh->GetInstanceCount();
		if (Have > Transforms.Num())
		{
			TArray<int32> Extra;
			for (int32 i = Have - 1; i >= Transforms.Num(); --i)
			{
				Extra.Add(i);
			}
			Mesh->RemoveInstances(Extra, true);
		}
		else if (Have < Transforms.Num())
		{
			Mesh->AddInstances(TArray<FTransform>(Transforms.GetData() + Have, Transforms.Num() - Have), false, true, false);
		}
		if (Transforms.Num() > 0)
		{
			Mesh->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
		}
	}
}
