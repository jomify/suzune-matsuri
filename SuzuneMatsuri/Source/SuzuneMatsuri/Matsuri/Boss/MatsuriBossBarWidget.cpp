// Suzune's Night Festival - the boss HUD: her three health bars, her skill and words, the grapple prompt.

#include "MatsuriBossBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "MatsuriBoss.h"
#include "MatsuriBossBrainComponent.h"
#include "MatsuriBossSkills.h"
#include "MatsuriGrappleComponent.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriBossBar"

namespace
{
	TAutoConsoleVariable<bool> CVarBossDebug(
		TEXT("matsuri.BossDebug"), false,
		TEXT("Show how the boss chose her last skill (model / learner / rules, latency, reason)."));

	constexpr int32 PhaseBars = 3;
	constexpr float BarWidth = 220.0f;
	constexpr float BarHeight = 14.0f;
	constexpr float TauntSeconds = 3.5f;
	const FLinearColor BarColors[PhaseBars] = {
		FLinearColor(1.0f, 0.78f, 0.3f), FLinearColor(1.0f, 0.45f, 0.15f), FLinearColor(0.85f, 0.12f, 0.2f) };

	const TCHAR* SourceLabel(EMatsuriBossSource Source)
	{
		switch (Source)
		{
		case EMatsuriBossSource::Model:		return TEXT("model");
		case EMatsuriBossSource::Learner:	return TEXT("learner");
		case EMatsuriBossSource::Rules:		return TEXT("rules");
		}
		return TEXT("");
	}
}

void UMatsuriBossBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	UVerticalBox* Top = WidgetTree->ConstructWidget<UVerticalBox>();
	NameText = MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Name", "九尾狐　玉藻"), 22, MatsuriUI::Gold);
	Top->AddChildToVerticalBox(NameText)->SetHorizontalAlignment(HAlign_Center);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 i = 0; i < PhaseBars; ++i)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(BarWidth);
		Size->SetHeightOverride(BarHeight);
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
		Bar->SetFillColorAndOpacity(BarColors[i]);
		Bar->SetPercent(1.0f);
		Size->AddChild(Bar);
		Row->AddChildToHorizontalBox(Size)->SetPadding(FMargin(3.0f, 0.0f));
		Bars.Add(Bar);
	}
	Top->AddChildToVerticalBox(Row)->SetHorizontalAlignment(HAlign_Center);
	SkillText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 20, MatsuriUI::Paper);
	Top->AddChildToVerticalBox(SkillText)->SetHorizontalAlignment(HAlign_Center);
	TauntText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 18, MatsuriUI::Paper, TEXT("Regular"));
	Top->AddChildToVerticalBox(TauntText)->SetHorizontalAlignment(HAlign_Center);
	UOverlaySlot* TopSlot = Root->AddChildToOverlay(Top);
	TopSlot->SetHorizontalAlignment(HAlign_Center);
	TopSlot->SetVerticalAlignment(VAlign_Top);
	TopSlot->SetPadding(FMargin(0.0f, 125.0f, 0.0f, 0.0f));	// under the clock

	GrappleText = MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Grapple", "F　鉤縄"), 18, MatsuriUI::Gold);
	UOverlaySlot* GrappleSlot = Root->AddChildToOverlay(GrappleText);
	GrappleSlot->SetHorizontalAlignment(HAlign_Center);
	GrappleSlot->SetVerticalAlignment(VAlign_Center);
	GrappleSlot->SetPadding(FMargin(0.0f, 90.0f, 0.0f, 0.0f));	// below the crosshair

	DebugText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 14, MatsuriUI::Paper, TEXT("Regular"));
	DebugText->SetAutoWrapText(true);
	UOverlaySlot* DebugSlot = Root->AddChildToOverlay(DebugText);
	DebugSlot->SetHorizontalAlignment(HAlign_Left);
	DebugSlot->SetVerticalAlignment(VAlign_Bottom);
	DebugSlot->SetPadding(FMargin(30.0f, 0.0f, 600.0f, 140.0f));
}

void UMatsuriBossBarWidget::Refresh(const AMatsuriBoss* Boss, const UMatsuriGrappleComponent* Grapple)
{
	if (!Boss)
	{
		return;
	}
	const float Health = Boss->GetHealthFraction();
	for (int32 i = 0; i < Bars.Num(); ++i)
	{
		// Bar 0 is the first third she loses, bar 2 the last.
		const float Low = 1.0f - (i + 1) / static_cast<float>(PhaseBars);
		Bars[i]->SetPercent(FMath::Clamp((Health - Low) * PhaseBars, 0.0f, 1.0f));
	}
	const FMatsuriSkillProgram* Program = Boss->GetProgram();
	if (Boss->IsDefeated())
	{
		SkillText->SetText(LOCTEXT("Defeated", "討伐！"));
		SkillText->SetColorAndOpacity(FSlateColor(MatsuriUI::Gold));
	}
	else if (Program)
	{
		SkillText->SetText(FText::Format(LOCTEXT("SkillDamage", "{0}  ·  {1} HP / hit"),
			Matsuri::BossSkills::DisplayName(Program->Action.Skill),
			FText::AsNumber(Matsuri::BossSkills::BulletDamageFor(Program->Action))));
		SkillText->SetColorAndOpacity(FSlateColor(Matsuri::BossSkills::Color(Program->Action.Skill)));
	}
	TauntText->SetText(FText::FromString(Boss->GetTaunt()));
	TauntText->SetRenderOpacity(FMath::Clamp(1.0f - (Boss->GetTauntAge() - TauntSeconds), 0.0f, 1.0f));
	GrappleText->SetVisibility(Grapple && Grapple->GetTarget() && !Grapple->IsPulling() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	const UMatsuriBossBrainComponent* Brain = Boss->GetBrain();
	if (CVarBossDebug.GetValueOnGameThread() && Brain)
	{
		const FMatsuriBossAction& Last = Brain->GetLastAction();
		DebugText->SetText(FText::FromString(FString::Printf(TEXT("brain %s | last: %s by %s | model %d asked, %d used, %.1f s%s\n%s"),
			*Brain->GetProvider(), Matsuri::BossSkills::Id(Last.Skill), SourceLabel(Last.Source), Brain->GetModelRequests(),
			Brain->GetDecisions(EMatsuriBossSource::Model), Brain->GetLastModelSeconds(),
			Brain->GetLastModelError().IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Brain->GetLastModelError()), *Last.Why)));
		DebugText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		DebugText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FText UMatsuriBossBarWidget::GetSkillText() const
{
	return SkillText ? SkillText->GetText() : FText::GetEmpty();
}

FText UMatsuriBossBarWidget::GetDebugText() const
{
	return DebugText ? DebugText->GetText() : FText::GetEmpty();
}

float UMatsuriBossBarWidget::GetBarFill(int32 Bar) const
{
	return Bars.IsValidIndex(Bar) ? Bars[Bar]->GetPercent() : 0.0f;
}

#undef LOCTEXT_NAMESPACE
