// Suzune's Night Festival - code-built visual effects: glowing shapes that grow, move, spin and fade.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriGraybox.h"
#include "MatsuriFx.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/** How one effect piece behaves over its life. */
struct FMatsuriFxSpec
{
	Matsuri::Graybox::EShape Shape = Matsuri::Graybox::EShape::Sphere;
	FLinearColor Color = FLinearColor::White;
	float Glow = 8.0f;						// emissive strength at birth, fades to 0
	FVector StartSize = FVector(20.0f);		// cm
	FVector EndSize = FVector(20.0f);		// cm
	FRotator Rotation = FRotator::ZeroRotator;
	FRotator Spin = FRotator::ZeroRotator;	// degrees per second
	FVector Velocity = FVector::ZeroVector;	// cm/s
	float Gravity = 0.0f;					// cm/s^2 pulling down
	float Lifetime = 0.5f;					// seconds
	float LightIntensity = 0.0f;			// > 0 adds a fading point light
	float LightRadius = 400.0f;
	bool bVisible = true;					// false for invisible pivots that only carry attached pieces
};

/**
 *  One effect piece. Spawned and configured through Matsuri::Fx. At the end of its life it is retired
 *  (hidden, not ticking) and reused by the next effect instead of being destroyed: playtest profiling
 *  found a single melee swing (9 pieces) cost 20-40 ms of the game thread in actor spawning alone.
 *  Effects are pure visuals: no collision, no gameplay.
 */
UCLASS()
class SUZUNEMATSURI_API AMatsuriFx : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriFx();

	/** Call between SpawnActorDeferred and FinishSpawning. */
	void Configure(const FMatsuriFxSpec& InSpec);

	virtual void Tick(float DeltaSeconds) override;

	/** False while it waits in the pool for its next effect. */
	bool IsActive() const { return bActive; }

	/** Brings a retired piece back as a new effect at Location. */
	void Reuse(const FVector& Location, const FMatsuriFxSpec& InSpec);

	/** Hides the piece and stops it ticking until it is reused. */
	void Retire();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Shape;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FMatsuriFxSpec Spec;
	float Age = 0.0f;
	bool bActive = true;
};

namespace Matsuri::Fx
{
	/** The building block: one piece at Location. Attach (optional) makes it follow an actor. */
	SUZUNEMATSURI_API AMatsuriFx* Spawn(UWorld* World, const FVector& Location, const FMatsuriFxSpec& Spec, AActor* Attach = nullptr);

	/** Count glowing motes thrown out from Location (pickups, hits, kills). Direction biases them (zero = all round). */
	SUZUNEMATSURI_API void Burst(UWorld* World, const FVector& Location, const FLinearColor& Color, int32 Count = 12,
		float Speed = 450.0f, float Size = 14.0f, float Lifetime = 0.45f, const FVector& Direction = FVector::ZeroVector);

	/** A flat shockwave ring expanding along the ground. */
	SUZUNEMATSURI_API void Ring(UWorld* World, const FVector& Location, const FLinearColor& Color, float StartRadius, float EndRadius,
		float Lifetime, AActor* Attach = nullptr, float Spin = 0.0f);

	/** A translucent-looking glowing bubble (barrier, aura). */
	SUZUNEMATSURI_API void Dome(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Lifetime, AActor* Attach = nullptr);

	/** A column of light (purge, ultimate hits). */
	SUZUNEMATSURI_API void Pillar(UWorld* World, const FVector& Location, const FLinearColor& Color, float Height, float Lifetime);

	/** A fading silhouette left where someone stood (blink). */
	SUZUNEMATSURI_API void AfterImage(UWorld* World, const FVector& Location, const FLinearColor& Color, float Lifetime);

	/** A thin glowing beam between two points (blink trail). */
	SUZUNEMATSURI_API void Streak(UWorld* World, const FVector& From, const FVector& To, const FLinearColor& Color, float Lifetime);

	/** A grey puff (decoy, yokai vanishing). */
	SUZUNEMATSURI_API void Smoke(UWorld* World, const FVector& Location, float Size = 120.0f);

	/** The engine's public Niagara templates (/Niagara/DefaultAssets/Templates/Systems), used as they come. */
	enum class ETemplate : uint8
	{
		Explosion,		// SimpleExplosion: a core, velocity-aligned sparks and debris (a yokai bursting)
		RadialBurst,	// RadialBurst: sparks with ribbon trails all round (a shot striking a yokai)
		Directional		// DirectionalBurst: sprites thrown one way (sparks off a wall, along its normal)
	};

	/** Spawns a template (matsuri.Niagara 0 turns them off); Direction aims Directional. */
	SUZUNEMATSURI_API void Template(UWorld* World, ETemplate Which, const FVector& Location, const FVector& Direction, float Scale);

	/**
	 *  Loads the templates and, in the editor, finishes compiling them (and their GPU shaders and meshes).
	 *  Playtest: the first spark of each kind compiled it mid-fight - frames of 0.2 to 1.6 s.
	 */
	SUZUNEMATSURI_API void PreloadTemplates();

	/** Fills the effect-piece pool for World now (floor load), so the first swing of a fight does not pay to spawn. */
	SUZUNEMATSURI_API void Prewarm(UWorld* World, int32 Pieces);

	/** How many templates were spawned since the editor started (tests). */
	SUZUNEMATSURI_API int32 TemplateCount();

	/** A bright pop and a short flame tongue where a shot leaves the weapon. */
	SUZUNEMATSURI_API void MuzzleFlash(UWorld* World, const FVector& Location, const FVector& Direction, const FLinearColor& Color);

	/** Where a shot lands: a flash and sparks off the surface; walls also keep a scorch mark a moment. */
	SUZUNEMATSURI_API void Impact(UWorld* World, const FVector& Location, const FVector& Normal, const FLinearColor& Color, bool bOnYokai, bool bScorch = true);

	/** A crescent swing in front of the viewer (melee). */
	SUZUNEMATSURI_API void SwingArc(UWorld* World, const FVector& Eye, const FVector& Facing, const FLinearColor& Color, float Radius, float HalfAngleDegrees);

	/** Spinning stars over a stunned yokai. */
	SUZUNEMATSURI_API void StunHalo(UWorld* World, AActor* Target, float Seconds);
}
