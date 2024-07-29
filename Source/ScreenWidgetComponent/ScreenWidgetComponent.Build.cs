using UnrealBuildTool;

public class ScreenWidgetComponent : ModuleRules
{
	public ScreenWidgetComponent(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UMG",
			}
		);
	}
}
