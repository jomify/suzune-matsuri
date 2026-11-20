// Suzune's Night Festival - danmaku: the boss's bullets as plain data, and the shapes they are fired in.

#include "MatsuriDanmaku.h"

namespace
{
	constexpr float RainDrift = 60.0f;	// cm/s sideways, so a rain curtain is not a grid of vertical lines
}

void Matsuri::Danmaku::Step(FMatsuriBullet& Bullet, float Dt, const FVector& Target)
{
	const float PreviousAge = Bullet.Age;
	const float SafeDt = FMath::Max(0.0f, Dt);
	Bullet.PreviousPosition = Bullet.Position;
	Bullet.bHasPreviousPosition = true;
	Bullet.ActiveStartFraction = SafeDt > SMALL_NUMBER ? FMath::Clamp((Bullet.Delay - PreviousAge) / SafeDt, 0.0f, 1.0f) : 0.0f;
	Bullet.Age += SafeDt;
	if (Bullet.Age >= Bullet.Lifetime)
	{
		Bullet.bDead = true;
		return;
	}
	if (Bullet.Age < Bullet.Delay)
	{
		return;
	}
	const float ActiveDt = FMath::Max(0.0f, Bullet.Age - FMath::Max(PreviousAge, Bullet.Delay));
	float Speed = Bullet.Velocity.Size();
	FVector Heading = Speed > KINDA_SMALL_NUMBER ? Bullet.Velocity / Speed : FVector::ForwardVector;
	if (Bullet.HomingDegreesPerSecond > 0.0f && Bullet.Age - Bullet.Delay < Bullet.HomingSeconds)
	{
		const FVector Want = (Target - Bullet.Position).GetSafeNormal();
		const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Heading, Want), -1.0f, 1.0f));
		const float MaxTurn = FMath::DegreesToRadians(Bullet.HomingDegreesPerSecond) * ActiveDt;
		if (Angle > KINDA_SMALL_NUMBER)
		{
			const FVector Axis = FVector::CrossProduct(Heading, Want).GetSafeNormal();
			if (!Axis.IsNearlyZero())
			{
				Heading = Heading.RotateAngleAxisRad(FMath::Min(Angle, MaxTurn), Axis);
			}
		}
	}
	Speed = FMath::Clamp(Speed + Bullet.Accel * ActiveDt, Bullet.MinSpeed, Bullet.MaxSpeed);
	Bullet.Velocity = Heading * Speed;
	Bullet.Position += Bullet.Velocity * ActiveDt;
}

bool Matsuri::Danmaku::Touches(const FMatsuriBullet& Bullet, const FVector& Center, float Radius)
{
	float Fraction = 0.0f;
	return PlayerImpact(Bullet, Center, Center, Radius, Fraction);
}

bool Matsuri::Danmaku::PlayerImpact(const FMatsuriBullet& Bullet, const FVector& PreviousCenter, const FVector& Center,
	float Radius, float& OutFraction)
{
	if (Bullet.bDead || Bullet.Age < Bullet.Delay)
	{
		return false;
	}
	const FVector From = Bullet.bHasPreviousPosition ? Bullet.PreviousPosition : Bullet.Position;
	const FVector PlayerFrom = Bullet.bHasPreviousPosition ? FMath::Lerp(PreviousCenter, Center, Bullet.ActiveStartFraction) : Center;
	const FVector RelativeFrom = From - PlayerFrom;
	const FVector RelativeTo = Bullet.Position - Center;
	const double CombinedRadius = FMath::Max(0.0f, Bullet.Radius) + FMath::Max(0.0f, Radius);
	const double C = RelativeFrom.SizeSquared() - FMath::Square(CombinedRadius);
	if (C <= 0.0)
	{
		OutFraction = 0.0f;
		return true;
	}
	const FVector Delta = RelativeTo - RelativeFrom;
	const double A = Delta.SizeSquared();
	if (A <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}
	const double B = FVector::DotProduct(RelativeFrom, Delta);
	const double Discriminant = B * B - A * C;
	if (Discriminant < 0.0)
	{
		return false;
	}
	const double Fraction = (-B - FMath::Sqrt(Discriminant)) / A;
	if (Fraction < 0.0 || Fraction > 1.0)
	{
		return false;
	}
	OutFraction = static_cast<float>(Fraction);
	return true;
}

