// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

#include "PixoDocumentationStyle.h"

class FPixoDocumentationCommands : public TCommands<FPixoDocumentationCommands>
{
public:

	FPixoDocumentationCommands()
		: TCommands<FPixoDocumentationCommands>(TEXT("PixoDocumentation"), NSLOCTEXT("Contexts", "PixoDocumentation", "PixoDocumentation Plugin"), NAME_None, FPixoDocumentationStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
