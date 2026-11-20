// Suzune's Night Festival - the loading screen between scenes and the title card on arrival.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "MatsuriLoadingWidget.generated.h"

class APlayerController;
class UBorder;
class UTextBlock;

/**
 *  Playtest: scene changes were abrupt cuts. Leaving a scene now fades into a loading screen (where you
 *  are going, a turning lantern mark, 読み込み中…) before the level loads; arriving opens on a title card
 *  (the floor's name and a tip) that holds a moment and fades into play. It never blocks input, never
 *  holds the clock, and runs while the game is paused (menus pause it). It is driven by the core ticker,
 *  not NativeTick, so it also fades with no renderer (headless tests), where Slate never ticks widgets.
 */
UCLASS()
class SUZUNEMATSURI_API UMatsuriLoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fades in over the scene being left. */
	static UMatsuriLoadingWidget* ShowLeaving(APlayerController& PC, const FText& Destination);

	/** Starts covering the new scene and fades away; removes itself when done. */
	static UMatsuriLoadingWidget* ShowArriving(APlayerController& PC, const FText& Title, bool bWithTip);

	bool IsArriving() const { return bArriving; }

	/** How much of the scene is covered right now (0 = clear, 1 = fully covered). */
	float GetCoverage() const { return Coverage; }

	FText GetHeadline() const;

	static constexpr float LeaveSeconds = 0.35f;	// fade-in before the level loads
	static constexpr float HoldSeconds = 0.6f;		// the title card holds before fading
	static constexpr float RevealSeconds = 0.8f;	// and then fades into play

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void Configure(const FText& Headline, const FText& Caption, const FText& Tip, bool bInArriving);

	/** One step of the fade; false once the card is gone (stops the ticker). */
	bool Advance(float DeltaTime);

	FTSTicker::FDelegateHandle TickHandle;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Cover;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadlineText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CaptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TipText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Spinner;

	bool bArriving = false;
	float Age = 0.0f;
	float Coverage = 0.0f;
};
