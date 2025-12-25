// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UE5_Multi_ShooterServerTarget : TargetRules
{
	public UE5_Multi_ShooterServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;

		ExtraModuleNames.Add("UE5_Multi_Shooter");
	}
}
