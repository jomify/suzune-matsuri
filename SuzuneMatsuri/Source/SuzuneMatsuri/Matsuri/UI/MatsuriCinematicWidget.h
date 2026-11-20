// Suzune's Night Festival - optional movie-backed cinematic chapters.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriCinematicWidget.generated.h"

class APlayerController;
class UButton;
class UImage;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;
class UMediaSoundComponent;

enum class EMatsuriCinematic : uint8
{
	FirstFloor,
	SecondFloor,
	ThirdFloor,
	Defeat,
	Victory,
	UltimateSakura,
	UltimateSuzune,
	UltimateMio
};

/** A skippable full-screen movie. Missing movies fall back to the caller's text/normal flow. */
UCLASS()
class SUZUNEMATSURI_API UMatsuriCinematicWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static bool TryPlay(APlayerController& PC, EMatsuriCinematic Cinematic, FSimpleDelegate OnFinished = FSimpleDelegate(), float MaxPlaybackSeconds = 0.0f);
	static bool TryPlayPath(APlayerController& PC, const FString& AbsolutePath, FSimpleDelegate OnFinished = FSimpleDelegate(), float MaxPlaybackSeconds = 0.0f);
	static bool IsUltimateEnabled();
	static FString RelativeMoviePath(EMatsuriCinematic Cinematic);
	static FString AbsoluteMoviePath(EMatsuriCinematic Cinematic);

	void Finish();
	bool IsFinished() const { return bFinished; }
	bool HasMediaOpened() const { return bMediaOpened; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	void Begin(const FString& AbsolutePath, FSimpleDelegate InOnFinished, float InMaxPlaybackSeconds);
	void RestoreAndNotify();
	UFUNCTION()
	void HandleMediaOpened(FString OpenedUrl);
	UFUNCTION()
	void HandleMediaOpenFailed(FString FailedUrl);
	UFUNCTION()
	void HandleMediaEnd();

	UFUNCTION()
	void HandleSkipClicked() { Finish(); }

	UPROPERTY()
	TObjectPtr<UImage> MovieImage;
	UPROPERTY()
	TObjectPtr<UButton> SkipButton;
	UPROPERTY()
	TObjectPtr<UMediaPlayer> MediaPlayer;
	UPROPERTY()
	TObjectPtr<UMediaTexture> MediaTexture;
	UPROPERTY()
	TObjectPtr<UFileMediaSource> MediaSource;
	UPROPERTY()
	TObjectPtr<UMediaSoundComponent> MediaSound;

	FSimpleDelegate OnFinished;
	TWeakObjectPtr<APlayerController> OwningController;
	FInputModeGameOnly PreviousGameInput;
	bool bWasPaused = false;
	bool bWasCursorVisible = false;
	bool bFinished = false;
	bool bTearingDown = false;
	bool bMediaOpened = false;
	float OpenAgeSeconds = 0.0f;
	FTSTicker::FDelegateHandle OpenTimeoutTicker;
	FTSTicker::FDelegateHandle PlaybackTicker;
	float MaxPlaybackSeconds = 0.0f;
	float PlaybackAgeSeconds = 0.0f;
};
