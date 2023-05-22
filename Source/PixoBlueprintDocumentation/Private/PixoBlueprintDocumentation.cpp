// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixoBlueprintDocumentation.h"

#define LOCTEXT_NAMESPACE "FPixoBlueprintDocumentationModule"

DEFINE_LOG_CATEGORY_STATIC(PixoBlueprintDocumentation, Log, All);

void FPixoBlueprintDocumentationModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	UE_LOG(PixoBlueprintDocumentation, Log, TEXT("Initialized."));
}

void FPixoBlueprintDocumentationModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPixoBlueprintDocumentationModule, PixoBlueprintDocumentation)