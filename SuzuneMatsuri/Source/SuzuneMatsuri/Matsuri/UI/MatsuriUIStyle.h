// Suzune's Night Festival - palette and widget factory helpers for code-built UMG.

#pragma once

#include "CoreMinimal.h"

class UWidgetTree;
class UTextBlock;
class UButton;
class UBorder;
class UWidget;

namespace MatsuriUI
{
	// Palette: vermilion = safe / exit, gold = reward, night indigo = backdrop.
	inline const FLinearColor Vermilion(0.80f, 0.12f, 0.06f);
	inline const FLinearColor VermilionHover(0.95f, 0.25f, 0.10f);
	inline const FLinearColor Gold(1.00f, 0.72f, 0.20f);
	inline const FLinearColor Paper(0.98f, 0.94f, 0.86f);
	inline const FLinearColor NightIndigo(0.015f, 0.015f, 0.06f, 0.92f);
	inline const FLinearColor InkShadow(0.0f, 0.0f, 0.0f, 0.55f);
	inline const FLinearColor Danger(1.00f, 0.20f, 0.15f);		// hurt, low time, empty ammo
	inline const FLinearColor AmmoBlue(0.35f, 0.80f, 1.00f);	// matches the ammo talisman

	UTextBlock* MakeText(UWidgetTree& Tree, const FText& Text, int32 Size, const FLinearColor& Color, FName Typeface = TEXT("Bold"));

	/** A vermilion button with a centered paper-colored label. */
	UButton* MakeButton(UWidgetTree& Tree, const FText& Label);

	/** A compact button for dense panels (the developer panel). */
	UButton* MakeSmallButton(UWidgetTree& Tree, const FText& Label);

	/** Changes a MakeButton / MakeSmallButton label (toggles show their state); Highlight turns it gold. */
	void SetButtonLabel(UButton* Button, const FText& Label, bool bHighlight = false);

	/** A colored panel around Content. */
	UBorder* MakePanel(UWidgetTree& Tree, UWidget* Content, const FLinearColor& Color, const FMargin& Padding);

	/** A portrait image cropped to Crop (UV), or nullptr when the art is not imported. */
	class UWidget* MakePortrait(UWidgetTree& Tree, class UTexture2D* Texture, const FBox2f& Crop, const FVector2D& Size);

	/** Makes Content a centred night-indigo card and installs it as the tree's root. */
	void InstallModalCard(UWidgetTree& Tree, UWidget* Content);
}
