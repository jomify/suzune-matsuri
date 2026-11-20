// Suzune's Night Festival - placing pieces of the Mountain Temple pack: on a point, to a height, into a box.

#include "MatsuriShrineProps.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "MatsuriArt.h"

namespace
{
	struct FPrefabPiece
	{
		const TCHAR* Mesh;
		float Location[3];
		float Rotation[3];	// pitch, yaw, roll
		float Scale[3];
		const TCHAR* Material0;
		const TCHAR* Material1;
	};

	const FPrefabPiece TempleHallPieces[] = {
#include "MatsuriTemplePrefab.inl"
	};

	AActor* SpawnHolder(UWorld& World, const FTransform& Transform)
	{
		AActor* Holder = World.SpawnActor<AActor>(AActor::StaticClass(), Transform);
		if (!Holder)
		{
			return nullptr;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
		Root->SetMobility(EComponentMobility::Movable);
		Holder->SetRootComponent(Root);
		Root->RegisterComponent();
		Holder->SetActorTransform(Transform);
		Holder->Tags.Add(TEXT("MatsuriShrinePack"));
		return Holder;
	}

	FTransform Placement(const UStaticMesh& Mesh, const FVector& Base, float Yaw, const FVector& Scale)
	{
		// The bottom-centre of the mesh's own bounds, carried to Base.
		const FBox Local = Mesh.GetBoundingBox();
		const FVector Foot((Local.Min.X + Local.Max.X) * 0.5f, (Local.Min.Y + Local.Max.Y) * 0.5f, Local.Min.Z);
		const FQuat Turn = FRotator(0.0f, Yaw, 0.0f).Quaternion();
		return FTransform(Turn, Base - Turn.RotateVector(Foot * Scale), Scale);
	}
}

FVector Matsuri::ShrineProps::Size(const TCHAR* Name)
{
	const UStaticMesh* Mesh = Matsuri::Art::PackMesh(Name);
	return Mesh ? Mesh->GetBoundingBox().GetSize() : FVector::ZeroVector;
}

bool Matsuri::ShrineProps::Bounds(const TCHAR* Name, const FVector& Base, float Yaw, const FVector& Scale, FBox& OutBounds)
{
	const UStaticMesh* Mesh = Matsuri::Art::PackMesh(Name);
	if (!Mesh)
	{
		return false;
	}
	OutBounds = Mesh->GetBoundingBox().TransformBy(Placement(*Mesh, Base, Yaw, Scale));
	return true;
}

AStaticMeshActor* Matsuri::ShrineProps::Place(UWorld& World, const TCHAR* Name, const FVector& Base, float Yaw, const FVector& Scale, bool bCastShadow)
{
	UStaticMesh* Mesh = Matsuri::Art::PackMesh(Name);
	if (!Mesh)
	{
		return nullptr;
	}
	const FTransform Transform = Placement(*Mesh, Base, Yaw, Scale);
	AStaticMeshActor* Actor = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);	// gameplay keeps its own boxes
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(bCastShadow);
	Actor->Tags.Add(TEXT("MatsuriShrinePack"));
	Actor->FinishSpawning(Transform);
	return Actor;
}

float Matsuri::ShrineProps::ScaleToHeight(const TCHAR* Name, float Height)
{
	const FVector MeshSize = Size(Name);
	return MeshSize.Z > KINDA_SMALL_NUMBER ? Height / MeshSize.Z : 1.0f;
}

FVector Matsuri::ShrineProps::FitScale(const TCHAR* Name, const FVector& BoxSize, float& OutYaw)
{
	const FVector MeshSize = Size(Name);
	if (MeshSize.IsNearlyZero())
	{
		OutYaw = 0.0f;
		return FVector::OneVector;
	}
	// Turn the mesh so its long side runs along the box's long side.
	const bool bMeshAlongX = MeshSize.X >= MeshSize.Y;
	const bool bBoxAlongX = BoxSize.X >= BoxSize.Y;
	OutYaw = bMeshAlongX == bBoxAlongX ? 0.0f : 90.0f;
	const float Along = FMath::Max(BoxSize.X, BoxSize.Y);
	const float Across = FMath::Min(BoxSize.X, BoxSize.Y);
	const float MeshAlong = FMath::Max(MeshSize.X, MeshSize.Y);
	const float MeshAcross = FMath::Max(FMath::Min(MeshSize.X, MeshSize.Y), 1.0f);
	const FVector Local = bMeshAlongX ? FVector(Along / MeshAlong, Across / MeshAcross, BoxSize.Z / MeshSize.Z)
		: FVector(Across / MeshAcross, Along / MeshAlong, BoxSize.Z / MeshSize.Z);
	return Local;
}

UNiagaraComponent* Matsuri::ShrineProps::Effect(UWorld& World, const TCHAR* Name, const FVector& At, float Scale)
{
	UNiagaraSystem* System = Matsuri::Art::PackEffect(Name);
	return System ? UNiagaraFunctionLibrary::SpawnSystemAtLocation(&World, System, At, FRotator::ZeroRotator, FVector(Scale), false) : nullptr;
}

AActor* Matsuri::ShrineProps::PlaceTempleHall(UWorld& World, const FVector& Origin, float Yaw)
{
	if (!Matsuri::Art::HasShrinePack())
	{
		return nullptr;
	}
	AActor* Hall = SpawnHolder(World, FTransform(FRotator(0.0f, Yaw, 0.0f), Origin));
	if (!Hall)
	{
		return nullptr;
	}
	for (const FPrefabPiece& Piece : TempleHallPieces)
	{
		UStaticMesh* Mesh = Matsuri::Art::PackMesh(Piece.Mesh);
		if (!Mesh)
		{
			continue;
		}
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Hall);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);	// the honden box keeps the collision
		Part->SetCanEverAffectNavigation(false);
		Part->SetupAttachment(Hall->GetRootComponent());
		Part->SetRelativeTransform(FTransform(FRotator(Piece.Rotation[0], Piece.Rotation[1], Piece.Rotation[2]),
			FVector(Piece.Location[0], Piece.Location[1], Piece.Location[2]), FVector(Piece.Scale[0], Piece.Scale[1], Piece.Scale[2])));
		const TCHAR* const Overrides[] = { Piece.Material0, Piece.Material1 };
		for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(Overrides); ++Slot)
		{
			if (UMaterialInterface* Material = Matsuri::Art::PackMaterial(Overrides[Slot]))
			{
				Part->SetMaterial(Slot, Material);
			}
		}
		Part->RegisterComponent();
		Hall->AddInstanceComponent(Part);
	}
	return Hall;
}

UInstancedStaticMeshComponent* Matsuri::ShrineProps::Scatter(UWorld& World, const TCHAR* Name, const TArray<FTransform>& Instances)
{
	UStaticMesh* Mesh = Matsuri::Art::PackMesh(Name);
	AActor* Holder = Mesh && Instances.Num() > 0 ? SpawnHolder(World, FTransform::Identity) : nullptr;
	if (!Holder)
	{
		return nullptr;
	}
	UInstancedStaticMeshComponent* Copies = NewObject<UInstancedStaticMeshComponent>(Holder);
	Copies->SetMobility(EComponentMobility::Movable);
	Copies->SetStaticMesh(Mesh);
	Copies->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Copies->SetCanEverAffectNavigation(false);
	Copies->SetCastShadow(false);
	Copies->SetupAttachment(Holder->GetRootComponent());
	Copies->RegisterComponent();
	Copies->AddInstances(Instances, false, true);
	Holder->AddInstanceComponent(Copies);
	return Copies;
}
