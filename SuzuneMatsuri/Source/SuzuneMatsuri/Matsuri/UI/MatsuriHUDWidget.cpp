// Suzune's Night Festival - in-game HUD.

#include "MatsuriHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriArt.h"
#include "MatsuriFloors.h"
#include "MatsuriSkillComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "MatsuriRunSubsystem.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriHUD"

namespace
{
	constexpr float BarWidth = 280.0f;
	constexpr float BarHeight = 12.0f;
	constexpr float LowTimeSeconds = 15.0f;
	constexpr float HurtFlashPeak = 0.45f;
	constexpr float HurtFlashFadePerSecond = 1.2f;
	constexpr float AmmoFlashDuration = 0.35f;
	const FMargin ScreenEdge(36.0f, 28.0f);
	const FMargin PanelPadding(18.0f, 12.0f);
	const TCHAR* const SkillKeys[] = { TEXT("Shift"), TEXT("E"), TEXT("Q") };
	constexpr float SkillSlotWidth = 170.0f;

	UOverlaySlot* Place(UOverlay& Root, UWidget* Child, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin())
	{
		UOverlaySlot* Slot = Root.AddChildToOverlay(Child);
		Slot->SetHorizontalAlignment(H);
		Slot->SetVerticalAlignment(V);
		Slot->SetPadding(Padding);
		return Slot;
	}
}

void UMatsuriHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	HurtFlash = WidgetTree->ConstructWidget<UBorder>();
	HurtFlash->SetBrushColor(MatsuriUI::Danger.CopyWithNewOpacity(0.0f));
	HurtFlash->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(*Root, HurtFlash, HAlign_Fill, VAlign_Fill);

	GuardGlow = WidgetTree->ConstructWidget<UBorder>();
	GuardGlow->SetBrushColor(MatsuriUI::Gold.CopyWithNewOpacity(0.0f));
	GuardGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(*Root, GuardGlow, HAlign_Fill, VAlign_Fill);

	Place(*Root, BuildScorePanel(), HAlign_Left, VAlign_Top, ScreenEdge);
	Place(*Root, BuildClockPanel(), HAlign_Center, VAlign_Top, ScreenEdge);
	Place(*Root, BuildStatusPanel(), HAlign_Right, VAlign_Top, ScreenEdge);
	Crosshair = MatsuriUI::MakeText(*WidgetTree, FText::FromString(TEXT("+")), 22, MatsuriUI::Paper);
	Place(*Root, Crosshair, HAlign_Center, VAlign_Center);

	Place(*Root, BuildSkillBar(), HAlign_Center, VAlign_Bottom, FMargin(0.0f, 0.0f, 0.0f, 54.0f));
	const FText Hint = LOCTEXT("Hint", "LMB  shoot (hold to charge: Sakura)      V  strike      Koban +1  ·  Ginka +2  ·  Magatama +5");
	Place(*Root, MatsuriUI::MakeText(*WidgetTree, Hint, 14, MatsuriUI::Paper, TEXT("Regular")), HAlign_Center, VAlign_Bottom, FMargin(0.0f, 0.0f, 0.0f, 18.0f));
}

UWidget* UMatsuriHUDWidget::BuildScorePanel()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("ScoreLabel", "SCORE"), 14, MatsuriUI::Gold))->SetHorizontalAlignment(HAlign_Left);

	ScoreText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 40, MatsuriUI::Paper);
	Column->AddChildToVerticalBox(ScoreText)->SetHorizontalAlignment(HAlign_Left);

	TargetBar = WidgetTree->ConstructWidget<UProgressBar>();
	TargetBar->SetFillColorAndOpacity(MatsuriUI::Gold);
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
	BarBox->SetWidthOverride(BarWidth);
	BarBox->SetHeightOverride(BarHeight);
	BarBox->SetContent(TargetBar);
	Column->AddChildToVerticalBox(BarBox)->SetPadding(FMargin(0.0f, 6.0f));

	RunScoreText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 14, MatsuriUI::Paper, TEXT("Regular"));
	Column->AddChildToVerticalBox(RunScoreText)->SetHorizontalAlignment(HAlign_Left);
	return MatsuriUI::MakePanel(*WidgetTree, Column, MatsuriUI::NightIndigo.CopyWithNewOpacity(0.6f), PanelPadding);
}

