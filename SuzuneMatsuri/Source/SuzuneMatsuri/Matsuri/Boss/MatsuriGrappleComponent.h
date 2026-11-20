// Suzune's Night Festival - 鉤縄, the grapple: F pulls the player to the hook ring under the crosshair.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MatsuriGrappleComponent.generated.h"

class AMatsuriHookPoint;
class UInputAction;
class UInputMappingContext;
class UStaticMeshComponent;

/**
 *  Added to the heroine on the boss floor. Look at a glowing ring (within Range, near the crosshair,
 *  nothing in between) and press F: a rope shoots out and pulls her there in a straight line; she lands
 *  on the perch below the ring. Jump lets go early and keeps the speed.
 */
UCLASS(ClassGroup=(Matsuri))
class SUZUNEMATSURI_API UMatsuriGrappleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMatsuriGrappleComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Fires at the current target; false when there is none or it is cooling down. Input calls this too. */
	bool TryGrapple();

	/** Lets go (Jump does this mid-pull). */
	void Release(bool bKeepSpeed);

	bool IsPulling() const { return bPulling; }
	AMatsuriHookPoint* GetTarget() const { return Target.Get(); }
	int32 GetUses() const { return Uses; }

	static constexpr float Range = 3500.0f;
	static constexpr float AimConeDegrees = 12.0f;
	static constexpr float PullSpeed = 2600.0f;
	static constexpr float Cooldown = 1.2f;
	static constexpr float MaxPullSeconds = 2.5f;
	static constexpr float ArriveDistance = 140.0f;
	static constexpr float BelowRing = 120.0f;	// she arrives just under the ring, over the perch

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindInput();
	void UnbindInput();
	void HandleGrappleInput() { TryGrapple(); }
	AMatsuriHookPoint* FindTarget() const;
	void UpdateRope();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GrappleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Rope;

	TWeakObjectPtr<AMatsuriHookPoint> Target;
	FVector Destination = FVector::ZeroVector;
	float PullTime = 0.0f;
	double ReadyAt = 0.0;
	int32 Uses = 0;
	bool bPulling = false;
	TArray<uint32> BindingHandles;
};
