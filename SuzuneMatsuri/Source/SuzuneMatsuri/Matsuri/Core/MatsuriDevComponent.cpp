// Suzune's Night Festival - developer keys during play (F5-F8) and the line that lists them.

#include "MatsuriDevComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "MatsuriDev.h"
#include "MatsuriUIStyle.h"

namespace
{
	constexpr int32 MappingPriority = 3;
	constexpr int32 LineZOrder = 2;
	const TCHAR* const KeysText = TEXT("DEV  F5 clear floor / defeat her   F6 ammo + ultimate   F7 boss: next bar   F8 god");
}

void UMatsuriDevLineWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	Label = MatsuriUI::MakeText(*WidgetTree, FText::FromString(KeysText), 14, MatsuriUI::Gold, TEXT("Regular"));
	UOverlaySlot* LabelSlot = Root->AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Bottom);
	LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
}

void UMatsuriDevLineWidget::SetText(const FString& Text)
{
	if (Label)
	{
		Label->SetText(FText::FromString(Text));
	}
}

UMatsuriDevComponent::UMatsuriDevComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMatsuriDevComponent::BeginPlay()
{
	Super::BeginPlay();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Input || !Subsystem)
	{
		return;
	}
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_MatsuriDev"));
	const TPair<FKey, void (UMatsuriDevComponent::*)()> Keys[] = {
		{ EKeys::F5, &UMatsuriDevComponent::HandleClear },
		{ EKeys::F6, &UMatsuriDevComponent::HandleRestock },
		{ EKeys::F7, &UMatsuriDevComponent::HandleBossBar },
		{ EKeys::F8, &UMatsuriDevComponent::HandleGod },
	};
	for (const TPair<FKey, void (UMatsuriDevComponent::*)()>& Key : Keys)
	{
		UInputAction* Action = NewObject<UInputAction>(this);
		Mapping->MapKey(Action, Key.Key);
		Actions.Add(Action);
		BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Started, this, Key.Value).GetHandle());
	}
	Subsystem->AddMappingContext(Mapping, MappingPriority);

	// The key list at the bottom of the screen, under the skill bar's hint line.
	Line = CreateWidget<UMatsuriDevLineWidget>(PC, UMatsuriDevLineWidget::StaticClass());
	Line->AddToViewport(LineZOrder);
}

void UMatsuriDevComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC)
	{
		if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PC->InputComponent))
		{
			for (const uint32 Handle : BindingHandles)
			{
				Input->RemoveBindingByHandle(Handle);
			}
		}
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(Mapping);
		}
	}
	if (Line)
	{
		Line->RemoveFromParent();
	}
	Super::EndPlay(EndPlayReason);
}

void UMatsuriDevComponent::Show(const FString& Done)
{
	LastAction = Done;
	if (Line)
	{
		Line->SetText(FString::Printf(TEXT("%s      [%s]"), KeysText, *Done));
	}
}

void UMatsuriDevComponent::HandleClear()
{
	Matsuri::Dev::ClearFloor(GetWorld());
	Show(TEXT("F5: floor cleared"));
}

void UMatsuriDevComponent::HandleRestock()
{
	Matsuri::Dev::Restock(GetWorld());
	Show(TEXT("F6: ammo + ultimate"));
}

void UMatsuriDevComponent::HandleBossBar()
{
	Show(Matsuri::Dev::NextBossPhase(GetWorld()) ? TEXT("F7: her next bar") : TEXT("F7: no boss on this floor"));
}

void UMatsuriDevComponent::HandleGod()
{
	Show(Matsuri::Dev::ToggleGod() ? TEXT("F8: god ON") : TEXT("F8: god OFF"));
}
