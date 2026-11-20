// Suzune's Night Festival - skippable code-built story chapter card.

#include "MatsuriStoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriStoryWidget"

UMatsuriStoryWidget* UMatsuriStoryWidget::Show(APlayerController& PC, const FMatsuriStoryChapter& InChapter, FSimpleDelegate InOnFinished)
{
	UMatsuriStoryWidget* Widget = CreateWidget<UMatsuriStoryWidget>(&PC, StaticClass());
	Widget->OwningController = &PC;
	Widget->bWasPaused = PC.IsPaused();
	Widget->bWasCursorVisible = PC.ShouldShowMouseCursor();
	Widget->Configure(InChapter, MoveTemp(InOnFinished));
	Widget->AddToViewport(100);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC.SetInputMode(InputMode);
	PC.SetShowMouseCursor(true);
	PC.SetPause(true);
	return Widget;
}

void UMatsuriStoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	TitleText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 44, MatsuriUI::Gold);
	Column->AddChildToVerticalBox(TitleText);
	ObjectiveText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 18, MatsuriUI::Paper, TEXT("Regular"));
	ObjectiveText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(ObjectiveText)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 26.0f));
	SpeakerText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 24, MatsuriUI::Gold);
	SpeakerText->SetJustification(ETextJustify::Left);
	Column->AddChildToVerticalBox(SpeakerText);
	LineText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 28, MatsuriUI::Paper, TEXT("Regular"));
	LineText->SetJustification(ETextJustify::Left);
	LineText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(LineText)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 24.0f));

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	NextButton = MatsuriUI::MakeButton(*WidgetTree, LOCTEXT("Next", "下一句  Next"));
	NextButton->OnClicked.AddDynamic(this, &UMatsuriStoryWidget::HandleNextClicked);
	Buttons->AddChildToHorizontalBox(NextButton)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	UButton* Skip = MatsuriUI::MakeSmallButton(*WidgetTree, LOCTEXT("Skip", "跳过  Skip"));
	Skip->OnClicked.AddDynamic(this, &UMatsuriStoryWidget::HandleSkipClicked);
	Buttons->AddChildToHorizontalBox(Skip);
	Column->AddChildToVerticalBox(Buttons)->SetHorizontalAlignment(HAlign_Center);
	PageText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 16, MatsuriUI::Paper, TEXT("Regular"));
	Column->AddChildToVerticalBox(PageText)->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
	MatsuriUI::InstallModalCard(*WidgetTree, Column);
}

void UMatsuriStoryWidget::Configure(const FMatsuriStoryChapter& InChapter, FSimpleDelegate InOnFinished)
{
	Chapter = InChapter;
	OnFinished = MoveTemp(InOnFinished);
	LineIndex = 0;
	bFinished = false;
	Refresh();
}

void UMatsuriStoryWidget::Refresh()
{
	if (!Chapter.Lines.IsValidIndex(LineIndex))
	{
		return;
	}
	TitleText->SetText(Chapter.Title);
	ObjectiveText->SetText(Chapter.Objective);
	SpeakerText->SetText(Chapter.Lines[LineIndex].Speaker);
	LineText->SetText(Chapter.Lines[LineIndex].Text);
	PageText->SetText(FText::Format(LOCTEXT("Page", "{0} / {1}"), LineIndex + 1, Chapter.Lines.Num()));
	MatsuriUI::SetButtonLabel(NextButton,
		LineIndex + 1 < Chapter.Lines.Num() ? LOCTEXT("Next", "下一句  Next") : LOCTEXT("Close", "开始行动  Continue"),
		LineIndex + 1 >= Chapter.Lines.Num());
}

void UMatsuriStoryWidget::NextLine()
{
	if (bFinished)
	{
		return;
	}
	if (LineIndex + 1 < Chapter.Lines.Num())
	{
		++LineIndex;
		Refresh();
		return;
	}
	Finish();
}

void UMatsuriStoryWidget::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	APlayerController* PC = OwningController.Get();
	RemoveFromParent();
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

FReply UMatsuriStoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		NextLine();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		Finish();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
