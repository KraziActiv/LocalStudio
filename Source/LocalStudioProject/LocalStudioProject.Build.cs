// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LocalStudioProject : ModuleRules
{
	public LocalStudioProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore", "EnhancedInput", "AIModule", "GameplayTasks", "Networking", "Json",
		"JsonUtilities" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
	}
}