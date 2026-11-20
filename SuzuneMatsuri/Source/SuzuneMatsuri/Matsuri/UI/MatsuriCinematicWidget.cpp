// Suzune's Night Festival - optional movie-backed cinematic chapters.

#include "MatsuriCinematicWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "FileMediaSource.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MediaSoundComponent.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriCinematic"

namespace
{
	constexpr int32 ZOrder = 1100;
	TAutoConsoleVariable<float> CVarOpenTimeout(
		TEXT("matsuri.CinematicOpenTimeout"), 10.0f,
		TEXT("Seconds to wait for a cinematic media backend to open before falling back."));
	TAutoConsoleVariable<bool> CVarUltimateEnabled(
		TEXT("matsuri.UltimateCinematic"), true,
		TEXT("Play per-hero anime ultimate videos (0 = use gameplay ultimate only)."));
	const TCHAR* const Paths[] = {
		TEXT("Movies/matsuri_intro_1.mp4"), TEXT("Movies/matsuri_intro_2.mp4"),
		TEXT("Movies/matsuri_intro_3.mp4"), TEXT("Movies/matsuri_defeat.mp4"),
		TEXT("Movies/matsuri_victory.mp4"), TEXT("Movies/matsuri_ultimate_sakura.mp4"),
		TEXT("Movies/matsuri_ultimate_suzune.mp4"), TEXT("Movies/matsuri_ultimate_mio.mp4")
	};
}

bool UMatsuriCinematicWidget::IsUltimateEnabled()
{
	return CVarUltimateEnabled.GetValueOnGameThread();
}

FString UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic Cinematic)
{
	const int32 Index = FMath::Clamp(static_cast<int32>(Cinematic), 0, UE_ARRAY_COUNT(Paths) - 1);
	return Paths[Index];
}

FString UMatsuriCinematicWidget::AbsoluteMoviePath(EMatsuriCinematic Cinematic)
{
	return FPaths::ProjectContentDir() / RelativeMoviePath(Cinematic);
}

bool UMatsuriCinematicWidget::TryPlay(APlayerController& PC, EMatsuriCinematic Cinematic, FSimpleDelegate InOnFinished, float MaxPlaybackSeconds)
{
	return TryPlayPath(PC, AbsoluteMoviePath(Cinematic), MoveTemp(InOnFinished), MaxPlaybackSeconds);
}

bool UMatsuriCinematicWidget::TryPlayPath(APlayerController& PC, const FString& Path, FSimpleDelegate InOnFinished, float MaxPlaybackSeconds)
{
	if (Path.IsEmpty() || !IFileManager::Get().FileExists(*Path))
	{
		return false;
	}
	UMatsuriCinematicWidget* Widget = CreateWidget<UMatsuriCinematicWidget>(&PC, StaticClass());
	if (!Widget)
	{
		return false;
	}
	Widget->OwningController = &PC;
	Widget->bWasPaused = PC.IsPaused();
	Widget->bWasCursorVisible = PC.ShouldShowMouseCursor();
	Widget->AddToViewport(ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC.SetInputMode(InputMode);
	PC.SetShowMouseCursor(true);
	PC.SetPause(true);
	Widget->Begin(Path, MoveTemp(InOnFinished), MaxPlaybackSeconds);
	return true;
}

void UMatsuriCinematicWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	MovieImage = WidgetTree->ConstructWidget<UImage>();
	UOverlaySlot* ImageSlot = Root->AddChildToOverlay(MovieImage);
	ImageSlot->SetHorizontalAlignment(HAlign_Fill);
	ImageSlot->SetVerticalAlignment(VAlign_Fill);
	SkipButton = MatsuriUI::MakeSmallButton(*WidgetTree, LOCTEXT("Skip", "跳过  Skip"));
	SkipButton->OnClicked.AddDynamic(this, &UMatsuriCinematicWidget::HandleSkipClicked);
	UOverlaySlot* ButtonSlot = Root->AddChildToOverlay(SkipButton);
	ButtonSlot->SetHorizontalAlignment(HAlign_Right);
	ButtonSlot->SetVerticalAlignment(VAlign_Bottom);
	ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 40.0f, 35.0f));
}

