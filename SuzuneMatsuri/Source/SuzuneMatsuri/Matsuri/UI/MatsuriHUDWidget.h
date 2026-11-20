// Suzune's Night Festival - in-game HUD.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriRunState.h"
#include "MatsuriHUDWidget.generated.h"

class UBorder;
class UTextBlock;
class UProgressBar;
class UMatsuriCombatComponent;

/**
 *  Score / target (top-left), floor + countdown (top-centre), HP + ammo
 *  (top-right), crosshair, red flash on hits. Score, HP and ammo update on
 *  the run subsystem's event; only the clock and the flash fade tick.
 */
UCLASS()
class SUZUNEMATSURI_API UMatsuriHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UWidget* BuildScorePanel();
	UWidget* BuildClockPanel();
	UWidget* BuildStatusPanel();
	UWidget* BuildSkillBar();
	void TickSkillBar();

	UFUNCTION()
	void HandleRunChanged(const FMatsuriRunState& State);

	UFUNCTION()
	void HandlePlayerHurt(EMatsuriHurtResult Result);

	void HandleDryFire();
	void HandleHitConfirmed();
	void HandleKillConfirmed();

	UPROPERTY()
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RunScoreText;

	UPROPERTY()
	TObjectPtr<UProgressBar> TargetBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY()
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY()
	TObjectPtr<UBorder> HurtFlash;

	/** Gold screen edge while invulnerable (barrier, respawn grace): the dome itself is not visible from inside. */
	UPROPERTY()
	TObjectPtr<UBorder> GuardGlow;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeroText;

	UPROPERTY()
	TObjectPtr<class UHorizontalBox> PortraitSlot;

	EMatsuriHero ShownPortrait = EMatsuriHero::Count;

	UPROPERTY()
	TObjectPtr<UTextBlock> Crosshair;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SkillNames;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SkillStates;

	TWeakObjectPtr<class UMatsuriSkillComponent> BoundSkills;

	TWeakObjectPtr<UMatsuriCombatComponent> BoundCombat;
	FDelegateHandle DryFireHandle;
	float HurtFlashAlpha = 0.0f;
	float AmmoFlashSeconds = 0.0f;
	float HitMarkerSeconds = 0.0f;
	static constexpr float HitMarkerDuration = 0.18f;
	static constexpr float HitMarkerScale = 1.6f;
	static constexpr float KillMarkerScale = 2.1f;
	static constexpr int32 DrawBracketSpaces = 4;
	FDelegateHandle HitHandle;
	float KillMarkerSeconds = 0.0f;
	static constexpr float KillMarkerDuration = 0.35f;
	FDelegateHandle KillHandle;
	bool bAmmoEmpty = false;
};
