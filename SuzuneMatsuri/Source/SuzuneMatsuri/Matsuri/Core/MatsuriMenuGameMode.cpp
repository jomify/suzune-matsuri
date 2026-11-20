// Suzune's Night Festival - game mode for the title screen.

#include "MatsuriMenuGameMode.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "MatsuriTitleStage.h"
#include "MatsuriLoadingWidget.h"
#include "MatsuriQuality.h"
#include "MatsuriMainMenuWidget.h"
#include "MatsuriMusic.h"
#include "SuzuneMatsuri.h"

AMatsuriMenuGameMode::AMatsuriMenuGameMode()
{
	bStartPlayersAsSpectators = true;
	MenuWidgetClass = UMatsuriMainMenuWidget::StaticClass();
}

void AMatsuriMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
	Matsuri::Quality::ApplyForPlay();
	Matsuri::Music::Play(this, EMatsuriMusic::Title);

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !MenuWidgetClass)
	{
		UE_LOG(LogSuzuneMatsuri, Error, TEXT("Main menu cannot be shown: missing player controller or widget class"));
		return;
	}

	// The heroines at the shrine gate behind the menu (only when their models are imported).
	if (ACameraActor* StageCamera = Matsuri::TitleStage::Build(*GetWorld()))
	{
		PC->SetViewTarget(StageCamera);
	}

	UMatsuriMainMenuWidget* Menu = CreateWidget<UMatsuriMainMenuWidget>(PC, MenuWidgetClass);
	Menu->AddToViewport();

	UMatsuriLoadingWidget::ShowArriving(*PC, FText::GetEmpty(), false);	// fade in from the loading screen

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Menu->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
}
