// Suzune's Night Festival - the boss HUD: her three health bars, her skill and words, the grapple prompt.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriBossBarWidget.generated.h"

class AMatsuriBoss;
class UMatsuriGrappleComponent;
class UProgressBar;
class UTextBlock;

/**
 *  Top of the screen: 九尾狐 玉藻, three bars (one per phase), the skill she is using and what she says.
 *  Under the crosshair: "F 鉤縄" when a hook ring is in reach. With matsuri.BossDebug 1, a line on how she
 *  decided (model / learner / rules, latency, her reason). Refreshed by the boss game mode every frame
 *  (not NativeTick, which does not run without a renderer).
 */
UCLASS()
class SUZUNEMATSURI_API UMatsuriBossBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Refresh(const AMatsuriBoss* Boss, const UMatsuriGrappleComponent* Grapple);

	/** Text shown for tests. */
	FText GetSkillText() const;
	FText GetDebugText() const;
	float GetBarFill(int32 Bar) const;

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> Bars;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SkillText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TauntText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GrappleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DebugText;
};
