// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LocalStudio : ModuleRules
{
	public LocalStudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);


        PublicDependencyModuleNames.AddRange(
    new string[]
    {
        "Core",
        "CoreUObject",      
        "Engine",           
        "HTTP",
        "Json",
        "JsonUtilities",
        "DeveloperSettings",
        "EditorSubsystem"
    }
);

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
        "Projects",
        "InputCore",
        "EditorFramework",
        "UnrealEd",
        "ToolMenus",
        "Slate",            
        "SlateCore",
        "Settings",
        "AssetTools",
        "AssetRegistry",
        "Landscape",
        "LandscapeEditor",
        "MaterialEditor",
        "ImageWrapper",
        "ImageCore",
        "LiveCoding",
        "DesktopPlatform",
        "SourceCodeAccess",
        "Kismet",
        "BlueprintGraph",
        "UMG"
            }
        );


        DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
