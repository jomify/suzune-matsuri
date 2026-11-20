// Suzune's Night Festival - Tamamo's six skills: what each does, its limits, and the program that fires it.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriDanmaku.h"

enum class EMatsuriBossSkill : uint8
{
	Ring,		// 狐火・環: widening rings with a gap to slip through
	Spiral,		// 狐火・螺旋: turning spiral arms
	Pursuit,	// 鬼火・追: aimed fans that home for a moment - never stand still
	TailSweep,	// 九尾薙ぎ: a wall across the courtyard, low (go up) or high (go down)
	FoxRain,	// 狐の雨: rain on one spot - a cover, a platform, or where the player stands
	Phantom,	// 幻影・瞬身: she blinks to an anchor; phantoms fire from the player's flanks
	Count
};

/** Who chose an action (shown on the debug panel and stored in the fight record). */
enum class EMatsuriBossSource : uint8
{
	Rules,		// the fixed rotation: always available
	Learner,	// the local contextual bandit over past fights
	Model		// the language model (DeepSeek / Ollama) reasoning over past fights in context
};

constexpr int32 MatsuriBossParamCount = 4;

/** One decision, already inside the skill's limits (see Matsuri::BossSkills::Clamped). */
struct FMatsuriBossAction
{
	EMatsuriBossSkill Skill = EMatsuriBossSkill::Ring;
	float Params[MatsuriBossParamCount] = { 0.5f, 0.5f, 0.5f, 0.5f };	// each 0..1; meaning per skill (ParamName)
	int32 Target = -1;		// FoxRain: zone index (-1 = where the player stands); Phantom: anchor index
	FString Taunt;
	FString Why;
	EMatsuriBossSource Source = EMatsuriBossSource::Rules;
};

/** What a skill program sees of the courtyard. */
struct FMatsuriBossView
{
	FVector Boss = FVector::ZeroVector;
	FVector Player = FVector::ZeroVector;	// the centre of the player's hit sphere
	FVector Center = FVector::ZeroVector;	// the courtyard's centre
	float GroundZ = 0.0f;
	float HalfExtent = 3000.0f;				// the courtyard is a square of +-HalfExtent round Center
	TArray<FVector> Zones;					// rain targets (ground points of covers and platforms)
	TArray<FVector> Anchors;				// where she can stand
	int32 Phase = 1;						// 1..3
};

/** A skill in progress. */
struct FMatsuriSkillProgram
{
	FMatsuriBossAction Action;
	float Time = 0.0f;
	float Duration = 0.0f;
	float NextShot = 0.0f;
	int32 Shots = 0;
	float Spin = 0.0f;
	FVector Center = FVector::ZeroVector;	// rain's spot, a sweep's start, ...
	FVector Heading = FVector::ForwardVector;
	TArray<FVector> Phantoms;
};

namespace Matsuri::BossSkills
{
	constexpr int32 Count = static_cast<int32>(EMatsuriBossSkill::Count);
	constexpr int32 PresetsPerSkill = 3;
	constexpr float TelegraphSeconds = 0.6f;	// every skill shows where it comes from before the first bullet

	/** Stable machine name used in prompts, records and JSON ("fox_ring"). */
	SUZUNEMATSURI_API const TCHAR* Id(EMatsuriBossSkill Skill);
	SUZUNEMATSURI_API bool FromId(const FString& Id, EMatsuriBossSkill& OutSkill);
	SUZUNEMATSURI_API FText DisplayName(EMatsuriBossSkill Skill);

	/** Machine name and one-line meaning of parameter Index (empty name = unused). */
	SUZUNEMATSURI_API const TCHAR* ParamName(EMatsuriBossSkill Skill, int32 Index);
	SUZUNEMATSURI_API const TCHAR* ParamMeaning(EMatsuriBossSkill Skill, int32 Index);
	/** What the skill does and what it forces the player to do (for the prompt). */
	SUZUNEMATSURI_API const TCHAR* Description(EMatsuriBossSkill Skill);

	/** Phase 1 has the three foxfire skills; phase 2 adds the sweep and the rain; phase 3 adds the phantoms. */
	SUZUNEMATSURI_API bool AllowedIn(EMatsuriBossSkill Skill, int32 Phase);

	/** Params into 0..1 and Target into range. False when the skill is not allowed in Phase. */
	SUZUNEMATSURI_API bool Clamp(FMatsuriBossAction& Action, int32 Phase, int32 ZoneCount, int32 AnchorCount);

	/** Three parameter sets per skill: the local learner's arms. */
	SUZUNEMATSURI_API FMatsuriBossAction Preset(EMatsuriBossSkill Skill, int32 Preset);

	/** Damage carried by each bullet for this action; denser patterns are individually lighter. */
	SUZUNEMATSURI_API float BulletDamageFor(const FMatsuriBossAction& Action);

	/** Seconds from the start of the skill to its last bullet (the bullets then fly on). */
	SUZUNEMATSURI_API float Duration(const FMatsuriBossAction& Action);

	SUZUNEMATSURI_API FLinearColor Color(EMatsuriBossSkill Skill);

	SUZUNEMATSURI_API FMatsuriSkillProgram Begin(const FMatsuriBossAction& Action, const FMatsuriBossView& View);

	/** Fires what is due by the program's new time into Out. False once the skill is over. */
	SUZUNEMATSURI_API bool Advance(FMatsuriSkillProgram& Program, float Dt, const FMatsuriBossView& View, FRandomStream& Random,
		TArray<FMatsuriBullet>& Out);
}
