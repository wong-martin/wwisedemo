// Copyright Audiokinetic 2015

using UnrealBuildTool;
using System.Collections.Generic;

public class WwiseDemoGameTarget : TargetRules
{
	public WwiseDemoGameTarget(TargetInfo Target)
	{
		Type = TargetType.Game;
	}

	//
	// TargetRules interface.
	//

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutExtraModuleNames.AddRange( new string[] { "WwiseDemoGame" } );
	}
}
