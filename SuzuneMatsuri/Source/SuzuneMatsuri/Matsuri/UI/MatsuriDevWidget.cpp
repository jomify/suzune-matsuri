// Suzune's Night Festival - 開発者モード: the title's developer panel (jump to a floor, demo switches).

#include "MatsuriDevWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MatsuriDev.h"
#include "MatsuriFloors.h"
#include "MatsuriFlow.h"
#include "MatsuriMainMenuWidget.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriDev"

namespace
{
	const FMargin RowGap(0.0f, 6.0f);
	const FMargin ButtonGap(4.0f, 0.0f);
	const TArray<FString> Brains = { TEXT("deepseek"), TEXT("ollama"), TEXT("off") };
	const TArray<FString> BossHealths = { TEXT("150"), TEXT("60"), TEXT("15") };

	FText OnOff(const FText& Name, bool bOn)
	{
		return FText::Format(LOCTEXT("Toggle", "{0}: {1}"), Name, bOn ? LOCTEXT("On", "ON") : LOCTEXT("Off", "OFF"));
	}

	FText Section(const TCHAR* Text)
	{
		return FText::FromString(Text);
	}

	bool QualityCapped()
	{
		return Matsuri::Dev::GetString(TEXT("matsuri.MaxQuality")) != TEXT("-1");
	}
}

UButton* UMatsuriDevWidget::AddButton(UPanelWidget& Row, const FText& Label, FName Handler)
{
	UButton* Button = MatsuriUI::MakeSmallButton(*WidgetTree, Label);
	FScriptDelegate Delegate;
	Delegate.BindUFunction(this, Handler);
	Button->OnClicked.Add(Delegate);
	if (UHorizontalBoxSlot* ButtonSlot = Cast<UHorizontalBoxSlot>(Row.AddChild(Button)))
	{
		ButtonSlot->SetPadding(ButtonGap);
	}
	return Button;
}

void UMatsuriDevWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Title", "開発者モード  Developer"), 40, MatsuriUI::Gold))
		->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	auto AddRow = [this, Column](const TCHAR* Heading) -> UHorizontalBox*
	{
		Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, Section(Heading), 18, MatsuriUI::Paper, TEXT("Regular")))
			->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 2.0f));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Column->AddChildToVerticalBox(Row)->SetPadding(RowGap);
		return Row;
	};

	UHorizontalBox* Heroes = AddRow(TEXT("ヒロイン  Heroine"));
	HeroButtons.Add(AddButton(*Heroes, Matsuri::Heroes::Get(EMatsuriHero::Sakura).Name, GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleSakura)));
	HeroButtons.Add(AddButton(*Heroes, Matsuri::Heroes::Get(EMatsuriHero::Suzune).Name, GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleSuzune)));
	HeroButtons.Add(AddButton(*Heroes, Matsuri::Heroes::Get(EMatsuriHero::Mio).Name, GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleMio)));

	UHorizontalBox* Floors = AddRow(TEXT("ステージ選択  Jump to"));
	AddButton(*Floors, Matsuri::Floors::ThemeFor(0).Title, GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleFloor1));
	AddButton(*Floors, Matsuri::Floors::ThemeFor(1).Title, GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleFloor2));
	AddButton(*Floors, Matsuri::Floors::ThemeFor(2).Title, GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleFloor3));
	AddButton(*Floors, Matsuri::Floors::ThemeFor(Matsuri::Floors::BossFloor).Title, GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleBoss));

	UHorizontalBox* Switches = AddRow(TEXT("スイッチ  Switches"));
	GodButton = AddButton(*Switches, FText::GetEmpty(), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleGod));
	ThreatsButton = AddButton(*Switches, FText::GetEmpty(), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleThreats));
	DevKeysButton = AddButton(*Switches, FText::GetEmpty(), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleDevKeys));
	QualityButton = AddButton(*Switches, FText::GetEmpty(), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleQuality));
	ShrinePackButton = AddButton(*Switches, FText::GetEmpty(), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleShrinePack));

	UHorizontalBox* Boss = AddRow(TEXT("玉藻  Boss"));
	BrainButton = AddButton(*Boss, FText::GetEmpty(), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleBrain));
	BossDebugButton = AddButton(*Boss, FText::GetEmpty(), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleBossDebug));
	ShowcaseButton = AddButton(*Boss, FText::GetEmpty(), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleShowcase));
	BossHealthButton = AddButton(*Boss, FText::GetEmpty(), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleBossHealth));

	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree,
		LOCTEXT("Keys", "In play (developer keys on):  F5 clear the floor / defeat her   F6 ammo + ultimate   F7 her next health bar   F8 god mode"),
		15, MatsuriUI::Gold, TEXT("Regular")))->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 8.0f));

	UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddButton(*Bottom, LOCTEXT("Back", "戻る  Back"), GET_FUNCTION_NAME_CHECKED(UMatsuriDevWidget, HandleBack));
	Column->AddChildToVerticalBox(Bottom)->SetHorizontalAlignment(HAlign_Center);

	MatsuriUI::InstallModalCard(*WidgetTree, Column);
	Refresh();
}

