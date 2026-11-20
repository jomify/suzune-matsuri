// Suzune's Night Festival - the 3D stage behind the title menu: the three heroines at a shrine gate.

#pragma once

#include "CoreMinimal.h"

class ACameraActor;
class UWorld;

namespace Matsuri::TitleStage
{
	/**
	 *  Builds lights, ground, torii, lanterns and any imported heroines playing their idle clip, then
	 *  returns the camera framing the stage. The torii and ground use engine shapes when optional art is
	 *  absent, so a source checkout still has a readable title screen.
	 */
	SUZUNEMATSURI_API ACameraActor* Build(UWorld& World);
}