UWidget* UMatsuriHUDWidget::BuildClockPanel()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	ClockText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 36, MatsuriUI::Paper);
	Column->AddChildToVerticalBox(ClockText);
	return MatsuriUI::MakePanel(*WidgetTree, Column, MatsuriUI::NightIndigo.CopyWithNewOpacity(0.6f), FMargin(28.0f, 6.0f));
}

UWidget* UMatsuriHUDWidget::BuildStatusPanel()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	HeroText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 18, MatsuriUI::Gold);
	Column->AddChildToVerticalBox(HeroText)->SetHorizontalAlignment(HAlign_Right);
	HealthText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 26, MatsuriUI::Paper);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
	HealthBar->SetFillColorAndOpacity(MatsuriUI::Danger);
	USizeBox* HealthBox = WidgetTree->ConstructWidget<USizeBox>();
	HealthBox->SetWidthOverride(220.0f);
	HealthBox->SetHeightOverride(12.0f);
	HealthBox->SetContent(HealthBar);
	AmmoText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 26, MatsuriUI::AmmoBlue);
	Column->AddChildToVerticalBox(HealthText)->SetHorizontalAlignment(HAlign_Right);
	Column->AddChildToVerticalBox(HealthBox)->SetPadding(FMargin(0.0f, 3.0f));
	Column->AddChildToVerticalBox(AmmoText)->SetHorizontalAlignment(HAlign_Right);

	// Portrait (filled in once the heroine is known) to the left of the status column.
	PortraitSlot = WidgetTree->ConstructWidget<UHorizontalBox>();
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->AddChildToHorizontalBox(PortraitSlot)->SetVerticalAlignment(VAlign_Center);
	Row->AddChildToHorizontalBox(Column)->SetVerticalAlignment(VAlign_Center);
	return MatsuriUI::MakePanel(*WidgetTree, Row, MatsuriUI::NightIndigo.CopyWithNewOpacity(0.6f), PanelPadding);
}

UWidget* UMatsuriHUDWidget::BuildSkillBar()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 i = 0; i < UE_ARRAY_COUNT(SkillKeys); ++i)
	{
		UVerticalBox* SkillColumn = WidgetTree->ConstructWidget<UVerticalBox>();
		SkillColumn->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, FText::FromString(SkillKeys[i]), 13, MatsuriUI::Gold, TEXT("Regular")));
		UTextBlock* Name = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 22, MatsuriUI::Paper);
		UTextBlock* State = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 14, MatsuriUI::Paper, TEXT("Regular"));
		SkillColumn->AddChildToVerticalBox(Name);
		SkillColumn->AddChildToVerticalBox(State);
		SkillNames.Add(Name);
		SkillStates.Add(State);

		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(SkillSlotWidth);
		Box->SetContent(MatsuriUI::MakePanel(*WidgetTree, SkillColumn, MatsuriUI::NightIndigo.CopyWithNewOpacity(0.65f), FMargin(10.0f, 6.0f)));
		Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(6.0f, 0.0f));
	}
	return Row;
}

void UMatsuriHUDWidget::TickSkillBar()
{
	const UMatsuriSkillComponent* Skills = BoundSkills.Get();
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	if (!Skills || !Run)
	{
		return;
	}
	for (int32 i = 0; i < SkillStates.Num(); ++i)
	{
		const EMatsuriSkillSlot SkillSlot = static_cast<EMatsuriSkillSlot>(i);
		const bool bUltimate = SkillSlot == EMatsuriSkillSlot::Q;
		const float Cooldown = Skills->GetCooldownRemaining(SkillSlot);
		const float Charge = Run->GetRunState().UltCharge;
		const bool bReady = bUltimate ? Charge >= 1.0f : Cooldown <= 0.0f;
		const FText State = bReady ? LOCTEXT("Ready", "READY")
			: bUltimate ? FText::Format(LOCTEXT("Charging", "{0}%"), FMath::FloorToInt32(Charge * 100.0f))
			: FText::FromString(FString::Printf(TEXT("%.1f s"), Cooldown));
		SkillStates[i]->SetText(State);
		SkillStates[i]->SetColorAndOpacity(FSlateColor(bReady ? MatsuriUI::Gold : MatsuriUI::Paper));
		SkillNames[i]->SetColorAndOpacity(FSlateColor(bReady ? MatsuriUI::Paper : MatsuriUI::Paper.CopyWithNewOpacity(0.45f)));
	}
}

void UMatsuriHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this))
	{
		Run->OnRunChanged.AddDynamic(this, &UMatsuriHUDWidget::HandleRunChanged);
		Run->OnPlayerHurt.AddDynamic(this, &UMatsuriHUDWidget::HandlePlayerHurt);
		HandleRunChanged(Run->GetRunState());
	}
	const APawn* Pawn = GetOwningPlayerPawn();
	if (UMatsuriCombatComponent* Combat = Pawn ? Pawn->FindComponentByClass<UMatsuriCombatComponent>() : nullptr)
	{
		BoundCombat = Combat;
		DryFireHandle = Combat->OnDryFire.AddUObject(this, &UMatsuriHUDWidget::HandleDryFire);
		HitHandle = Combat->OnHitConfirmed.AddUObject(this, &UMatsuriHUDWidget::HandleHitConfirmed);
		KillHandle = Combat->OnKillConfirmed.AddUObject(this, &UMatsuriHUDWidget::HandleKillConfirmed);
	}
	BoundSkills = Pawn ? Pawn->FindComponentByClass<UMatsuriSkillComponent>() : nullptr;
}

void UMatsuriHUDWidget::NativeDestruct()
{
	if (UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this))
	{
		Run->OnRunChanged.RemoveDynamic(this, &UMatsuriHUDWidget::HandleRunChanged);
		Run->OnPlayerHurt.RemoveDynamic(this, &UMatsuriHUDWidget::HandlePlayerHurt);
	}
	if (UMatsuriCombatComponent* Combat = BoundCombat.Get())
	{
		Combat->OnDryFire.Remove(DryFireHandle);
		Combat->OnHitConfirmed.Remove(HitHandle);
		Combat->OnKillConfirmed.Remove(KillHandle);
	}
	Super::NativeDestruct();
}

void UMatsuriHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (const AMatsuriFloorGameMode* GameMode = GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>())
	{
		const int32 Seconds = FMath::CeilToInt32(GameMode->GetRemainingSeconds());
		ClockText->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60)));
		ClockText->SetColorAndOpacity(FSlateColor(Seconds <= LowTimeSeconds ? MatsuriUI::Danger : MatsuriUI::Paper));

		const float Pulse = 0.10f + 0.06f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.0f);
		GuardGlow->SetBrushColor(MatsuriUI::Gold.CopyWithNewOpacity(GameMode->IsPlayerInvulnerable() ? Pulse : 0.0f));
	}

	HurtFlashAlpha = FMath::Max(0.0f, HurtFlashAlpha - HurtFlashFadePerSecond * InDeltaTime);
	HurtFlash->SetBrushColor(MatsuriUI::Danger.CopyWithNewOpacity(HurtFlashAlpha));

	TickSkillBar();
	const UMatsuriCombatComponent* Combat = BoundCombat.Get();
	const bool bCharging = Combat && Combat->IsCharging();
	HitMarkerSeconds = FMath::Max(0.0f, HitMarkerSeconds - InDeltaTime);
	KillMarkerSeconds = FMath::Max(0.0f, KillMarkerSeconds - InDeltaTime);
	const bool bKillMarker = KillMarkerSeconds > 0.0f && !bCharging;
	const bool bHitMarker = HitMarkerSeconds > 0.0f && !bCharging;
	// Drawing: the brackets close in as the bow draws (a steadier, harder shot); a full draw locks gold.
	const float Draw = bCharging ? Combat->GetChargeAlpha() : 0.0f;
	const bool bFullDraw = bCharging && Draw >= 1.0f;
	FString Mark = TEXT("+");
	if (bCharging)
	{
		const FString Gap = FString::ChrN(FMath::RoundToInt32((1.0f - Draw) * DrawBracketSpaces), TEXT(' '));
		Mark = bFullDraw ? TEXT("(◉)") : FString::Printf(TEXT("(%s+%s)"), *Gap, *Gap);
	}
	else if (bKillMarker || bHitMarker)
	{
		Mark = bKillMarker ? TEXT("✖") : TEXT("✕");
	}
	Crosshair->SetText(FText::FromString(Mark));
	Crosshair->SetColorAndOpacity(FSlateColor(bKillMarker ? MatsuriUI::Danger : bFullDraw || bHitMarker ? MatsuriUI::Gold : MatsuriUI::Paper));
	const float Pulse = bFullDraw ? 1.15f + 0.1f * FMath::Sin(GetWorld()->GetTimeSeconds() * 14.0f) : 1.0f;
	Crosshair->SetRenderScale(FVector2D(bKillMarker ? KillMarkerScale : bHitMarker ? HitMarkerScale : Pulse));

	AmmoFlashSeconds = FMath::Max(0.0f, AmmoFlashSeconds - InDeltaTime);
	const bool bAmmoAlert = bAmmoEmpty || AmmoFlashSeconds > 0.0f;
	AmmoText->SetColorAndOpacity(FSlateColor(bAmmoAlert ? MatsuriUI::Danger : MatsuriUI::AmmoBlue));
}

