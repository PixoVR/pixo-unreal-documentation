// Copyright PixoVR, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FPixoDocumentationModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command. */
	void PluginButtonClicked();

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

	/** The tab name for the PixoDocumentation editor tab */
	//static FName PixoDocumentationTabName;

	/** The default label for the PixoDocumentation editor tab */
	//static FText PixoDocumentationTabLabel;
	
	/** The tab name for the PixoDocumentation editor tab */
	//static FName PixoDocumentationConfigEditorTabName;

	/** The default label for the PixoDocumentation editor tab */
	//static FText PixoDocumentationConfigEditorTabLabel;
	
private:

	void RegisterMenus();


private:
	TSharedPtr<class FUICommandList> PluginCommands;
};
