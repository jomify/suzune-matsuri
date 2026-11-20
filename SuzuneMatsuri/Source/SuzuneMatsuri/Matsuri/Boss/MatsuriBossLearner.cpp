// Suzune's Night Festival - the boss's local learner: a contextual bandit over her skills.

#include "MatsuriBossLearner.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr int32 D = FMatsuriBossLearner::Dimensions;
}

FMatsuriBossLearner::FMatsuriBossLearner(double InAlpha)
	: Alpha(InAlpha)
{
	for (int32 Arm = 0; Arm < Arms; ++Arm)
	{
		A[Arm].SetNumZeroed(D * D);
		for (int32 i = 0; i < D; ++i)
		{
			A[Arm][i * D + i] = 1.0;
		}
		B[Arm].SetNumZeroed(D);
	}
}

int32 FMatsuriBossLearner::ArmFor(EMatsuriBossSkill Skill, int32 Preset)
{
	return static_cast<int32>(Skill) * Matsuri::BossSkills::PresetsPerSkill + FMath::Clamp(Preset, 0, Matsuri::BossSkills::PresetsPerSkill - 1);
}

EMatsuriBossSkill FMatsuriBossLearner::SkillOf(int32 Arm)
{
	return static_cast<EMatsuriBossSkill>(FMath::Clamp(Arm, 0, Arms - 1) / Matsuri::BossSkills::PresetsPerSkill);
}

int32 FMatsuriBossLearner::PresetOf(int32 Arm)
{
	return FMath::Clamp(Arm, 0, Arms - 1) % Matsuri::BossSkills::PresetsPerSkill;
}

TArray<double> FMatsuriBossLearner::Context(const FMatsuriBossObservation& Observation)
{
	TArray<double> X;
	X.Reserve(D);
	X.Add(1.0);	// bias: what the arm earns anywhere
	for (const float Feature : Observation.Features())
	{
		X.Add(Feature);
	}
	X.SetNumZeroed(D);
	return X;
}

TArray<double> FMatsuriBossLearner::Inverse(const TArray<double>& Matrix)
{
	TArray<double> M = Matrix;
	TArray<double> Inv;
	Inv.SetNumZeroed(D * D);
	for (int32 i = 0; i < D; ++i)
	{
		Inv[i * D + i] = 1.0;
	}
	for (int32 Col = 0; Col < D; ++Col)
	{
		int32 Pivot = Col;
		for (int32 Row = Col + 1; Row < D; ++Row)
		{
			if (FMath::Abs(M[Row * D + Col]) > FMath::Abs(M[Pivot * D + Col]))
			{
				Pivot = Row;
			}
		}
		if (Pivot != Col)
		{
			for (int32 k = 0; k < D; ++k)
			{
				Swap(M[Col * D + k], M[Pivot * D + k]);
				Swap(Inv[Col * D + k], Inv[Pivot * D + k]);
			}
		}
		const double Diagonal = M[Col * D + Col];	// A = I + sum(x x^T) is positive definite: never zero
		for (int32 k = 0; k < D; ++k)
		{
			M[Col * D + k] /= Diagonal;
			Inv[Col * D + k] /= Diagonal;
		}
		for (int32 Row = 0; Row < D; ++Row)
		{
			if (Row == Col)
			{
				continue;
			}
			const double Factor = M[Row * D + Col];
			for (int32 k = 0; k < D; ++k)
			{
				M[Row * D + k] -= Factor * M[Col * D + k];
				Inv[Row * D + k] -= Factor * Inv[Col * D + k];
			}
		}
	}
	return Inv;
}

double FMatsuriBossLearner::Estimate(int32 Arm, const FMatsuriBossObservation& Observation) const
{
	const TArray<double> X = Context(Observation);
	const TArray<double> AInv = Inverse(A[Arm]);
	double Value = 0.0;
	for (int32 i = 0; i < D; ++i)
	{
		double Theta = 0.0;
		for (int32 k = 0; k < D; ++k)
		{
			Theta += AInv[i * D + k] * B[Arm][k];
		}
		Value += Theta * X[i];
	}
	return Value;
}

int32 FMatsuriBossLearner::Choose(const FMatsuriBossObservation& Observation) const
{
	const TArray<double> X = Context(Observation);
	int32 Best = INDEX_NONE;
	double BestScore = -TNumericLimits<double>::Max();
	// Ties (every untried arm looks the same) go through the skills before the presets: a boss with no
	// experience opens ring, spiral, pursuit... not ring, ring, ring.
	for (int32 Order = 0; Order < Arms; ++Order)
	{
		const int32 Arm = ArmFor(static_cast<EMatsuriBossSkill>(Order % Matsuri::BossSkills::Count), Order / Matsuri::BossSkills::Count);
		if (!Matsuri::BossSkills::AllowedIn(SkillOf(Arm), Observation.Phase))
		{
			continue;
		}
		const TArray<double> AInv = Inverse(A[Arm]);
		double Mean = 0.0;
		double Spread = 0.0;
		for (int32 i = 0; i < D; ++i)
		{
			double Theta = 0.0;
			double AInvX = 0.0;
			for (int32 k = 0; k < D; ++k)
			{
				Theta += AInv[i * D + k] * B[Arm][k];
				AInvX += AInv[i * D + k] * X[k];
			}
			Mean += Theta * X[i];
			Spread += X[i] * AInvX;
		}
		const double Score = Mean + Alpha * FMath::Sqrt(FMath::Max(Spread, 0.0));
		if (Score > BestScore + 1e-9)	// deterministic: the first in that order wins a tie
		{
			BestScore = Score;
			Best = Arm;
		}
	}
	return Best;
}