void UMatsuriHUDWidget::HandleRunChanged(const FMatsuriRunState& State)
{
	const FMatsuriScoreState& Score = State.Score;
	ScoreText->SetText(Matsuri::Floors::IsBossFloor(State.FloorIndex)
		? FText::AsNumber(Score.FloorScore)	// cleared by defeating her, not by a target
		: FText::Format(LOCTEXT("ScoreValue", "{0} / {1}"), Score.FloorScore, Score.FloorTarget));
	RunScoreText->SetText(FText::Format(LOCTEXT("RunValue", "{0}    Run total  {1}"), Matsuri::Floors::ThemeFor(State.FloorIndex).Title, Score.RunScore));
	TargetBar->SetPercent(Score.GetFloorProgress());
	const float MaxHealth = State.GetMaxHealth();
	HealthText->SetText(FText::Format(LOCTEXT("Health", "HP  {0} / {1}"), FMath::RoundToInt32(State.Health), FMath::RoundToInt32(MaxHealth)));
	HealthBar->SetPercent(MaxHealth > 0.0f ? FMath::Clamp(State.Health / MaxHealth, 0.0f, 1.0f) : 0.0f);
	HealthBar->SetFillColorAndOpacity(State.Health <= MaxHealth * 0.25f ? MatsuriUI::Danger : MatsuriUI::Gold);
	const FMatsuriHeroDef& Hero = State.GetHero();
	HeroText->SetText(FText::Format(LOCTEXT("Hero", "{0}  {1}"), Hero.Title, Hero.Name));
	if (ShownPortrait != State.Hero)
	{
		ShownPortrait = State.Hero;
		PortraitSlot->ClearChildren();
		if (UWidget* Face = MatsuriUI::MakePortrait(*WidgetTree, Matsuri::Art::Portrait(State.Hero), Matsuri::Art::PortraitFaceUV, FVector2D(84.0f, 84.0f)))
		{
			PortraitSlot->AddChildToHorizontalBox(Face)->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		}
	}
	HeroText->SetColorAndOpacity(FSlateColor(Hero.Color));
	for (int32 i = 0; i < SkillNames.Num(); ++i)
	{
		SkillNames[i]->SetText(Hero.Skills[i].Name);
	}
	AmmoText->SetText(FText::Format(LOCTEXT("Ammo", "弾  {0} / {1}"), State.Ammo, State.GetAmmoCap()));
	bAmmoEmpty = State.Ammo == 0;
}

void UMatsuriHUDWidget::HandlePlayerHurt(EMatsuriHurtResult Result)
{
	HurtFlashAlpha = HurtFlashPeak;
}

void UMatsuriHUDWidget::HandleKillConfirmed()
{
	KillMarkerSeconds = KillMarkerDuration;
}

void UMatsuriHUDWidget::HandleHitConfirmed()
{
	HitMarkerSeconds = HitMarkerDuration;
}

void UMatsuriHUDWidget::HandleDryFire()
{
	AmmoFlashSeconds = AmmoFlashDuration;
}

#undef LOCTEXT_NAMESPACE
