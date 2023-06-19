// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixoDocumentationCommands.h"

#define LOCTEXT_NAMESPACE "FPixoDocumentationModule"

void FPixoDocumentationCommands::RegisterCommands()
{
	//UI_COMMAND(PluginAction, "PixoDocumentation", "Execute PixoDocumentation action", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PluginAction, "PixoDocumentation", "Execute PixoDocumentation action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