int32 FMatsuriBossLearner::NearestArm(const FMatsuriBossAction& Action)
{
	int32 Best = ArmFor(Action.Skill, 0);
	float BestDistance = TNumericLimits<float>::Max();
	for (int32 Preset = 0; Preset < Matsuri::BossSkills::PresetsPerSkill; ++Preset)
	{
		const FMatsuriBossAction Candidate = Matsuri::BossSkills::Preset(Action.Skill, Preset);
		float Distance = 0.0f;
		for (int32 i = 0; i < MatsuriBossParamCount; ++i)
		{
			Distance += FMath::Square(Candidate.Params[i] - Action.Params[i]);
		}
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = ArmFor(Action.Skill, Preset);
		}
	}
	return Best;
}

void FMatsuriBossLearner::Update(const FMatsuriBossObservation& Observation, int32 Arm, double Reward)
{
	if (Arm < 0 || Arm >= Arms || !FMath::IsFinite(Reward))
	{
		return;
	}
	const TArray<double> X = Context(Observation);
	for (int32 i = 0; i < D; ++i)
	{
		for (int32 k = 0; k < D; ++k)
		{
			A[Arm][i * D + k] += X[i] * X[k];
		}
		B[Arm][i] += Reward * X[i];
	}
	++Counts[Arm];
}

void FMatsuriBossLearner::Rebuild(const TArray<FMatsuriBossStep>& Steps)
{
	*this = FMatsuriBossLearner(Alpha);
	for (const FMatsuriBossStep& Step : Steps)
	{
		Update(Step.Observation, NearestArm(Step.Action), Step.Reward);
	}
}

bool FMatsuriBossLearner::Save(const FString& Path) const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("alpha"), Alpha);
	TArray<TSharedPtr<FJsonValue>> ArmsJson;
	for (int32 Arm = 0; Arm < Arms; ++Arm)
	{
		TSharedRef<FJsonObject> One = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> AJson, BJson;
		for (const double Value : A[Arm]) { AJson.Add(MakeShared<FJsonValueNumber>(Value)); }
		for (const double Value : B[Arm]) { BJson.Add(MakeShared<FJsonValueNumber>(Value)); }
		One->SetStringField(TEXT("skill"), Matsuri::BossSkills::Id(SkillOf(Arm)));
		One->SetNumberField(TEXT("preset"), PresetOf(Arm));
		One->SetNumberField(TEXT("pulls"), Counts[Arm]);
		One->SetArrayField(TEXT("a"), AJson);
		One->SetArrayField(TEXT("b"), BJson);
		ArmsJson.Add(MakeShared<FJsonValueObject>(One));
	}
	Json->SetArrayField(TEXT("arms"), ArmsJson);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	return FJsonSerializer::Serialize(Json, Writer) && FFileHelper::SaveStringToFile(Text, *Path);
}

bool FMatsuriBossLearner::Load(const FString& Path)
{
	FString Text;
	TSharedPtr<FJsonObject> Json;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json)
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* ArmsJson = nullptr;
	if (!Json->TryGetArrayField(TEXT("arms"), ArmsJson) || ArmsJson->Num() != Arms)
	{
		return false;	// a learner from another table of skills: start again
	}
	FMatsuriBossLearner Loaded(Json->GetNumberField(TEXT("alpha")));
	for (int32 Arm = 0; Arm < Arms; ++Arm)
	{
		const TSharedPtr<FJsonObject>& One = (*ArmsJson)[Arm]->AsObject();
		const TArray<TSharedPtr<FJsonValue>>& AJson = One->GetArrayField(TEXT("a"));
		const TArray<TSharedPtr<FJsonValue>>& BJson = One->GetArrayField(TEXT("b"));
		if (AJson.Num() != D * D || BJson.Num() != D)
		{
			return false;
		}
		for (int32 i = 0; i < D * D; ++i) { Loaded.A[Arm][i] = AJson[i]->AsNumber(); }
		for (int32 i = 0; i < D; ++i) { Loaded.B[Arm][i] = BJson[i]->AsNumber(); }
		Loaded.Counts[Arm] = static_cast<int32>(One->GetNumberField(TEXT("pulls")));
	}
	*this = Loaded;
	return true;
}
