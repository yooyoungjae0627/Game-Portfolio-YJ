// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class UE5_Multi_Shooter : ModuleRules
{
    public UE5_Multi_Shooter(ReadOnlyTargetRules Target) : base(Target)
    {
        // PCH(미리 컴파일된 헤더) 사용 방식
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 다른 모듈에서도 접근 가능한 공개 의존성
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",            // 기본 타입, 로그
			"CoreUObject",     // UObject, 리플렉션
			"Engine",          // Actor, World
			"InputCore",       // 기본 입력
			"EnhancedInput",   // Enhanced Input 시스템

			"GameplayTags",    // GAS 태그 시스템
			"ModularGameplay", // GameFeature 기반 구조
			"GameFeatures",    // GameFeature 플러그인
			"Projects",        // PluginManager 해결
			"NetCore",
            "UMG",
            "CommonUI",
        });

        // 필요하면 Private 쪽에도 가능
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
        });

        // ============================
        // WinRT (C++/WinRT) Support
        // ============================
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // C++/WinRT는 예외/RTTI가 필요한 경우가 많음
            bEnableExceptions = true;
            bUseRTTI = true;

            // WinRT 관련 라이브러리
            PublicSystemLibraries.AddRange(new string[]
            {
                "runtimeobject.lib",
            });

            // Windows SDK의 cppwinrt include path를 명시적으로 추가
            // 예: C:\Program Files (x86)\Windows Kits\10\Include\<ver>\cppwinrt
            string SdkDir = Target.WindowsPlatform.WindowsSdkDir;
            string SdkVer = Target.WindowsPlatform.WindowsSdkVersion;

            string CppWinRtInclude = Path.Combine(SdkDir, "Include", SdkVer, "cppwinrt");
            if (Directory.Exists(CppWinRtInclude))
            {
                PublicSystemIncludePaths.Add(CppWinRtInclude);
            }
            else
            {
                // 환경에 따라 WindowsSdkVersion이 비어있거나 경로가 다를 수 있어서,
                // 폴백으로 Include 밑에서 cppwinrt를 검색
                string IncludeRoot = Path.Combine(SdkDir, "Include");
                if (Directory.Exists(IncludeRoot))
                {
                    foreach (string VerDir in Directory.GetDirectories(IncludeRoot))
                    {
                        string Candidate = Path.Combine(VerDir, "cppwinrt");
                        if (Directory.Exists(Candidate))
                        {
                            PublicSystemIncludePaths.Add(Candidate);
                            break;
                        }
                    }
                }
            }

            // (선택) 최소 매크로 정의
            PublicDefinitions.Add("WINRT_LEAN_AND_MEAN=1");
        }
    }
}
