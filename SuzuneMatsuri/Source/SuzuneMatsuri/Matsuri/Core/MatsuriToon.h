// Optional toon prototype for the two inspected opaque Tripo character materials.
#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace Matsuri::Toon
{
    inline bool Apply(USkeletalMeshComponent* Mesh, bool bTamamo)
    {
        if (!Mesh || Mesh->GetNumMaterials() != 1)
        {
            return false;
        }
        const TCHAR* Path = bTamamo
            ? TEXT("/Game/Matsuri/Materials/MI_Toon_Tamamo.MI_Toon_Tamamo")
            : TEXT("/Game/Matsuri/Materials/MI_Toon_Suzune.MI_Toon_Suzune");
        if (UMaterialInterface* Toon = LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_Quiet | LOAD_NoWarn))
        {
            Mesh->SetMaterial(0, Toon);
            return true;
        }
        return false;
    }
}