bool Matsuri::Danmaku::BoxImpact(const FMatsuriBullet& Bullet, const FBox& Box, float& OutFraction)
{
	if (Bullet.bDead || Bullet.Age < Bullet.Delay || !Box.IsValid)
	{
		return false;
	}
	const FBox Expanded = Box.ExpandBy(FMath::Max(0.0f, Bullet.Radius));
	const FVector From = Bullet.bHasPreviousPosition ? Bullet.PreviousPosition : Bullet.Position;
	const FVector Delta = Bullet.Position - From;
	double Enter = 0.0;
	double Leave = 1.0;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (FMath::IsNearlyZero(Delta[Axis]))
		{
			if (From[Axis] < Expanded.Min[Axis] || From[Axis] > Expanded.Max[Axis])
			{
				return false;
			}
			continue;
		}
		double A = (Expanded.Min[Axis] - From[Axis]) / Delta[Axis];
		double B = (Expanded.Max[Axis] - From[Axis]) / Delta[Axis];
		if (A > B)
		{
			Swap(A, B);
		}
		Enter = FMath::Max(Enter, A);
		Leave = FMath::Min(Leave, B);
		if (Enter > Leave)
		{
			return false;
		}
	}
	OutFraction = static_cast<float>(Enter);
	return true;
}

FVector Matsuri::Danmaku::ConeDirection(const FVector& From, const FVector& Target, float YawDegrees)
{
	const float Distance = FMath::Max(FVector::Dist2D(From, Target), 1.0f);
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Target.Z - From.Z, Distance));
	return FRotator(Pitch, YawDegrees, 0.0f).Vector();
}

void Matsuri::Danmaku::Ring(TArray<FMatsuriBullet>& Out, const FMatsuriBullet& Template, const FVector& Origin, const FVector& Target,
	int32 Count, float Speed, float StartYaw, float GapDegrees, float GapYaw)
{
	const float Step = 360.0f / FMath::Max(Count, 1);
	for (int32 i = 0; i < Count; ++i)
	{
		const float Yaw = StartYaw + Step * i;
		if (GapDegrees > 0.0f && FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, GapYaw)) < GapDegrees * 0.5f)
		{
			continue;
		}
		FMatsuriBullet& Bullet = Out.Add_GetRef(Template);
		Bullet.Position = Origin;
		Bullet.Velocity = ConeDirection(Origin, Target, Yaw) * Speed;
	}
}

void Matsuri::Danmaku::Fan(TArray<FMatsuriBullet>& Out, const FMatsuriBullet& Template, const FVector& Origin, const FVector& Target,
	int32 Count, float SpreadDegrees, float Speed)
{
	const FRotator Aim = (Target - Origin).Rotation();
	for (int32 i = 0; i < Count; ++i)
	{
		const float Offset = Count > 1 ? FMath::Lerp(-SpreadDegrees * 0.5f, SpreadDegrees * 0.5f, static_cast<float>(i) / (Count - 1)) : 0.0f;
		FMatsuriBullet& Bullet = Out.Add_GetRef(Template);
		Bullet.Position = Origin;
		Bullet.Velocity = FRotator(Aim.Pitch, Aim.Yaw + Offset, 0.0f).Vector() * Speed;
	}
}

void Matsuri::Danmaku::Wall(TArray<FMatsuriBullet>& Out, const FMatsuriBullet& Template, const FVector& Start, const FVector& Heading,
	float Width, float Spacing, float GroundZ, float HeightMin, float HeightMax, float RowSpacing, float Speed)
{
	const FVector Forward = Heading.GetSafeNormal2D();
	const FVector Side(-Forward.Y, Forward.X, 0.0f);
	const int32 Columns = FMath::Max(1, FMath::FloorToInt32(Width / Spacing) + 1);
	const int32 Rows = FMath::Max(1, FMath::FloorToInt32((HeightMax - HeightMin) / RowSpacing) + 1);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const float Z = GroundZ + HeightMin + Row * RowSpacing;
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			FMatsuriBullet& Bullet = Out.Add_GetRef(Template);
			Bullet.Position = FVector(Start.X, Start.Y, Z) + Side * (Column * Spacing - Width * 0.5f);
			Bullet.Velocity = Forward * Speed;
		}
	}
}

void Matsuri::Danmaku::Rain(TArray<FMatsuriBullet>& Out, const FMatsuriBullet& Template, const FVector& Center, float Radius,
	float Height, int32 Count, float Speed, FRandomStream& Random)
{
	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
		const float Distance = Radius * FMath::Sqrt(Random.FRand());	// uniform over the disc
		FMatsuriBullet& Bullet = Out.Add_GetRef(Template);
		Bullet.Position = Center + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, Height);
		Bullet.Velocity = FVector(Random.FRandRange(-RainDrift, RainDrift), Random.FRandRange(-RainDrift, RainDrift), -Speed);
	}
}
