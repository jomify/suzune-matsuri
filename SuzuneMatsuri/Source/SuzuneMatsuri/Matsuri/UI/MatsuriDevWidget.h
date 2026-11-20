// Suzune's Night Festival - 開発者モード: the title's developer panel (jump to a floor, demo switches).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriHeroes.h"
#include "MatsuriDevWidget.generated.h"

class UButton;
class UTextBlock;

/**
 *  Opened from the title. Pick a heroine and jump straight into 1F, 2F, 3F or the boss; flip god mode,
 *  yokai, the in-game developer keys (F5-F8), the quality cap, and the boss's brain, decision overlay,
 *  six-skill showcase and health. Every switch is a console variable, so it can also be typed.
 */
UCLASS()
class SUZUNEMATSURI_API UMatsuriDevWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ChooseHero(EMatsuriHero Hero);
	/** Starts a run with the chosen heroine on FloorIndex (Matsuri::Floors::BossFloor = the boss). */
	void JumpTo(int32 FloorIndex);
	EMatsuriHero GetHero() const { return Hero; }

protected:
	virtual void NativeOnInitialized() override;

private:
	void Refresh();
	UButton* AddButton(class UPanelWidget& Row, const FText& Label, FName Handler);

	UFUNCTION() void HandleSakura() { ChooseHero(EMatsuriHero::Sakura); }
	UFUNCTION() void HandleSuzune() { ChooseHero(EMatsuriHero::Suzune); }
	UFUNCTION() void HandleMio() { ChooseHero(EMatsuriHero::Mio); }
	UFUNCTION() void HandleFloor1() { JumpTo(0); }
	UFUNCTION() void HandleFloor2() { JumpTo(1); }
	UFUNCTION() void HandleFloor3() { JumpTo(2); }
	UFUNCTION() void HandleBoss();
	UFUNCTION() void HandleGod();
	UFUNCTION() void HandleThreats();
	UFUNCTION() void HandleDevKeys();
	UFUNCTION() void HandleQuality();
	UFUNCTION() void HandleShrinePack();
	UFUNCTION() void HandleBrain();
	UFUNCTION() void HandleBossDebug();
	UFUNCTION() void HandleShowcase();
	UFUNCTION() void HandleBossHealth();
	UFUNCTION() void HandleBack();

	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> HeroButtons;
	UPROPERTY(Transient) TObjectPtr<UButton> GodButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ThreatsButton;
	UPROPERTY(Transient) TObjectPtr<UButton> DevKeysButton;
	UPROPERTY(Transient) TObjectPtr<UButton> QualityButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ShrinePackButton;
	UPROPERTY(Transient) TObjectPtr<UButton> BrainButton;
	UPROPERTY(Transient) TObjectPtr<UButton> BossDebugButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ShowcaseButton;
	UPROPERTY(Transient) TObjectPtr<UButton> BossHealthButton;

	EMatsuriHero Hero = EMatsuriHero::Sakura;
};
