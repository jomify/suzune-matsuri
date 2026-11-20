// Suzune's Night Festival - gray-box parts built from engine basic shapes.

#include "MatsuriGraybox.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MatsuriArt.h"

namespace
{
	const TCHAR* ShapePath(Matsuri::Graybox::EShape Shape)
	{
		switch (Shape)
		{
		case Matsuri::Graybox::EShape::Cube:		return TEXT("/Engine/BasicShapes/Cube.Cube");
		case Matsuri::Graybox::EShape::Sphere:		return TEXT("/Engine/BasicShapes/Sphere.Sphere");
		case Matsuri::Graybox::EShape::Cylinder:	return TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		case Matsuri::Graybox::EShape::Cone:		return TEXT("/Engine/BasicShapes/Cone.Cone");
		}
		return TEXT("/Engine/BasicShapes/Cube.Cube");
	}

	const TCHAR* const BaseMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const TCHAR* const GlowMaterialPath = TEXT("/Game/Matsuri/Materials/M_MatsuriGlow.M_MatsuriGlow");	// tools/ue/create_materials.py
	const FName ColorParameter(TEXT("Color"));
	const FName GlowParameter(TEXT("Glow"));

	const TCHAR* const SurfaceMaterialPath = TEXT("/Game/Matsuri/Materials/M_MatsuriSurface.M_MatsuriSurface");	// tools/ue/create_materials.py

	struct FSurfaceLook
	{
		const TCHAR* Texture;
		float TileSize;		// cm per repeat
		float Brightness;
	};

	FSurfaceLook LookFor(Matsuri::Graybox::ESurface Surface)
	{
		switch (Surface)
		{
		case Matsuri::Graybox::ESurface::Wood:		return { TEXT("T_Wood_Planks"), 260.0f, 1.5f };
		case Matsuri::Graybox::ESurface::Stone:		return { TEXT("T_Stone_Blocks"), 220.0f, 1.3f };
		case Matsuri::Graybox::ESurface::Paving:	return { TEXT("T_Stone_Paving"), 320.0f, 1.0f };
		case Matsuri::Graybox::ESurface::Roof:		return { TEXT("T_Stone_Blocks"), 120.0f, 0.6f };
		case Matsuri::Graybox::ESurface::Plaster:	return { TEXT("T_Wood_Planks"), 260.0f, 1.5f };
		}
		return { TEXT(""), 300.0f, 1.0f };
	}

	/** The Mountain Temple pack's tileable textures (weathered wood, mossy rock, roof shingles). */
	FSurfaceLook PackLookFor(Matsuri::Graybox::ESurface Surface)
	{
		switch (Surface)
		{
		case Matsuri::Graybox::ESurface::Wood:		return { TEXT("Tileable_Wood/T_Wood_Tileable_BaseColor"), 240.0f, 1.25f };
		case Matsuri::Graybox::ESurface::Stone:		return { TEXT("Rock/T_Rock_BaseColor"), 300.0f, 1.35f };
		case Matsuri::Graybox::ESurface::Paving:	return { TEXT(""), 0.0f, 0.0f };	// the flagstones stay
		case Matsuri::Graybox::ESurface::Roof:		return { TEXT("Shingles/T_Shingles_BaseColor"), 160.0f, 1.0f };
		case Matsuri::Graybox::ESurface::Plaster:	return { TEXT("Dirt_Ground/T_Dirt_Ground_BaseColor"), 420.0f, 1.9f };
		}
		return { TEXT(""), 0.0f, 0.0f };
	}
}

UStaticMesh* Matsuri::Graybox::Mesh(EShape Shape)
{
	return LoadObject<UStaticMesh>(nullptr, ShapePath(Shape));
}

UStaticMeshComponent* Matsuri::Graybox::MakePart(AActor& Owner, USceneComponent* Parent, FName Name, EShape Shape,
	const FVector& Offset, const FVector& Scale, const FRotator& Rotation)
{
	UStaticMeshComponent* Part = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(Mesh(Shape));
	Part->SetRelativeLocation(Offset);
	Part->SetRelativeRotation(Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	return Part;
}

void Matsuri::Graybox::Paint(UStaticMeshComponent* Part, const FLinearColor& Color, bool bGlow, float GlowStrength)
{
	UMaterialInterface* Glow = bGlow ? LoadObject<UMaterialInterface>(nullptr, GlowMaterialPath, nullptr, LOAD_Quiet | LOAD_NoWarn) : nullptr;
	UMaterialInterface* Base = Glow ? Glow : LoadObject<UMaterialInterface>(nullptr, BaseMaterialPath);
	if (!Part || !Base)
	{
		return;
	}
	UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0, Base);
	Material->SetVectorParameterValue(ColorParameter, Color);
	if (Glow)
	{
		Material->SetScalarParameterValue(GlowParameter, GlowStrength);
	}
}

void Matsuri::Graybox::PaintSurface(UStaticMeshComponent* Part, ESurface Surface, const FLinearColor& Tint)
{
	const FSurfaceLook PackLook = PackLookFor(Surface);
	UTexture2D* PackTexture = *PackLook.Texture ? Matsuri::Art::PackTexture(PackLook.Texture) : nullptr;
	const FSurfaceLook Look = PackTexture ? PackLook : LookFor(Surface);
	UTexture2D* Texture = PackTexture ? PackTexture : Matsuri::Art::Texture(Look.Texture);
	static TWeakObjectPtr<UMaterialInterface> CachedSurface;
	if (!CachedSurface.IsValid() && Texture)
	{
		CachedSurface = LoadObject<UMaterialInterface>(nullptr, SurfaceMaterialPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
	}
	UMaterialInterface* Base = Texture ? CachedSurface.Get() : nullptr;
	if (!Part || !Base)
	{
		Paint(Part, Tint);
		return;
	}
	UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0, Base);
	Material->SetTextureParameterValue(TEXT("Albedo"), Texture);
	Material->SetVectorParameterValue(TEXT("Tint"), Tint);
	Material->SetScalarParameterValue(TEXT("TileSize"), Look.TileSize);
	Material->SetScalarParameterValue(TEXT("Brightness"), Look.Brightness);
}
