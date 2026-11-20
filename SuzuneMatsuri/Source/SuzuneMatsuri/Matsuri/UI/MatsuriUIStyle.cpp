// Suzune's Night Festival - palette and widget factory helpers for code-built UMG.

#include "MatsuriUIStyle.h"
#include "MatsuriAudio.h"
#include "Sound/SoundBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr int32 ButtonFontSize = 26;
	const FMargin ButtonPadding(48.0f, 12.0f);
	constexpr int32 SmallButtonFontSize = 17;
	const FMargin SmallButtonPadding(16.0f, 6.0f);
}

UTextBlock* MatsuriUI::MakeText(UWidgetTree& Tree, const FText& Text, int32 Size, const FLinearColor& Color, FName Typeface)
{
	UTextBlock* Block = Tree.ConstructWidget<UTextBlock>();
	Block->SetText(Text);
	Block->SetFont(FCoreStyle::GetDefaultFontStyle(Typeface, Size));
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetShadowColorAndOpacity(InkShadow);
	Block->SetShadowOffset(FVector2D(1.5f, 1.5f));
	Block->SetJustification(ETextJustify::Center);
	return Block;
}

UButton* MatsuriUI::MakeButton(UWidgetTree& Tree, const FText& Label)
{
	UButton* Button = Tree.ConstructWidget<UButton>();

	FButtonStyle Style = Button->GetStyle();
	Style.Normal.TintColor = FSlateColor(Vermilion);
	Style.Hovered.TintColor = FSlateColor(VermilionHover);
	Style.Pressed.TintColor = FSlateColor(Gold);
	if (USoundBase* Hover = Matsuri::Audio::Sound(EMatsuriSfx::UiClick))
	{
		Style.HoveredSlateSound.SetResourceObject(Hover);
	}
	if (USoundBase* Press = Matsuri::Audio::Sound(EMatsuriSfx::UiConfirm))
	{
		Style.PressedSlateSound.SetResourceObject(Press);
	}
	Button->SetStyle(Style);

	if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(MakeText(Tree, Label, ButtonFontSize, Paper))))
	{
		Slot->SetPadding(ButtonPadding);
		Slot->SetHorizontalAlignment(HAlign_Center);
	}
	return Button;
}

UButton* MatsuriUI::MakeSmallButton(UWidgetTree& Tree, const FText& Label)
{
	UButton* Button = MakeButton(Tree, Label);
	if (UTextBlock* Text = Cast<UTextBlock>(Button->GetChildAt(0)))
	{
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), SmallButtonFontSize));
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Text->Slot))
		{
			Slot->SetPadding(SmallButtonPadding);
		}
	}
	return Button;
}

void MatsuriUI::SetButtonLabel(UButton* Button, const FText& Label, bool bHighlight)
{
	if (UTextBlock* Text = Button ? Cast<UTextBlock>(Button->GetChildAt(0)) : nullptr)
	{
		Text->SetText(Label);
		Text->SetColorAndOpacity(FSlateColor(bHighlight ? Gold : Paper));
	}
}

UWidget* MatsuriUI::MakePortrait(UWidgetTree& Tree, UTexture2D* Texture, const FBox2f& Crop, const FVector2D& Size)
{
	if (!Texture)
	{
		return nullptr;
	}
	UImage* Image = Tree.ConstructWidget<UImage>();
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = Size;
	Brush.SetUVRegion(Crop);
	Image->SetBrush(Brush);

	USizeBox* Box = Tree.ConstructWidget<USizeBox>();
	Box->SetWidthOverride(Size.X);
	Box->SetHeightOverride(Size.Y);
	Box->SetContent(Image);
	return Box;
}

void MatsuriUI::InstallModalCard(UWidgetTree& Tree, UWidget* Content)
{
	UOverlay* Root = Tree.ConstructWidget<UOverlay>();
	Tree.RootWidget = Root;
	UOverlaySlot* CardSlot = Root->AddChildToOverlay(MakePanel(Tree, Content, NightIndigo, FMargin(72.0f, 48.0f)));
	CardSlot->SetHorizontalAlignment(HAlign_Center);
	CardSlot->SetVerticalAlignment(VAlign_Center);
}

UBorder* MatsuriUI::MakePanel(UWidgetTree& Tree, UWidget* Content, const FLinearColor& Color, const FMargin& Padding)
{
	UBorder* Panel = Tree.ConstructWidget<UBorder>();
	Panel->SetBrushColor(Color);
	Panel->SetPadding(Padding);
	Panel->SetContent(Content);
	return Panel;
}