void UMatsuriDevWidget::Refresh()
{
	for (int32 i = 0; i < HeroButtons.Num(); ++i)
	{
		const EMatsuriHero ThisHero = static_cast<EMatsuriHero>(i);
		const FText Name = Matsuri::Heroes::Get(ThisHero).Name;
		MatsuriUI::SetButtonLabel(HeroButtons[i], ThisHero == Hero ? FText::Format(LOCTEXT("Chosen", "▶ {0}"), Name) : Name, ThisHero == Hero);
	}
	MatsuriUI::SetButtonLabel(GodButton, OnOff(LOCTEXT("God", "無敵 God"), Matsuri::Dev::GetBool(TEXT("matsuri.God"))));
	MatsuriUI::SetButtonLabel(ThreatsButton, OnOff(LOCTEXT("Threats", "妖怪 Yokai"), Matsuri::Dev::GetBool(TEXT("matsuri.Threats"))));
	MatsuriUI::SetButtonLabel(DevKeysButton, OnOff(LOCTEXT("DevKeys", "F5-F8"), Matsuri::Dev::IsEnabled()));
	MatsuriUI::SetButtonLabel(QualityButton, OnOff(LOCTEXT("Quality", "画質上限 Quality cap"), QualityCapped()));
	MatsuriUI::SetButtonLabel(ShrinePackButton, OnOff(LOCTEXT("ShrinePack", "神社素材 Shrine art"), Matsuri::Dev::GetBool(TEXT("matsuri.ShrinePack"))));
	MatsuriUI::SetButtonLabel(BrainButton, FText::Format(LOCTEXT("Brain", "頭脳 Brain: {0}"), FText::FromString(Matsuri::Dev::GetString(TEXT("matsuri.BossLLM")))));
	MatsuriUI::SetButtonLabel(BossDebugButton, OnOff(LOCTEXT("BossDebug", "判断表示 Overlay"), Matsuri::Dev::GetBool(TEXT("matsuri.BossDebug"))));
	MatsuriUI::SetButtonLabel(ShowcaseButton, OnOff(LOCTEXT("Showcase", "技の展示 Showcase"), Matsuri::Dev::GetBool(TEXT("matsuri.BossShowcase"))));
	MatsuriUI::SetButtonLabel(BossHealthButton, FText::Format(LOCTEXT("BossHealth", "体力 HP: {0}"),
		FText::AsNumber(FMath::RoundToInt32(FCString::Atof(*Matsuri::Dev::GetString(TEXT("matsuri.BossHealth")))))));
}

void UMatsuriDevWidget::ChooseHero(EMatsuriHero InHero)
{
	Hero = InHero;
	Refresh();
}

void UMatsuriDevWidget::JumpTo(int32 FloorIndex)
{
	Matsuri::Flow::StartAtFloor(this, Hero, FloorIndex);
}

void UMatsuriDevWidget::HandleBoss()
{
	JumpTo(Matsuri::Floors::BossFloor);
}

void UMatsuriDevWidget::HandleGod()
{
	Matsuri::Dev::ToggleGod();
	Refresh();
}

void UMatsuriDevWidget::HandleThreats()
{
	Matsuri::Dev::SetBool(TEXT("matsuri.Threats"), !Matsuri::Dev::GetBool(TEXT("matsuri.Threats")));
	Refresh();
}

void UMatsuriDevWidget::HandleDevKeys()
{
	Matsuri::Dev::SetEnabled(!Matsuri::Dev::IsEnabled());
	Refresh();
}

void UMatsuriDevWidget::HandleQuality()
{
	const bool bCap = !QualityCapped();
	Matsuri::Dev::SetString(TEXT("matsuri.MaxQuality"), bCap ? TEXT("3") : TEXT("-1"));
	Matsuri::Dev::SetString(TEXT("matsuri.MaxGIQuality"), bCap ? TEXT("2") : TEXT("-1"));
	Refresh();
}

void UMatsuriDevWidget::HandleShrinePack()
{
	Matsuri::Dev::SetBool(TEXT("matsuri.ShrinePack"), !Matsuri::Dev::GetBool(TEXT("matsuri.ShrinePack")));
	Refresh();
}

void UMatsuriDevWidget::HandleBrain()
{
	Matsuri::Dev::Cycle(TEXT("matsuri.BossLLM"), Brains);
	Refresh();
}

void UMatsuriDevWidget::HandleBossDebug()
{
	Matsuri::Dev::SetBool(TEXT("matsuri.BossDebug"), !Matsuri::Dev::GetBool(TEXT("matsuri.BossDebug")));
	Refresh();
}

void UMatsuriDevWidget::HandleShowcase()
{
	Matsuri::Dev::SetBool(TEXT("matsuri.BossShowcase"), !Matsuri::Dev::GetBool(TEXT("matsuri.BossShowcase")));
	Refresh();
}

void UMatsuriDevWidget::HandleBossHealth()
{
	Matsuri::Dev::Cycle(TEXT("matsuri.BossHealth"), BossHealths);
	Refresh();
}

void UMatsuriDevWidget::HandleBack()
{
	APlayerController* PC = GetOwningPlayer();
	UMatsuriMainMenuWidget* Menu = CreateWidget<UMatsuriMainMenuWidget>(PC, UMatsuriMainMenuWidget::StaticClass());
	Menu->AddToViewport();
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Menu->TakeWidget());
	PC->SetInputMode(InputMode);
	RemoveFromParent();
}

#undef LOCTEXT_NAMESPACE
