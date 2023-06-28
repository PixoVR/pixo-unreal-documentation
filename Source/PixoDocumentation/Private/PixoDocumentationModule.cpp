// Copyright PixoVR, Inc. All Rights Reserved.

#include "PixoDocumentationModule.h"

#include "PixoDocumentation.h"
#include "PixoDocumentationStyle.h"
#include "PixoDocumentationCommands.h"

#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

#include "Dialogs/SOutputLogDialog.h"

static const FName PixoDocumentationTabName("pDocumentation");

#define LOCTEXT_NAMESPACE "FPixoDocumentationModule"

DEFINE_LOG_CATEGORY_STATIC(PixoDocumentation, Log, All);

void FPixoDocumentationModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FPixoDocumentationStyle::Initialize();
	FPixoDocumentationStyle::ReloadTextures();

	FPixoDocumentationCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FPixoDocumentationCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FPixoDocumentationModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPixoDocumentationModule::RegisterMenus));
}

void FPixoDocumentationModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FPixoDocumentationStyle::Shutdown();

	FPixoDocumentationCommands::Unregister();
}

void FPixoDocumentationModule::PluginButtonClicked()
{
	// Put your "OnButtonClicked" stuff here
	FText DialogText = FText::Format(
		LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
		FText::FromString(TEXT("FPixoDocumentationModule::PluginButtonClicked()")),
		FText::FromString(TEXT("PixoDocumentation.cpp"))
	);
	FMessageDialog::Open(EAppMsgType::Ok, DialogText);

	SOutputLogDialog::Open(
		FText::FromString(TEXT("Some title")),
		FText::FromString(TEXT("Some header")),
		FText::FromString(TEXT("Some log output...")),
		FText::FromString(TEXT("some footer"))
	);
}

void FPixoDocumentationModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FPixoDocumentationCommands::Get().PluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FPixoDocumentationCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPixoDocumentationModule, PixoDocumentation)
