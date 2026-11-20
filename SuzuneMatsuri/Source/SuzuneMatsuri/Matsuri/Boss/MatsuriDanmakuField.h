// Suzune's Night Festival - the danmaku field: moves, draws and collides every boss bullet at once.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriBossArena.h"
#include "MatsuriDanmaku.h"
#include "MatsuriDanmakuField.generated.h"

class UInstancedStaticMeshComponent;
class AMatsuriBreakableCover;
class APawn;

/** Running totals since the fight began; the boss's reward reads the change over one skill. */
struct FMatsuriDanmakuStats
{
	int32 Fired = 0;
	int32 Touches = 0;		// bullets that reached the player's hit sphere (also during invulnerability)
	int32 Hits = 0;			// touches that cost the player (not invulnerable)
	int32 Grazes = 0;
	int32 CoverHits = 0;	// bullets that died on a fence or solid cover
};

/**
 *  Playtest set the budget: the game had just been brought back from 17 fps. Bullets are plain structs
 *  advanced here, drawn with one instanced mesh per style and skill (one draw each, not one actor each),
 *  and collided analytically: the player's small hit sphere, the courtyard's boxes, the breakable fences.
 */
UCLASS()
class SUZUNEMATSURI_API AMatsuriDanmakuField : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriDanmakuField();

	/** The courtyard whose boxes stop bullets; fences are looked up in the world. */
	void SetArena(const FMatsuriBossArena& InArena);

	/** Adds bullets (beyond MaxBullets the newest are dropped - the budget is a promise). */
	void Spawn(TArray<FMatsuriBullet>& Bullets);

	/** Removes every bullet (a phase change, her defeat), each with a little puff. */
	void ClearAll();

	virtual void Tick(float DeltaSeconds) override;

	int32 Num() const { return Bullets.Num(); }
	const TArray<FMatsuriBullet>& GetBullets() const { return Bullets; }
	const FMatsuriDanmakuStats& GetStats() const { return Stats; }

	/** Draws captured bullets without changing live bullets, statistics or collision state. */
	void ShowReplayBullets(const TArray<FMatsuriBullet>& RecordedBullets);

	/** The player teleported: test the landing, not a path through the skipped space. */
	void ResetPlayerSweep() { bHasPreviousPlayer = false; }

	/** Where the player's hit sphere is, for skills and tests. False when there is no player. */
	bool GetPlayerHitSphere(FVector& OutCenter) const;

	static constexpr int32 MaxBullets = 1400;
	static constexpr float PlayerHitRadius = 22.0f;		// cm: much smaller than the body, as in Touhou
	static constexpr float PlayerHitHeight = 30.0f;		// above the capsule centre (the chest)
	static constexpr float GrazeRadius = 90.0f;
	static constexpr float GrazeUltCharge = 0.004f;

protected:
	virtual void BeginPlay() override;

private:
	bool FindCoverImpact(const FMatsuriBullet& Bullet, float& OutFraction, AMatsuriBreakableCover*& OutFence) const;
	void Draw();
	void Draw(const TArray<FMatsuriBullet>& VisibleBullets);
	UInstancedStaticMeshComponent* MeshFor(EMatsuriBulletStyle Style, uint8 Skill);

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	/** One per style x skill (each its own glow colour). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Meshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AMatsuriBreakableCover>> Fences;

	TArray<FMatsuriBullet> Bullets;
	TArray<FBox> SolidBoxes;
	FMatsuriBossArena Arena;
	FMatsuriDanmakuStats Stats;
	TArray<TArray<FTransform>> Scratch;	// per mesh, reused every frame
	double LastGrazeSound = 0.0;
	TWeakObjectPtr<APawn> PreviousPlayerPawn;
	FVector PreviousPlayerCenter = FVector::ZeroVector;
	bool bHasPreviousPlayer = false;
};
