// Dedicated close-up for the two toon prototypes. Run with a renderer.
#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#include "Animation/SkeletalMeshActor.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "MatsuriArt.h"
#include "MatsuriMainMenuWidget.h"
#include "MatsuriToon.h"
#include "MatsuriTypes.h"

using namespace MatsuriTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriToonPreviewTest, "MatsuriArt.ToonPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMatsuriToonPreviewTest::RunTest(const FString&)
{
    TArray<TPair<FString, FStepRunner::FStep>> Steps;
    for (int32 Which = 0; Which < 3; ++Which)
    {
        Steps.Add({ Which == 0 ? TEXT("Tamamo toon") : Which == 1 ? TEXT("Suzune toon") : TEXT("Tamamo original"), [this, Which](UWorld* World, int32 Frame)
        {
            APlayerController* PC = World->GetFirstPlayerController();
            if (!PC || !FindWidget<UMatsuriMainMenuWidget>(World)) return EStep::Wait;
            if (Frame == 0)
            {
                if (UMatsuriMainMenuWidget* Menu = FindWidget<UMatsuriMainMenuWidget>(World)) Menu->SetVisibility(ESlateVisibility::Collapsed);
                USkeletalMesh* Mesh = Which != 1 ? Matsuri::Art::CharacterMesh(TEXT("SK_Boss_Tamamo"), TEXT("Idle")) : Matsuri::Art::HeroMesh(EMatsuriHero::Suzune, TEXT("Idle"));
                if (!TestNotNull(TEXT("toon preview mesh"), Mesh)) return EStep::Done;
                const FVector Spot(4000.0f, Which * 1000.0f, 200.0f);
                ASkeletalMeshActor* Model = World->SpawnActor<ASkeletalMeshActor>(Spot, FRotator(0, 180, 0));
                Model->GetSkeletalMeshComponent()->SetSkeletalMesh(Mesh);
                Model->GetSkeletalMeshComponent()->SetWorldScale3D(FVector(1.6f));
                if (Which != 2) TestTrue(TEXT("toon material applied"), Matsuri::Toon::Apply(Model->GetSkeletalMeshComponent(), Which == 0));
                UAnimSequence* Idle = Which != 1 ? Matsuri::Art::CharacterAnim(TEXT("SK_Boss_Tamamo"), TEXT("Idle")) : Matsuri::Art::HeroAnim(EMatsuriHero::Suzune, TEXT("Idle"));
                if (Idle)
                {
                    Model->GetSkeletalMeshComponent()->PlayAnimation(Idle, true);
                    Model->GetSkeletalMeshComponent()->SetPosition(Idle->GetPlayLength() * 0.5f, false);
                    Model->GetSkeletalMeshComponent()->bPauseAnims = true;
                }
                ACameraActor* Camera = World->SpawnActor<ACameraActor>(Spot + FVector(-205, 0, 90), FRotator(0, 0, 0));
                PC->SetViewTarget(Camera);
            }
            return DoneIf(SettleAndShoot(Frame, Which == 0 ? TEXT("toon_tamamo_close") : Which == 1 ? TEXT("toon_suzune_close") : TEXT("toon_tamamo_original"), 120));
        } });
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Maps::Title));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
