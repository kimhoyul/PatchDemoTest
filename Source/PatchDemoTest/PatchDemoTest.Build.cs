// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PatchDemoTest : ModuleRules
{
	public PatchDemoTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput", "UMG" });

        PrivateDependencyModuleNames.AddRange(new string[] { "ChunkDownloader", "HTTP", "Slate", "SlateCore" });
    }
}
