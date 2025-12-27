// Copyright Epic Games, Inc. All Rights Reserved.

#include "GF_Lobby_CodeRuntimeModule.h"

#define LOCTEXT_NAMESPACE "FGF_Lobby_CodeRuntimeModule"

void FGF_Lobby_CodeRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory;
	// the exact timing is specified in the .uplugin file per-module
}

void FGF_Lobby_CodeRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
	// For modules that support dynamic reloading, we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGF_Lobby_CodeRuntimeModule, GF_Lobby_CodeRuntime)
