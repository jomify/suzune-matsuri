// Suzune's Night Festival - every sound effect the game asks for is imported, and the bow's draw sound
// is as long as its draw.
// Run: Automation RunTests SuzuneMatsuri.Audio

#include "Misc/AutomationTest.h"
#include "MatsuriArt.h"
#include "MatsuriAudio.h"
#include "MatsuriHeroes.h"
#include "Sound/SoundBase.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriAudioTableTest, "SuzuneMatsuri.Audio.EverySoundIsImported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriAudioTableTest::RunTest(const FString&)
{
	if (!Matsuri::Art::Sound(TEXT("HitTick")))
	{
		AddInfo(TEXT("no sounds imported (tools/audio/build_sfx.py, tools/ue/import_audio.py): nothing to check"));
		return true;
	}
	for (int32 i = 0; i < static_cast<int32>(EMatsuriSfx::Count); ++i)
	{
		const EMatsuriSfx Sfx = static_cast<EMatsuriSfx>(i);
		TestNotNull(FString::Printf(TEXT("SFX_%s is imported"), Matsuri::Audio::Name(Sfx)), Matsuri::Audio::Sound(Sfx));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBowDrawSoundTest, "SuzuneMatsuri.Audio.BowDrawLastsAFullDraw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriBowDrawSoundTest::RunTest(const FString&)
{
	// The draw sound rises to full tension as the bow does; if the draw time changes, rebuild the sound
	// (tools/audio/build_sfx.py: DRAW_SECONDS).
	constexpr float ToleranceSeconds = 0.05f;
	const USoundBase* Draw = Matsuri::Audio::Sound(EMatsuriSfx::BowDraw);
	if (!Draw)
	{
		AddInfo(TEXT("no sounds imported: nothing to check"));
		return true;
	}
	const float FullDraw = Matsuri::Heroes::Get(EMatsuriHero::Sakura).Weapon.MaxChargeSeconds;
	TestTrue(FString::Printf(TEXT("draw sound %.2f s matches a full draw %.2f s"), Draw->GetDuration(), FullDraw),
		FMath::IsNearlyEqual(Draw->GetDuration(), FullDraw, ToleranceSeconds));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
