// Suzune's Night Festival - pick a heroine before the run.

#include "MatsuriHeroSelectWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MatsuriArt.h"
#include "MatsuriFlow.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriHeroSelect"

namespace
{
	constexpr float CardWidth = 340.0f;
	constexpr float CardHeight = 500.0f;
	constexpr float PortraitHeight = 230.0f;	// added on top when the portraits are imported	// fits the longest card (Suzune) with wrapped descriptions
	const FMargin CardGap(14.0f, 0.0f);
	const TCHAR* const SlotKeys[] = { TEXT("Shift"), TEXT("E"), TEXT("Q") };

	UTextBlock* Left(UTextBlock* Text)
	{
		Text->SetJustification(ETextJustify::Left);
		Text->SetAutoWrapText(true);
		return Text;
	}
}

void UMatsuriHeroSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Title", "誰と祭りへ行く？"), 46, MatsuriUI::Gold));
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Subtitle", "CHOOSE YOUR HEROINE  —  click a card or press 1 / 2 / 3"), 18, MatsuriUI::Paper, TEXT("Regular")))
		->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 26.0f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 i = 0; i < Matsuri::Heroes::Count; ++i)
	{
		Row->AddChildToHorizontalBox(BuildCard(static_cast<EMatsuriHero>(i)))->SetPadding(CardGap);
	}
	Column->AddChildToVerticalBox(Row)->SetHorizontalAlignment(HAlign_Center);
	MatsuriUI::InstallModalCard(*WidgetTree, Column);
}

UWidget* UMatsuriHeroSelectWidget::BuildCard(EMatsuriHero HeroId)
{
	const FMatsuriHeroDef& Hero = Matsuri::Heroes::Get(HeroId);
	UVerticalBox* Face = WidgetTree->ConstructWidget<UVerticalBox>();
	UWidget* Portrait = MatsuriUI::MakePortrait(*WidgetTree, Matsuri::Art::Portrait(HeroId), Matsuri::Art::PortraitBustUV,
		FVector2D(CardWidth - 40.0f, PortraitHeight));
	if (Portrait)
	{
		Face->AddChildToVerticalBox(Portrait)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}
	const FText Number = FText::AsNumber(static_cast<int32>(HeroId) + 1);
	Face->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, FText::Format(LOCTEXT("Head", "{0}   {1}"), Number, Hero.Title), 18, MatsuriUI::Gold));
	Face->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, Hero.Name, 34, MatsuriUI::Paper));
	Face->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, Hero.Role, 15, Hero.Color, TEXT("Regular")))->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 12.0f));

	const FText Weapon = FText::Format(LOCTEXT("Weapon", "LMB   {0}"), Hero.Weapon.Name);
	Face->AddChildToVerticalBox(Left(MatsuriUI::MakeText(*WidgetTree, Weapon, 16, MatsuriUI::Paper)));
	const FText Passive = FText::Format(LOCTEXT("Passive", "Passive   {0}"), Hero.Passive);
	Face->AddChildToVerticalBox(Left(MatsuriUI::MakeText(*WidgetTree, Passive, 15, MatsuriUI::Paper, TEXT("Regular"))))->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 10.0f));

	for (int32 i = 0; i < UE_ARRAY_COUNT(SlotKeys); ++i)
	{
		const FMatsuriSkillDef& Skill = Hero.Skills[i];
		const FText Line = FText::Format(LOCTEXT("Skill", "{0}   {1}"), FText::FromString(SlotKeys[i]), Skill.Name);
		Face->AddChildToVerticalBox(Left(MatsuriUI::MakeText(*WidgetTree, Line, 17, MatsuriUI::Gold)));
		Face->AddChildToVerticalBox(Left(MatsuriUI::MakeText(*WidgetTree, Skill.Description, 14, MatsuriUI::Paper, TEXT("Regular"))))
			->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	UButton* Card = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle Style = Card->GetStyle();
	Style.Normal.TintColor = FSlateColor(MatsuriUI::NightIndigo.CopyWithNewOpacity(1.0f) + Hero.Color * 0.18f);
	Style.Hovered.TintColor = FSlateColor(Hero.Color * 0.55f);
	Style.Pressed.TintColor = FSlateColor(Hero.Color);
	Card->SetStyle(Style);
	if (UButtonSlot* FaceSlot = Cast<UButtonSlot>(Card->AddChild(Face)))
	{
		FaceSlot->SetPadding(FMargin(18.0f, 14.0f));
		FaceSlot->SetHorizontalAlignment(HAlign_Fill);
		FaceSlot->SetVerticalAlignment(VAlign_Top);
	}
	switch (HeroId)
	{
	case EMatsuriHero::Sakura:	Card->OnClicked.AddDynamic(this, &UMatsuriHeroSelectWidget::HandleSakura); break;
	case EMatsuriHero::Suzune:	Card->OnClicked.AddDynamic(this, &UMatsuriHeroSelectWidget::HandleSuzune); break;
	default:					Card->OnClicked.AddDynamic(this, &UMatsuriHeroSelectWidget::HandleMio); break;
	}

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(CardWidth);
	Box->SetHeightOverride(CardHeight + (Portrait ? PortraitHeight + 8.0f : 0.0f));
	Box->SetContent(Card);
	return Box;
}

FReply UMatsuriHeroSelectWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const int32 Index = Key == EKeys::One ? 0 : Key == EKeys::Two ? 1 : Key == EKeys::Three ? 2 : INDEX_NONE;
	if (Index != INDEX_NONE)
	{
		ChooseHero(static_cast<EMatsuriHero>(Index));
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMatsuriHeroSelectWidget::ChooseHero(EMatsuriHero Hero)
{
	if (bChosen)
	{
		return;
	}
	bChosen = true;
	Matsuri::Flow::StartNewRun(this, Hero);
}

#undef LOCTEXT_NAMESPACE
