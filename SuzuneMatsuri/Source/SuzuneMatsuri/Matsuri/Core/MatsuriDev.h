// Suzune's Night Festival - developer mode: jump to any floor, demo switches, in-game shortcuts.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 *  For testing and showing the game: the title's 開発者モード panel sets these (all console variables,
 *  so they can also be typed), and with matsuri.Dev on, F5-F8 work during play (UMatsuriDevComponent).
 */
namespace Matsuri::Dev
{
	SUZUNEMATSURI_API bool IsEnabled();
	SUZUNEMATSURI_API void SetEnabled(bool bEnabled);

	SUZUNEMATSURI_API bool GetBool(const TCHAR* Name);
	SUZUNEMATSURI_API void SetBool(const TCHAR* Name, bool bValue);
	SUZUNEMATSURI_API FString GetString(const TCHAR* Name);
	SUZUNEMATSURI_API void SetString(const TCHAR* Name, const FString& Value);

	/** Cycles a string variable through Values (wrapping); returns the new value. */
	SUZUNEMATSURI_API FString Cycle(const TCHAR* Name, const TArray<FString>& Values);

	/** F5: the floor counts as cleared (on the boss floor she is defeated). */
	SUZUNEMATSURI_API void ClearFloor(UWorld* World);

	/** F6: ammo to the cap, ultimate full. */
	SUZUNEMATSURI_API void Restock(UWorld* World);

	/** F7: the boss starts her next health bar (or is defeated on her last). False off the boss floor. */
	SUZUNEMATSURI_API bool NextBossPhase(UWorld* World);

	/** F8: god mode on / off; returns the new state. */
	SUZUNEMATSURI_API bool ToggleGod();
}
