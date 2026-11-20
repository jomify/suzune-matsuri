// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SuzuneMatsuri : ModuleRules
{
	public SuzuneMatsuri(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Niagara",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities",
			"MediaAssets"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore", "RHI", "RenderCore", "HTTP" });	// HTTP: the boss's model client (DeepSeek / Ollama)	// RHI / RenderCore: frame timings for the perf probe

		// PIE-driven end-to-end tests (Matsuri/Tests) use editor latent commands.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.AddRange(new string[] {
			"SuzuneMatsuri",
			"SuzuneMatsuri/Variant_Horror",
			"SuzuneMatsuri/Variant_Horror/UI",
			"SuzuneMatsuri/Variant_Shooter",
			"SuzuneMatsuri/Variant_Shooter/AI",
			"SuzuneMatsuri/Variant_Shooter/UI",
			"SuzuneMatsuri/Variant_Shooter/Weapons",
			"SuzuneMatsuri/Matsuri/Core",
			"SuzuneMatsuri/Matsuri/Combat",
			"SuzuneMatsuri/Matsuri/Enemies",
			"SuzuneMatsuri/Matsuri/Level",
			"SuzuneMatsuri/Matsuri/Pickups",
			"SuzuneMatsuri/Matsuri/UI",
			"SuzuneMatsuri/Matsuri/Boss"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
