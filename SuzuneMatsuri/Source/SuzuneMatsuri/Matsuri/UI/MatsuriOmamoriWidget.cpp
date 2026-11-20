// Suzune's Night Festival - pick one of three omamori between floors.

#include "MatsuriOmamoriWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MatsuriFlow.h"
#include "MatsuriRunSubsystem.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriOmamori"

namespace
{
	constexpr float CardWidth = 250.0f;
	constexpr float CardHeight = 190.0f;
	const FMargin CardGap(12.0f, 0.0f);
}

void UMatsuriOmamoriWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	if (const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this))
	{
		Offer = Run->GetOmamoriOffer();
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Title", "御守をひとつ選べ"), 44, MatsuriUI::Gold));
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Subtitle", "CHOOSE ONE OMAMORI  —  click a card or press 1 / 2 / 3"), 18, MatsuriUI::Paper, TEXT("Regular")))
		->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 28.0f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 i = 0; i < Offer.Num(); ++i)
	{
		Row->AddChildToHorizontalBox(BuildCard(i))->SetPadding(CardGap);
	}
	Column->AddChildToVerticalBox(Row)->SetHorizontalAlignment(HAlign_Center);
	MatsuriUI::InstallModalCard(*WidgetTree, Column);
}

UWidget* UMatsuriOmamoriWidget::BuildCard(int32 Index)
{
	UVerticalBox* Face = WidgetTree->ConstructWidget<UVerticalBox>();
	Face->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, FText::AsNumber(Index + 1), 16, MatsuriUI::Gold, TEXT("Regular")));
	Face->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, Matsuri::Omamori::DisplayName(Offer[Index]), 34, MatsuriUI::Paper))
		->SetPadding(FMargin(0.0f, 6.0f));
	Face->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, Matsuri::Omamori::Effect(Offer[Index]), 17, MatsuriUI::Paper, TEXT("Regular")));

	UButton* Card = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle Style = Card->GetStyle();
	Style.Normal.TintColor = FSlateColor(MatsuriUI::Vermilion);
	Style.Hovered.TintColor = FSlateColor(MatsuriUI::VermilionHover);
	Style.Pressed.TintColor = FSlateColor(MatsuriUI::Gold);
	Card->SetStyle(Style);
	if (UButtonSlot* FaceSlot = Cast<UButtonSlot>(Card->AddChild(Face)))
	{
		FaceSlot->SetHorizontalAlignment(HAlign_Center);
		FaceSlot->SetVerticalAlignment(VAlign_Center);
	}

	switch (Index)
	{
	case 0:		Card->OnClicked.AddDynamic(this, &UMatsuriOmamoriWidget::HandleCard0); break;
	case 1:		Card->OnClicked.AddDynamic(this, &UMatsuriOmamoriWidget::HandleCard1); break;
	default:	Card->OnClicked.AddDynamic(this, &UMatsuriOmamoriWidget::HandleCard2); break;
	}

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(CardWidth);
	Box->SetHeightOverride(CardHeight);
	Box->SetContent(Card);
	return Box;
}

FReply UMatsuriOmamoriWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const int32 Index = Key == EKeys::One ? 0 : Key == EKeys::Two ? 1 : Key == EKeys::Three ? 2 : INDEX_NONE;
	if (Index != INDEX_NONE)
	{
		ChooseCard(Index);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMatsuriOmamoriWidget::ChooseCard(int32 Index)
{
	if (bChosen || !Offer.IsValidIndex(Index))
	{
		return;
	}
	bChosen = true;	// one pick only, even if a click and a key press land on the same frame
	Matsuri::Flow::ContinueWithCharm(this, Offer[Index]);
}

#undef LOCTEXT_NAMESPACE
