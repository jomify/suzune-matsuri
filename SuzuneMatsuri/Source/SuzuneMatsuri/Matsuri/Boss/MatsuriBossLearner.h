// Suzune's Night Festival - the boss's local learner: a contextual bandit over her skills.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriBossMemory.h"

/**
 *  LinUCB over 18 arms (6 skills x 3 parameter presets). Each arm keeps a ridge regression from the
 *  situation (FMatsuriBossObservation::Features, plus a bias) to the reward it earned, and she picks the
 *  arm with the best estimate plus an exploration bonus that shrinks as the arm is tried. It is the
 *  fallback when the language model is off, slow or wrong - and its estimates are also shown to the
 *  model as a hint. Learns online from every step, and can be rebuilt from the fight records.
 */
class SUZUNEMATSURI_API FMatsuriBossLearner
{
public:
	static constexpr int32 Arms = Matsuri::BossSkills::Count * Matsuri::BossSkills::PresetsPerSkill;
	static constexpr int32 Dimensions = 10;		// 9 features + bias
	static constexpr double DefaultAlpha = 0.6;

	explicit FMatsuriBossLearner(double InAlpha = DefaultAlpha);

	static int32 ArmFor(EMatsuriBossSkill Skill, int32 Preset);
	static EMatsuriBossSkill SkillOf(int32 Arm);
	static int32 PresetOf(int32 Arm);

	/** The best allowed arm for this situation (phase gating applies). */
	int32 Choose(const FMatsuriBossObservation& Observation) const;

	/** Estimated reward (no exploration bonus) of Arm here. */
	double Estimate(int32 Arm, const FMatsuriBossObservation& Observation) const;

	/** The arm closest to an action someone else chose (the model's free parameters map to a preset). */
	static int32 NearestArm(const FMatsuriBossAction& Action);

	void Update(const FMatsuriBossObservation& Observation, int32 Arm, double Reward);

	/** Forgets everything and learns again from recorded steps. */
	void Rebuild(const TArray<FMatsuriBossStep>& Steps);

	int32 Pulls(int32 Arm) const { return Counts[Arm]; }

	bool Save(const FString& Path) const;
	bool Load(const FString& Path);

private:
	static TArray<double> Context(const FMatsuriBossObservation& Observation);
	/** A^-1 for one arm (Gauss-Jordan on a 10x10, cheap enough for every decision). */
	static TArray<double> Inverse(const TArray<double>& Matrix);

	double Alpha;
	TArray<double> A[Arms];		// Dimensions x Dimensions, row-major, starts as the identity
	TArray<double> B[Arms];		// Dimensions
	int32 Counts[Arms] = {};
};
