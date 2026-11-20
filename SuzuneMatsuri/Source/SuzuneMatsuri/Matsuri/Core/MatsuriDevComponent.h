// Suzune's Night Festival - developer keys during play (F5-F8) and the line that lists them.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriDevComponent.generated.h"

class UInputAction;
class UInputMappingContext;
class UTextBlock;

/** The developer key list at the bottom of the screen. */
UCLASS()
class SUZUNEMATSURI_API UMatsuriDevLineWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetText(const FString& Text);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;
};

/**
 *  Added to the heroine on every floor while matsuri.Dev is on. F5 clears the floor (on the boss floor:
 *  defeats her), F6 fills ammo and the ultimate, F7 starts the boss's next health bar, F8 toggles god mode.
 *  A small line at the bottom of the screen lists the keys and what was last done.
 */
UCLASS(ClassGroup=(Matsuri))
class SUZUNEMATSURI_API UMatsuriDevComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMatsuriDevComponent();

	const FString& GetLastAction() const { return LastAction; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleClear();
	void HandleRestock();
	void HandleBossBar();
	void HandleGod();
	void Show(const FString& Done);

	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> Actions;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TObjectPtr<UMatsuriDevLineWidget> Line;

	FString LastAction;
	TArray<uint32> BindingHandles;
};
