// Suzune's Night Festival - the loading screen between scenes and the title card on arrival.

#include "MatsuriLoadingWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriLoading"

namespace
{
	constexpr int32 LoadingZOrder = 1000;			// above the HUD and every menu
	constexpr float SpinDegreesPerSecond = 220.0f;
	constexpr float MaxStepSeconds = 0.05f;		// the first frame after a level load is a long hitch: never skip the card
	const FLinearColor Night(0.01f, 0.01f, 0.035f, 1.0f);

	/** Short tips shown while a floor opens (they rotate). */
	const TCHAR* const Tips[] = {
		TEXT("提灯は見つめている間はほとんど動かない — Chochin barely move while you watch them. Turn your back and they rush in."),
		TEXT("唐傘は角の向こうで待ち伏せする — Kasa wait round corners and pounce. Check corners before you turn them."),
		TEXT("河童は投げたら物陰に隠れる — Kappa duck behind cover after a throw. Catch them as they step out."),
		TEXT("近接 (V) はきゅうりを打ち返せる — Melee (V) knocks cucumbers out of the air and costs no ammo."),
		TEXT("さくら: 引き絞るほど矢は速く、重い — Sakura: the fuller the draw, the faster and harder the arrow. You walk slower while drawing."),
		TEXT("弾薬の御札は時間で戻る — Ammo talismans come back after a while. Melee keeps you fighting meanwhile."),
	};
}

UMatsuriLoadingWidget* UMatsuriLoadingWidget::ShowLeaving(APlayerController& PC, const FText& Destination)
{
	UMatsuriLoadingWidget* Widget = CreateWidget<UMatsuriLoadingWidget>(&PC, UMatsuriLoadingWidget::StaticClass());
	Widget->Configure(Destination, LOCTEXT("Loading", "読み込み中…  Loading"), FText::GetEmpty(), false);
	Widget->AddToViewport(LoadingZOrder);
	return Widget;
}

UMatsuriLoadingWidget* UMatsuriLoadingWidget::ShowArriving(APlayerController& PC, const FText& Title, bool bWithTip)
{
	static int32 NextTip = 0;
	UMatsuriLoadingWidget* Widget = CreateWidget<UMatsuriLoadingWidget>(&PC, UMatsuriLoadingWidget::StaticClass());
	const FText Tip = bWithTip ? FText::FromString(Tips[NextTip++ % UE_ARRAY_COUNT(Tips)]) : FText::GetEmpty();
	Widget->Configure(Title, FText::GetEmpty(), Tip, true);
	Widget->AddToViewport(LoadingZOrder);
	return Widget;
}

FText UMatsuriLoadingWidget::GetHeadline() const
{
	return HeadlineText ? HeadlineText->GetText() : FText::GetEmpty();
}

void UMatsuriLoadingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);	// never takes a click or a key

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	Cover = WidgetTree->ConstructWidget<UBorder>();
	Cover->SetBrushColor(Night);
	UOverlaySlot* CoverSlot = Root->AddChildToOverlay(Cover);
	CoverSlot->SetHorizontalAlignment(HAlign_Fill);
	CoverSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Middle = WidgetTree->ConstructWidget<UVerticalBox>();
	HeadlineText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 46, MatsuriUI::Gold);
	Middle->AddChildToVerticalBox(HeadlineText)->SetHorizontalAlignment(HAlign_Center);
	CaptionText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 18, MatsuriUI::Paper, TEXT("Regular"));
	Middle->AddChildToVerticalBox(CaptionText)->SetHorizontalAlignment(HAlign_Center);
	UOverlaySlot* MiddleSlot = Root->AddChildToOverlay(Middle);
	MiddleSlot->SetHorizontalAlignment(HAlign_Center);
	MiddleSlot->SetVerticalAlignment(VAlign_Center);

	TipText = MatsuriUI::MakeText(*WidgetTree, FText::GetEmpty(), 17, MatsuriUI::Paper, TEXT("Regular"));
	TipText->SetAutoWrapText(true);
	UOverlaySlot* TipSlot = Root->AddChildToOverlay(TipText);
	TipSlot->SetHorizontalAlignment(HAlign_Center);
	TipSlot->SetVerticalAlignment(VAlign_Bottom);
	TipSlot->SetPadding(FMargin(160.0f, 0.0f, 160.0f, 70.0f));

	Spinner = MatsuriUI::MakeText(*WidgetTree, FText::FromString(TEXT("❖")), 34, MatsuriUI::Gold);
	Spinner->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UOverlaySlot* SpinnerSlot = Root->AddChildToOverlay(Spinner);
	SpinnerSlot->SetHorizontalAlignment(HAlign_Right);
	SpinnerSlot->SetVerticalAlignment(VAlign_Bottom);
	SpinnerSlot->SetPadding(FMargin(0.0f, 0.0f, 60.0f, 50.0f));
}

void UMatsuriLoadingWidget::Configure(const FText& Headline, const FText& Caption, const FText& Tip, bool bInArriving)
{
	bArriving = bInArriving;
	Age = 0.0f;
	HeadlineText->SetText(Headline);
	CaptionText->SetText(Caption);
	TipText->SetText(Tip);
	Spinner->SetVisibility(bArriving ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Coverage = bArriving ? 1.0f : 0.0f;
	SetRenderOpacity(Coverage);
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
		{
			return Advance(DeltaTime);
		}));
	}
}

void UMatsuriLoadingWidget::NativeDestruct()
{
	FTSTicker::RemoveTicker(TickHandle);
	TickHandle.Reset();
	Super::NativeDestruct();
}

bool UMatsuriLoadingWidget::Advance(float DeltaTime)
{
	if (!IsInViewport())
	{
		TickHandle.Reset();	// its level went away (or it already finished)
		return false;
	}
	Age += FMath::Min(DeltaTime, MaxStepSeconds);
	if (bArriving)
	{
		// Hold the title card, then fade into play and go.
		Coverage = Age < HoldSeconds ? 1.0f : 1.0f - FMath::Clamp((Age - HoldSeconds) / RevealSeconds, 0.0f, 1.0f);
		if (Coverage <= 0.0f)
		{
			RemoveFromParent();
			TickHandle.Reset();
			return false;
		}
	}
	else
	{
		Coverage = FMath::Clamp(Age / LeaveSeconds, 0.0f, 1.0f);
		Spinner->SetRenderTransformAngle(FMath::Fmod(Age * SpinDegreesPerSecond, 360.0f));
	}
	SetRenderOpacity(FMath::SmoothStep(0.0f, 1.0f, Coverage));
	return true;
}

#undef LOCTEXT_NAMESPACE
