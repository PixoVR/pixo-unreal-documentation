// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PixoDocumentation : ModuleRules
{
	public PixoDocumentation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		//PrecompileForTargets = PrecompileTargetsType.Any;

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
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",

				"Projects",
				"InputCore",
				"ToolMenus",

				"BlueprintGraph",
				"ImageWrapper",
				//"HTTP",
				//"Json",
				//"RHI",
				//"RenderCore",
				//"Launch"
				// ... add private dependencies that you statically link with here ...	
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