void UMatsuriCinematicWidget::Begin(const FString& AbsolutePath, FSimpleDelegate InOnFinished, float InMaxPlaybackSeconds)
{
	OnFinished = MoveTemp(InOnFinished);
	MaxPlaybackSeconds = FMath::Max(0.0f, InMaxPlaybackSeconds);
	PlaybackAgeSeconds = 0.0f;
	MediaPlayer = NewObject<UMediaPlayer>(this, TEXT("CinematicMediaPlayer"));
	MediaTexture = NewObject<UMediaTexture>(this, TEXT("CinematicMediaTexture"));
	MediaSource = NewObject<UFileMediaSource>(this, TEXT("CinematicMediaSource"));
	UObject* SoundOuter = OwningController.IsValid() ? static_cast<UObject*>(OwningController.Get()) : static_cast<UObject*>(this);
	MediaSound = NewObject<UMediaSoundComponent>(SoundOuter);
	MediaTexture->SetMediaPlayer(MediaPlayer);
	MediaTexture->UpdateResource();
	MediaSource->SetFilePath(AbsolutePath);
	MediaSound->SetMediaPlayer(MediaPlayer);
	MediaSound->bIsUISound = true;
	MediaSound->PrimaryComponentTick.bTickEvenWhenPaused = true;
	MediaSound->RegisterComponent();
	MediaSound->AddClockSink();
	MovieImage->SetBrushResourceObject(MediaTexture);
	MediaPlayer->OnMediaOpened.AddDynamic(this, &UMatsuriCinematicWidget::HandleMediaOpened);
	MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &UMatsuriCinematicWidget::HandleMediaOpenFailed);
	MediaPlayer->OnEndReached.AddDynamic(this, &UMatsuriCinematicWidget::HandleMediaEnd);
	if (!MediaPlayer->OpenSource(MediaSource))
	{
		Finish();
		return;
	}
	OpenTimeoutTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
	{
		OpenAgeSeconds += DeltaTime;
		if (bFinished || bMediaOpened || OpenAgeSeconds >= CVarOpenTimeout.GetValueOnGameThread())
		{
			if (!bMediaOpened && !bFinished)
			{
				Finish();
			}
			return false;
		}
		return true;
	}));
}

void UMatsuriCinematicWidget::HandleMediaOpened(FString)
{
	bMediaOpened = true;
	if (OpenTimeoutTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(OpenTimeoutTicker);
		OpenTimeoutTicker.Reset();
	}
	if (!bFinished && MediaPlayer)
	{
		if (!MediaPlayer->Play())
		{
			Finish();
			return;
		}
		if (MaxPlaybackSeconds > 0.0f)
		{
			PlaybackTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds)
			{
				PlaybackAgeSeconds += FMath::Max(0.0f, DeltaSeconds);
				if (bFinished || PlaybackAgeSeconds >= MaxPlaybackSeconds)
				{
					if (!bFinished) { Finish(); }
					return false;
				}
				return true;
			}));
		}
	}
}

void UMatsuriCinematicWidget::HandleMediaOpenFailed(FString)
{
	Finish();
}

void UMatsuriCinematicWidget::HandleMediaEnd()
{
	Finish();
}

void UMatsuriCinematicWidget::NativeDestruct()
{
	bTearingDown = GetWorld() && GetWorld()->bIsTearingDown;
	if (OpenTimeoutTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(OpenTimeoutTicker);
		OpenTimeoutTicker.Reset();
	}
	if (PlaybackTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PlaybackTicker);
		PlaybackTicker.Reset();
	}
	if (MediaSound)
	{
		MediaSound->RemoveClockSink();
		MediaSound->DestroyComponent();
		MediaSound = nullptr;
	}
	if (!bFinished)
	{
		bFinished = true;
		if (MediaPlayer)
		{
			MediaPlayer->Close();
		}
		if (!bTearingDown)
		{
			RestoreAndNotify();
		}
		else
		{
			OnFinished.Unbind();
		}
	}
	Super::NativeDestruct();
}

void UMatsuriCinematicWidget::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	if (OpenTimeoutTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(OpenTimeoutTicker);
		OpenTimeoutTicker.Reset();
	}
	if (PlaybackTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PlaybackTicker);
		PlaybackTicker.Reset();
	}
	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}
	if (MediaSound)
	{
		MediaSound->RemoveClockSink();
		MediaSound->DestroyComponent();
		MediaSound = nullptr;
	}
	RemoveFromParent();
	RestoreAndNotify();
}

void UMatsuriCinematicWidget::RestoreAndNotify()
{
	APlayerController* PC = OwningController.Get();
	if (PC)
	{
		PC->SetPause(bWasPaused);
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(bWasCursorVisible);
	}
	FSimpleDelegate Callback = MoveTemp(OnFinished);
	if (Callback.IsBound())
	{
		Callback.Execute();
	}
}

FReply UMatsuriCinematicWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Enter || Event.GetKey() == EKeys::SpaceBar)
	{
		Finish();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
