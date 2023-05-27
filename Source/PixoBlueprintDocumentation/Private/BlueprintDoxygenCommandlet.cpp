// (c) 2023 PixoVR

#include "BlueprintDoxygenCommandlet.h"

#include "AssetRegistryModule.h"
#include "GenericPlatform/GenericPlatformFile.h"
//#include "GenericPlatform/GenericPlatformMisc.h"

#include "ObjectTools.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"

#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"

#include "K2Node.h"
#include "K2Node_Composite.h"
#include "K2Node_Variable.h"
//#include "K2Node_VariableSet.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
//#include "K2Node_SpawnActor.h"
//#include "K2Node_ConstructObjectFromClass.h"
//#include "K2Node_AddPinInterface.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialGraph/MaterialGraph.h"

#include "Engine/ObjectLibrary.h"
#include "Engine/Texture2D.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "UObject/UObjectThreadContext.h"

#include "Editor/UnrealEdEngine.h"
#include "Editor/EditorEngine.h"

extern UNREALED_API UUnrealEdEngine* GUnrealEd;
extern UNREALED_API UEditorEngine* GEditor;
//#include "LaunchEngineLoop.h"

DEFINE_LOG_CATEGORY_STATIC(LOG_DOT, Log, All);

UBlueprintDoxygenCommandlet::UBlueprintDoxygenCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, outputMode(OutputMode::none)
	, outputDir("-")
	, TotalGraphsProcessed(0)
	, TotalBlueprintsIgnored(0)
	, TotalMaterialsIgnored(0)
	, TotalNumFailedLoads(0)
	, TotalNumFatalIssues(0)
	, TotalNumWarnings(0)
{
	BlueprintBaseClassName = UBlueprint::StaticClass()->GetFName();
	MaterialBaseClassName = UMaterial::StaticClass()->GetFName();

	//IsClient = false;
	//IsEditor = false;
	//IsServer = false;
	//LogToConsole = true;

	BlueprintStyle.Empty();
	BlueprintStyle.Add("_STYLESHEET_", stylesheet);
	BlueprintStyle.Add("_GRAPHBG_", "transparent");
	//BlueprintStyle.Add("_GRAPHBG_", "#F1F1F1");		//can't do it this way.  Use css instead.
	BlueprintStyle.Add("_FONTNAME_", "Arial");
	//BlueprintStyle.Add("_FONTNAME_", "Helvetica");
	BlueprintStyle.Add("_FONTSIZE_", "10");				//for header/title
	BlueprintStyle.Add("_FONTSIZE2_", "9");				//for header/title2
	BlueprintStyle.Add("_FONTCOLOR_", "black");			//for header/title
	BlueprintStyle.Add("_FONTSIZEPORT_", "10");			//for pins
	BlueprintStyle.Add("_FONTSIZECOMMENT_", "18");		//comment size
	BlueprintStyle.Add("_FONTSIZEBUBBLE_", "11");		//bubble text size
	BlueprintStyle.Add("_FONTCOLORBUBBLE_", "#888888");	//bubble font color
	BlueprintStyle.Add("_NODECOLOR_", "#F8F9FA");
	BlueprintStyle.Add("_NODECOLORTRANS_", "#F8F9FAEE");
	BlueprintStyle.Add("_VALCOLOR_", "#888888");
	BlueprintStyle.Add("_BORDERCOLOR_", "#999999");
	BlueprintStyle.Add("_EDGECOLOR_", "#444444");
	BlueprintStyle.Add("_EDGETHICKNESS_", "2");
	BlueprintStyle.Add("_PINDEFAULTCOLOR_", "#444444");
	BlueprintStyle.Add("_PINURL_", "");
	BlueprintStyle.Add("_CELLPADDING_", "3");
	BlueprintStyle.Add("_COMMENTPADDING_", "7");
	BlueprintStyle.Add("_COMPOSITEPADDING_", "5");
	BlueprintStyle.Add("_COMPACTCOLOR_", "#888888");

	BlueprintStyle.Add("_HEIGHTSPACER_", "<font color=\"transparent\" point-size=\"12\">|</font>");
}

int32 UBlueprintDoxygenCommandlet::Main(const FString& Params)
{
	InitCommandLine(Params);

	if (outputMode == OutputMode::none)
	{
		UE_LOG(LOG_DOT, Error, TEXT("Usage flags:"));
		UE_LOG(LOG_DOT, Error, TEXT("	\"-OutputMode=verbose|debug|doxygen\" (where doxygen produces c++ for doxygen parsing)"));
		UE_LOG(LOG_DOT, Error, TEXT("	\"-OutputDir=-|[path]\" (where - means stdout, which is the default. The directory will not be cleared first.)"));
		UE_LOG(LOG_DOT, Error, TEXT("	\"-Stylesheet=[url]\" (defaults to 'doxygen-pixo.css')"));
		UE_LOG(LOG_DOT, Error, TEXT("	\"-Groups=[filename.dox]\" (defaults to 'groups.dox')"));
		return 1;
	}

	if (!ClearGroups())
		return 1;

	BuildAssetLists();
	ReportBlueprints();
	//ReportMaterials();

	LogResults();

	return (TotalNumFatalIssues + TotalNumFailedLoads);
}

/*
void UBlueprintDoxygenCommandlet::CreateCustomEngine(const FString& Params)
{
	printf("GONNA START ENGINE\n");
	UClass* EngineClass = nullptr;
	FString UnrealEdEngineClassName;
	GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("UnrealEdEngine"), UnrealEdEngineClassName, GEngineIni);
	EngineClass = StaticLoadClass(UUnrealEdEngine::StaticClass(), nullptr, *UnrealEdEngineClassName);
	wcout << "GIsEditor: " << GIsEditor << "Engine class: " << *UnrealEdEngineClassName << endl;
	if (EngineClass == nullptr)
	{
		UE_LOG(LogInit, Fatal, TEXT("Failed to load UnrealEd Engine class '%s'."), *UnrealEdEngineClassName);
	}
	//GEngine=GEditor=GUnrealEd=NewObject<UUnrealEdEngine>(GetTransientPackage(), EngineClass);
	GEngine=NewObject<UUnrealEdEngine>(GetTransientPackage(), EngineClass);

	//FEngineLoop::Init();
	//FEngineLoop::AppInit();
	printf("STARTED ENGINE\n");
}
*/

void UBlueprintDoxygenCommandlet::InitCommandLine(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> SwitchParams;
	ParseCommandLine(*Params, Tokens, Switches, SwitchParams);

	IgnoreFolders.Add("/Engine");
	IgnoreFolders.Add("/Game");

	//IgnoreFolders.Add("/PixoBlueprintDocumentation");	//ignore our own plugin when not debugging

	//TODO: detect other (unwanted plugins, or specify a specific plugin)
	//Add an include folder instead of an exclude?
	IgnoreFolders.Add("/Bridge");				//plugin
	IgnoreFolders.Add("/SpeedtreeImporter");	//plugin
	IgnoreFolders.Add("/ResonanceAudio");		//plugin
	IgnoreFolders.Add("/MediaCompositing");		//plugin
	IgnoreFolders.Add("/AnimationSharing");		//plugin

	//if (SwitchParams.Contains(TEXT("BlueprintBaseClass")))
	//	BlueprintBaseClassName = *SwitchParams[TEXT("BlueprintBaseClass")];

	//if (SwitchParams.Contains(TEXT("MaterialBaseClass")))
	//	MaterialBaseClassName = *SwitchParams[TEXT("MaterialBaseClass")];

	if (SwitchParams.Contains(TEXT("Stylesheet")))
		stylesheet = *SwitchParams[TEXT("Stylesheet")];

	if (SwitchParams.Contains(TEXT("OutputDir")))
	{	outputDir = *SwitchParams[TEXT("OutputDir")];
		outputDir.TrimStartAndEndInline();			//Sorry if you want a directory ending with a space!
		outputDir.RemoveFromEnd("/");				//remove dir slash if present
		outputDir.RemoveFromEnd("\\");				//remove dir slash if present
		//printf("output dir: %s\n", TCHAR_TO_UTF8(*outputDir));
	}

	if (SwitchParams.Contains(TEXT("Groups")))
		_groups = *SwitchParams[TEXT("Groups")];

	//outputDir = "-";			//comment this line when not debugging
	if (outputDir == "-")
		out = &std::wcout;

	if (SwitchParams.Contains(TEXT("OutputMode")))
	{
		outputMode = OutputMode::none;		//reset

		FString m = SwitchParams[TEXT("OutputMode")];
		OutputMode e = OutputMode_e.FindOrAdd(m,OutputMode::none);

		switch (e)
		{
		case OutputMode::debug:
			outputMode |= OutputMode::debug;
			break;

		case OutputMode::doxygen:
			outputMode |= OutputMode::doxygen;
			break;

		case OutputMode::verbose:
			outputMode |= OutputMode::verbose;
			outputMode |= OutputMode::debug;
			break;

		default:
			UE_LOG(LOG_DOT, Warning, TEXT("Unknown OutputMode: '%s'..."), *m);
			break;
		}

		//printf("output mode %x\n", outputMode);
	}
	else
		UE_LOG(LOG_DOT, Error, TEXT("No OutputMode specified."));
}

void UBlueprintDoxygenCommandlet::BuildAssetLists()
{
	BlueprintAssetList.Empty();
	MaterialAssetList.Empty();

	if (outputMode & OutputMode::verbose)
		UE_LOG(LOG_DOT, Warning, TEXT("Loading Asset Registry..."));

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
	AssetRegistryModule.Get().SearchAllAssets(/*bSynchronousSearch =*/true);
	//UE_LOG(LOG_DOT, Display, TEXT("Finished Loading Asset Registry."));

	//========

	AssetRegistryModule.Get().GetAssetsByClass(BlueprintBaseClassName, BlueprintAssetList, true);
	//AssetRegistryModule.Get().GetAllAssets(BlueprintAssetList, true);

	if (outputMode & OutputMode::verbose)
		UE_LOG(LOG_DOT, Warning, TEXT("Found %d blueprints in asset registry."), BlueprintAssetList.Num());

	//if (outputMode & OutputMode::doxygen)
	//	wcout << "// Found " << BlueprintAssetList.Num() << " blueprint assets in library." << endl;

	//========

	AssetRegistryModule.Get().GetAssetsByClass(MaterialBaseClassName, MaterialAssetList, true);

	if (outputMode & OutputMode::verbose)
		UE_LOG(LOG_DOT, Warning, TEXT("Found %d materials in asset registry."), MaterialAssetList.Num());

	//if (outputMode & OutputMode::doxygen)
	//	wcout << "// Found " << MaterialAssetList.Num() << " material assets in library." << endl;
}

bool UBlueprintDoxygenCommandlet::ShouldReportAsset(FAssetData const& Asset) const
{
	bool bShouldReport = true;

	if (IgnoreFolders.Num() > 0)
	{
		for (const FString& IgnoreFolder : IgnoreFolders)
		{
			if (Asset.ObjectPath.ToString().StartsWith(IgnoreFolder))
			{
				//FString const AssetPath = Asset.ObjectPath.ToString();
				//UE_LOG(LOG_DOT, Display, TEXT("Ignoring %s"), *AssetPath);
				FString const PackagePath = Asset.ObjectPath.ToString();
				if (outputMode & verbose)
					UE_LOG(LOG_DOT, Warning, TEXT("Ignoring %s"), *PackagePath);

				bShouldReport = false;
			}
		}
	}

	return bShouldReport;
}

bool UBlueprintDoxygenCommandlet::ShouldReportObject(UObject* object) const
{
	bool bShouldReport = true;
	FString path = object->GetPathName();

	//wcout << "SRO path: " << *path << endl;

	for (const FString& IgnoreFolder : IgnoreFolders)
	{
		if (path.StartsWith(IgnoreFolder))
			bShouldReport = false;
	}

	return bShouldReport;
}

bool UBlueprintDoxygenCommandlet::ClearGroups()
{
	//only in doxygen mode
	if (!(outputMode & OutputMode::doxygen))
		return true;

	if (outputDir == "-")
		return true;

	GroupList.Empty();

	FString fpath = outputDir + "/" + _groups;
	FPaths::MakePlatformFilename(fpath);

	std::ofstream::openmode mode = std::ofstream::out | std::ofstream::trunc;
	std::wofstream stream;

	stream.open(*fpath, mode);

	if (stream.is_open())
	{
		//UE_LOG(LOG_DOT, Warning, TEXT("Clearing '%s'..."), *fpath);
		//wcout << "Clearing " << *fpath << endl;
		stream.close();
	}
	else
	{
		UE_LOG(LOG_DOT, Error, TEXT("Could not open '%s' for writing."), *fpath);
		//wcerr << "Error opening file: '" << *fpath << "'" << endl;
		return false;
	}

	return true;
}

void UBlueprintDoxygenCommandlet::ReportGroup(FString groupName, FString groupNamePretty, FString brief, FString details)
{
	//only in doxygen mode
	if (!(outputMode & OutputMode::doxygen))
		return;

	if (GroupList.Contains(groupName))
		return;

	GroupList.Add(groupName);

	FString tmpl(R"PixoVR(/**
	\defgroup {0} {1}
	\brief {2}

{3}
{4}*/)PixoVR");

	FString gallery = "";

	if (GalleryList.Num())
	{
		FString galleryItems;

		GalleryList.Sort();		//alphabetize the gallery
		for (FString e : GalleryList)
		{	galleryItems += "	" + e + "\n";	}

		FString gtmpl(R"PixoVR(
	<h2 class='groupheader'>Gallery</h2>
	<div class='gallery'>
{0}	</div>
)PixoVR");

		gallery = FString::Format(*gtmpl, { galleryItems });
	}
	GalleryList.Empty();		//reset the gallery

	FString groupdata = FString::Format(*tmpl, { groupName, groupNamePretty, brief, details, gallery });

	FString path = outputDir + "/" + _groups;
	FPaths::MakePlatformFilename(path);
	if (!OpenFile(path,true))
	{
		UE_LOG(LOG_DOT, Error, TEXT("Could not open '%s' for writing."), *path);
		return;
	}

	*out << *groupdata << endl;

	CloseFile();
}

void UBlueprintDoxygenCommandlet::ReportBlueprints()
{
	for (FAssetData const& Asset : BlueprintAssetList)
	{
		if (ShouldReportAsset(Asset))
		{
			FString const AssetPath = Asset.ObjectPath.ToString();
			FString const AssetName = Asset.AssetName.ToString();
			FString const PackagePath = Asset.PackagePath.ToString();

			if (outputMode & verbose)
			{
				UE_LOG(LOG_DOT, Warning, TEXT("Loading Blueprint Asset:   '%s'..."), *AssetPath);
				UE_LOG(LOG_DOT, Warning, TEXT("Loading Package: '%s'..."), *PackagePath);
				UE_LOG(LOG_DOT, Warning, TEXT("Asset name: '%s'..."), *AssetName);
			}

			//Load with LOAD_NoWarn and LOAD_DisableCompileOnLoad as we are covering those explicitly with CompileBlueprint errors.
			UBlueprint* LoadedBlueprint = Cast<UBlueprint>(StaticLoadObject(Asset.GetClass(), /*Outer =*/nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_DisableCompileOnLoad));
			if (LoadedBlueprint == nullptr)
			{
				++TotalNumFailedLoads;
				if (outputMode & verbose)
				{	UE_LOG(LOG_DOT, Error, TEXT("%sFailed to Load : '%s'."), *_tab, *AssetPath);	}
				else
					wcerr << "Failed to load : " << *AssetPath << endl;

				continue;
			}
			else
			{
				ReportBlueprint(_tab, LoadedBlueprint);
			}
		}
		else
			TotalBlueprintsIgnored++;
	}

	ReportGroup(
		"blueprints",
		"Blueprints",
		"Blueprints found in the **$(PROJECT_NAME)** library.",
		"	The blueprint system may contain multiple graph classes(Event Graph, Construction Graph, etc)\n"
		"	per blueprint, and each graph may contain subgraphs.  Links to subgraphs can be accessed by\n"
		"	clicking on their collapsed node or by navigating to the additional entries in the class."
	);
}

void UBlueprintDoxygenCommandlet::ReportMaterials()
{
	for (FAssetData const& Asset : MaterialAssetList)
	{
		if (ShouldReportAsset(Asset))
		{
			FString const AssetPath = Asset.ObjectPath.ToString();
			FString const AssetName = Asset.AssetName.ToString();
			FString const PackagePath = Asset.PackagePath.ToString();

			if (outputMode & verbose)
			{
				UE_LOG(LOG_DOT, Warning, TEXT("Loading Material Asset:   '%s'..."), *AssetPath);
				UE_LOG(LOG_DOT, Warning, TEXT("Loading Package: '%s'..."), *PackagePath);
				UE_LOG(LOG_DOT, Warning, TEXT("Asset name: '%s'..."), *AssetName);
			}

			//Load with LOAD_NoWarn and LOAD_DisableCompileOnLoad as we are covering those explicitly with CompileBlueprint errors.
			UMaterial * LoadedMaterial = Cast<UMaterial>(StaticLoadObject(Asset.GetClass(), /*Outer =*/nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_DisableCompileOnLoad));
			if (LoadedMaterial == nullptr)
			{
				++TotalNumFailedLoads;
				if (outputMode & verbose)
				{	UE_LOG(LOG_DOT, Error, TEXT("%sFailed to Load : '%s'."), *_tab, *AssetPath);	}
				else
					wcerr << "Failed to load : " << *AssetPath << endl;

				continue;
			}
			else
			{
				ReportGroup(
					"materials",	//groupname
					"Materials",	//cosmetic groupname
					"Materials found in the **$(PROJECT_NAME)** library.",		//group brief
					"	The material system allows for subgraphs within a material node network.\n"
					"	Links to subgraphs can be accessed by clicking on their collapsed node or by navigating to the\n"
					"	additional entries in the class."
				);
				ReportMaterial(_tab, AssetName, LoadedMaterial);
			}
		}
		else
			TotalMaterialsIgnored++;
	}
}

void UBlueprintDoxygenCommandlet::ReportBlueprint(FString prefix, UBlueprint* blueprint)
{
	const UPackage* package = blueprint->GetPackage();
	FString className = GetClassName(blueprint->GeneratedClass);
	FString path, subDir;

	//TODO is this useful?
	bool isLevelScriptBlueprint = FBlueprintEditorUtils::IsLevelScriptBlueprint(blueprint);
	//bool isParentClassABlueprint = FBlueprintEditorUtils::IsParentClassABlueprint(blueprint);
	
	//each blueprint will have different graphs.  This is just to save some memory on a large build.
	GraphDescriptions.Empty();

	//each graph will make calls to nodes and variables.  Clear these before we report each blueprint.
	GraphCalls.Empty();

	if (outputMode & verbose)
	{	
		const UClass* parent = blueprint->ParentClass;
		FString description = "This class " + blueprint->GetDesc() + ".\n\n" + prefix + blueprint->BlueprintDescription;
		description = description.TrimStartAndEnd();

		UE_LOG(LOG_DOT, Display, TEXT("%sParsing: %s"),	*prefix, *blueprint->GetPathName());
		UE_LOG(LOG_DOT, Display, TEXT("%sPackage: %s"), *prefix, *package->GetLoadedPath().GetLocalFullPath());
		UE_LOG(LOG_DOT, Display, TEXT("%sName: %s"), *prefix, *package->GetName());
		UE_LOG(LOG_DOT, Display, TEXT("%sExtends: %s"), *prefix, *(parent->GetPrefixCPP()+parent->GetName()));
		UE_LOG(LOG_DOT, Display, TEXT("%sDescription: %s"), *prefix, *(description.Replace(TEXT("\\n"),TEXT(" || "))));
	}

	if (outputMode & doxygen)
	if (outputDir != "-")
	{
		IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
		subDir = package->GetName();				//get the full UDF path
		subDir = FPaths::GetPath(subDir);			//chop the "file" entry off
		subDir.RemoveFromStart("/");				//remove prefix slash from UFS path
		currentDir = outputDir + "/" + subDir;		//jam it all together
		FPaths::NormalizeDirectoryName(currentDir);	//fix/normalize the slashes

		// Directory Exists?
		//FPaths::DirectoryExists(dir);
		if (!platformFile.DirectoryExists(*currentDir))
		{
			if (!platformFile.CreateDirectoryTree(*currentDir))
			{
				UE_LOG(LOG_DOT, Error, TEXT("Could not create folder: %s"),*currentDir);
				return;
			}
		}

		path = currentDir + "/" + className + ".h";
		FPaths::MakePlatformFilename(path);				//clean slashes
		if (!OpenFile(path))
		{
			UE_LOG(LOG_DOT, Error, TEXT("Could not open '%s' for writing."), *path);
			return;
		}
	}

	TArray<UEdGraph*> graphs;
	blueprint->GetAllGraphs(graphs);

	if (outputMode & verbose)
	{	UE_LOG(LOG_DOT, Display, TEXT("%sFound %d graph%s..."), *prefix, graphs.Num(), graphs.Num() > 1 ? "s" : "");	}

	if (outputMode & doxygen)
	{
		writeBlueprintHeader(blueprint, "blueprints", "Blueprint", package->GetName(), graphs.Num());
		writeAssetMembers(blueprint, "Blueprint");

		if (graphs.Num())
		{	
//			*out << "	/**" << endl;
//			*out << "	\\name Blueprint Graphs" << endl;
//			//*out << "	\\privatesection" << endl;
//			*out << "	*/" << endl;
//			*out << "	///@{" << endl;
		}
	}

	for (UEdGraph* g : graphs)
		ReportGraph(prefix, g);

	if (outputMode & doxygen)
	if (outputDir != "-")
	{
		if (graphs.Num())
		{
//			*out << "	///@}" << endl;
		}

		writeAssetFooter();

		CloseFile();		//close .h file

		//TODO: make/replicate the path to this file under the outputDir, rather than a flat folder.

		path = currentDir + "/" + className + ".cpp";
		FPaths::MakePlatformFilename(path);				//clean slashes
		if (!OpenFile(path))
		{
			UE_LOG(LOG_DOT, Error, TEXT("Could not open '%s' for writing."), *path);
			return;
		}

		writeAssetCalls();

		CloseFile();		//close .cpp file
	}
}

void UBlueprintDoxygenCommandlet::ReportMaterial(FString prefix, FString assetName, UMaterial* material)
{
	//TODO: needs a cleanup!

	//UE_LOG(LOG_DOT, Display, TEXT("%sParsing: %s"),	*prefix, *Blueprint->GetPathName());

	const UPackage* package = material->GetPackage();
	FString parentClass = material->GetFName().ToString();
	//FString parentClass = UMaterial::StaticClass()->GetFName().ToString();

	UMaterial* baseMaterial = material->GetBaseMaterial();			// we're kind of assuming that there's not many (any) parent classes above this.
	//FString parentClass = baseMaterial->GetFName().ToString();

	//if (!assetName.EndsWith(TEXT("_C")))							// TODO: is this needed?
	//	assetName += "_C";

	FString description = material->GetDesc();						// will probably be basically empty or useless
	description += "\n" + material->GetDetailedInfo();
	description = description.Replace(TEXT("\r"), TEXT(""));
	description.TrimStartAndEndInline();

	//TODO:
	//material->ThumbnailInfo

	//each material will have different graphs.  This is just to save some memory on a large build.
	GraphDescriptions.Empty();

	if (outputMode & debug)
	{
		UE_LOG(LOG_DOT, Display, TEXT("%sPackage: %s"), *prefix, *package->GetLoadedPath().GetLocalFullPath());
		UE_LOG(LOG_DOT, Display, TEXT("%sName: %s"), *prefix, *package->GetName());
		UE_LOG(LOG_DOT, Display, TEXT("%sExtends: %s"), *prefix, *parentClass);
		UE_LOG(LOG_DOT, Display, TEXT("%sDescription: %s"), *prefix, *description);
	}

	if (outputMode & doxygen)
	if (outputDir != "-")
	{
		//TODO: make/replicate the path to this file under the outputDir, rather than a flat folder.

		//FString path = outputDir + FGenericPlatformMisc::GetDefaultPathSeparator() + assetName + ".cpp";
		FString path = outputDir + "/" + assetName + ".h";
		if (!OpenFile(path))
		{
			UE_LOG(LOG_DOT, Error, TEXT("Could not open '%s' for writing."), *path);
			return;
		}
	}

	TArray<UEdGraph*> graphs;
	UEdGraph* graph = material->MaterialGraph;

	if (!graph)
	{
		UE_LOG(LOG_DOT, Error, TEXT("No Material Graph!") );
		//return;
	}
	else
	{
		graphs.Add(graph);
		addAllGraphs(graphs, graph->SubGraphs);
	}

	wcout << *assetName << " has " << graphs.Num() << " graphs in it." << endl;

	TArray< UMaterialExpression*> expressions;
	material->GetAllReferencedExpressions(expressions, 0);
	//expression->GetAllInputExpressions

	wcout << *assetName << " has " << expressions.Num() << " expressions in it." << endl;

	for (UMaterialExpression* expression : expressions)
	{
		wcout << *expression->GetName() << " :: " << *expression->Desc << endl;
		//expression->GraphNode;
	}

	//TODO: get comments!

	if (outputMode & verbose)
		UE_LOG(LOG_DOT, Display, TEXT("%sFound %d graph%s..."), *prefix, graphs.Num(), graphs.Num() != 1 ? "s" : "");

	//EMaterialProperty::
	//FString graph->OriginalMaterialFullName
	//TODO MaterialInputs !! should list as parameters of this class
	printf("TODO: LIST MATERIAL INPUTS HERE!\n");

	if (outputMode & doxygen)
	{
		writeMaterialHeader(material, "materials", "Material", assetName, package->GetName(), description, parentClass, graphs.Num());
		//writeAssetMembers(material, "Material");

		if (graphs.Num())
		{
			*out << "	/**" << endl;
			*out << "	\\name Material Graphs" << endl;
			*out << "	*/" << endl;
			*out << "	///@{" << endl;
		}
	}

	for (UEdGraph* g : graphs)
		ReportGraph(prefix, g);

	if (outputMode & doxygen)
	{
		if (graphs.Num())
		{
			*out << "	///@}" << endl;
		}

		writeAssetFooter();
	}

	CloseFile();				//will close if open, or do nothing.
}

void UBlueprintDoxygenCommandlet::addAllGraphs(TArray<UEdGraph*>& container, TArray<UEdGraph*>& graphs)
{
	for (UEdGraph* g : graphs)
	{
		container.Add(g);
		addAllGraphs(container, g->SubGraphs);
	}
}

void UBlueprintDoxygenCommandlet::ReportGraph(FString prefix, UEdGraph* g)
{
	PinConnections.Empty();		//clear out all connections for each new graph

	if (outputMode & debug)
	{
		FString graphName;
		g->GetName(graphName);

		FString graphNameVariable = createVariableName(graphName, false);
		FString graphNameHuman = FName::NameToDisplayString(graphName, false);
		FString description = g->GetDetailedInfo();				//basically never useful

		//FString parent = g->GetClass()->GetSuperClass()->GetFName().ToString();
		FString parent = g->GetClass()->GetFName().ToString();

		UE_LOG(LOG_DOT, Display, TEXT("%s  Parsing '%s' (%s)..."), *prefix, *(graphName), *(g->GraphGuid.ToString()));
		UE_LOG(LOG_DOT, Display, TEXT("%s    (variable) '%s'..."), *prefix, *graphNameVariable);
		UE_LOG(LOG_DOT, Display, TEXT("%s    (human) '%s'..."), *prefix, *graphNameHuman);
		UE_LOG(LOG_DOT, Display, TEXT("%s  Detail: %s"), *prefix, *description);
		UE_LOG(LOG_DOT, Display, TEXT("%s  Extends %s"), *prefix, *parent);
		UE_LOG(LOG_DOT, Display, TEXT("%s  Found %d nodes..."), *prefix, g->Nodes.Num());
	}

	if (outputMode & doxygen)
		writeGraphHeader(prefix, g, "Blueprint");

	for (UEdGraphNode* n : g->Nodes)
	{
		ReportNode(prefix + _tab, n);
	}

	if (outputMode & doxygen)
	{	writeGraphConnections(prefix);
		writeGraphFooter(prefix, g);
	}

	//TODO: local variables for a function (graph)
	// if n is UK2Node_Function (Entry, etc)
	//find local variables
	// FBlueprintEditorUtils::FindLocalVariable
	// FBlueprintEditorUtils::GetLocalVariablesOfType
	//	casts to UK2Node_FunctionEntry (node)
	//  node->LocalVariables
	//  //iterate from there
	//g->loca

	TotalGraphsProcessed++;
}

void UBlueprintDoxygenCommandlet::ReportNode(FString prefix, UEdGraphNode* n)
{
	//FString type = n->GetSchema
	FString type = n->GetClass()->GetFName().ToString();

	//add the subgraph node's description here, for when we are parsing that graph
	UK2Node_Composite* subgraph_node = dynamic_cast<UK2Node_Composite*>(n);
	//if (type == "K2Node_Composite")
	if (subgraph_node)
	{
		UEdGraph *subgraph = subgraph_node->BoundGraph;
		FString brief = subgraph_node->GetTooltipText().ToString().TrimStartAndEnd();
		GraphDescriptions.Add(subgraph, brief);
	}
	
	if (outputMode & debug)
	{
		FLinearColor titleColor = n->GetNodeTitleColor();

		UEdGraphNode_Comment* comment = dynamic_cast<UEdGraphNode_Comment*>(n);
		if (comment)
			titleColor = comment->CommentColor;// .ToString();

		FString prefix2 = prefix + _tab;

		FString nn, ng;
		n->GetName(nn);
		ng = n->NodeGuid.ToString();

		FString title = n->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		//FString title = n->GetNodeTitle(ENodeTitleType::ListView).ToString();
		title = title.Replace(TEXT("\r"), TEXT(""));
		title = title.Replace(TEXT("\n"), TEXT(" || "));

		//UE_LOG(LOG_DOT, Display, TEXT("%s%s (%s)"), *prefix, *nn, *ng);
		UE_LOG(LOG_DOT, Display, TEXT("%s%s"), *prefix, *nn);
		UE_LOG(LOG_DOT, Display, TEXT("%stitle: '%s'"), *prefix2, *title);
		UE_LOG(LOG_DOT, Display, TEXT("%stitle color: %s"), *prefix2, *titleColor.ToString());
		UE_LOG(LOG_DOT, Display, TEXT("%stype: %s"), *prefix2, *type);
		UE_LOG(LOG_DOT, Display, TEXT("%sposition: (%d,%d)"), *prefix2, n->NodePosX, n->NodePosY);
		UE_LOG(LOG_DOT, Display, TEXT("%ssize: [%d,%d]"), *prefix2, n->NodeWidth, n->NodeHeight);
		UE_LOG(LOG_DOT, Display, TEXT("%scolor: %s"), *prefix2, *n->GetNodeBodyTintColor().ToString());

		FString tt = n->GetTooltipText().ToString();
		tt = tt.Replace(TEXT("\r"), TEXT(""));
		tt = tt.Replace(TEXT("\n"), TEXT(" || "));
		tt.LeftInline(50);		//trim while debugging...
		UE_LOG(LOG_DOT, Display, TEXT("%stooltip: %s"), *prefix2, *tt);

		//FString d = n->GetDocumentationLink().Replace(TEXT("\n"), TEXT(" || "));
		//UE_LOG(LOG_DOT, Display, TEXT("%sdocumentation: %s"), *prefix2, *d);
		//FString de = n->GetDocumentationExcerptName().Replace(TEXT("\n"), TEXT(" || "));
		//UE_LOG(LOG_DOT, Display, TEXT("%sdoc exerpt: %s"), *prefix2, *de);

		if (n->bCommentBubbleVisible)
		{
			FString c = n->NodeComment;
			c = c.Replace(TEXT("\r"), TEXT(""));
			c = c.Replace(TEXT("\n"), TEXT(" || "));
			UE_LOG(LOG_DOT, Display, TEXT("%scomment: %s"), *prefix2, *c);
			UE_LOG(LOG_DOT, Display, TEXT("%scomment color: %s"), *prefix2, *n->GetNodeCommentColor().ToString());
		}

		const TArray< UEdGraphPin* > pins = n->Pins;
		//const TArray< UEdGraphPin* > pins = n->GetAllPins();

		if (pins.Num() > 0)
		{
			if (outputMode & debug)
			{
				UE_LOG(LOG_DOT, Display, TEXT("%spins (%d):"), *prefix2, pins.Num());
			}

			for (UEdGraphPin* p : pins)
			{
				ReportPin(prefix2 + _tab, p);

				//if (n->CanSplitPin(p))
				//	ReportSplittablePin(p, prefix2 + _tab);
			}
		}
	}

	if (outputMode & doxygen)
		writeNodeBody(prefix, n);
}

void UBlueprintDoxygenCommandlet::ReportPin(FString prefix, UEdGraphPin* p)
{
	//don't show hidden pins
	if (!PinShouldBeVisible(p))
		return;

	FString flabel = GetPinLabel(p);
	FString type = GetPinType(p);
	FString tooltip = GetPinTooltip(p);
	FString dir = (p->Direction == EEdGraphPinDirection::EGPD_Input) ? TEXT("<===") : TEXT("===>");
	//FString pid = p->PinId.ToString();

	if (p->HasAnyConnections())
	{
		for (UEdGraphPin* d : p->LinkedTo)
		{
			FString tlabel = GetPinLabel(d);

			if (outputMode & debug)
			{
				UE_LOG(LOG_DOT, Display,
					TEXT("%s|%15s %s %-15s| {%8s} [%s]"),
					*prefix,
					*flabel,
					*dir,
					*tlabel,
					*type,
					*tooltip
				);
			}
		}
	}
	else
	{
		FString dval = p->DefaultValue;
		//UObject *dobj = p->DefaultObject;

		FString tlabel = dval.IsEmpty() ? "(*:NULL)" : "(*:'"+dval+"')";

		if (outputMode & debug)
		{
			UE_LOG(LOG_DOT, Display,
				TEXT("%s|%15s %s %-15s| {%8s} [%s]"),
				*prefix,
				*flabel,
				*dir,
				*tlabel,
				*type,
				*tooltip
			);
		}
	}
}

/*
//TODO: delete this
void UBlueprintDoxygenCommandlet::ReportSplittablePin(UEdGraphPin* p, FString prefix)
{
	//don't show hidden pins
	if (p->bHidden)
		return;

	FString flabel = GetPinLabel(p);
	FString type = GetPinType(p);
	FString tooltip = GetPinTooltip(p);
	FString dir = (p->Direction == EEdGraphPinDirection::EGPD_Input) ? TEXT("<===") : TEXT("===>");
	//FString pid = p->PinId.ToString();

	FString tlabel = "(+)";

	if (outputMode & debug)
	{
		UE_LOG(LOG_DOT, Display,
			TEXT("%s|%15s %s %-15s| {%8s} [%s]"),
			*prefix,
			*flabel,
			*dir,
			*tlabel,
			*type,
			*tooltip
		);
	}
}
*/

bool UBlueprintDoxygenCommandlet::OpenFile(FString fpath, bool append)
{
	CloseFile();

	std::ofstream::openmode mode = std::ofstream::out;
	if (append)
		mode |= std::ofstream::app;

	std::wofstream* stream = new std::wofstream();
	stream->open(*fpath, mode);
	if (stream->is_open())
	{
		out = outfile = stream;

		FString tpath = fpath;
		tpath.RemoveFromStart(outputDir);
		tpath = "[OutputDir]" + tpath;

		//wcout << "Writing to " << *fpath << endl;
		wcout << "Writing to " << *tpath << endl;
	}
	else
	{
		out = &std::wcout;
		wcerr << "Error opening file: '" << *fpath << "'" << endl;
		return false;
	}

	return true;
}

bool UBlueprintDoxygenCommandlet::CloseFile()
{
	if (outfile)
	{
		if (outfile->is_open())
			outfile->close();

		delete outfile;
		outfile = NULL;

		return true;
	}

	return false;
}

FString UBlueprintDoxygenCommandlet::GetTrimmedConfigFilePath(FString path)
{
	//TODO: chop the head of this path off to be proper
	FString confdir = FPaths::ProjectConfigDir();
	confdir.RemoveFromEnd("/");
	confdir = FPaths::GetPath(confdir)+"/";

	FPaths::NormalizeDirectoryName(confdir);
	FPaths::NormalizeFilename(path);

	FString trimmed = path;
	trimmed.RemoveFromStart(confdir);
	trimmed.RemoveFromStart("/");

	//wcout << "TRIMMED: " << *trimmed << " || " << *confdir << " || " << *path << endl;

	return trimmed;
}

// straight up copied from SGraphNode.cpp
bool UBlueprintDoxygenCommandlet::PinShouldBeVisible(UEdGraphPin* InPin)
{
	bool bHideNoConnectionPins = false;
	bool bHideNoConnectionNoDefaultPins = false;

	// Not allowed to hide exec pins 
	const bool bCanHidePin = (InPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec);

	//if (OwnerGraphPanelPtr.IsValid() && bCanHidePin)
	//if (true && bCanHidePin)
	//{
		//bHideNoConnectionPins = OwnerGraphPanelPtr.Pin()->GetPinVisibility() == SGraphEditor::Pin_HideNoConnection;
		//bHideNoConnectionNoDefaultPins = OwnerGraphPanelPtr.Pin()->GetPinVisibility() == SGraphEditor::Pin_HideNoConnectionNoDefault;
	//}

	const bool bIsOutputPin = InPin->Direction == EGPD_Output;
	const bool bPinHasDefaultValue = !InPin->DefaultValue.IsEmpty() || (InPin->DefaultObject != NULL);
	const bool bIsSelfTarget = (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object) && (InPin->PinName == UEdGraphSchema_K2::PN_Self);
	const bool bPinHasValidDefault = !bIsOutputPin && (bPinHasDefaultValue || bIsSelfTarget);
	const bool bPinHasConections = InPin->LinkedTo.Num() > 0;

	const bool bPinDesiresToBeHidden = 
		   InPin->bHidden
		|| (bHideNoConnectionPins && !bPinHasConections) 
		|| (bHideNoConnectionNoDefaultPins && !bPinHasConections && !bPinHasValidDefault);

	// No matter how strong the desire, a pin with connections can never be hidden!
	const bool bShowPin = !bPinDesiresToBeHidden || bPinHasConections;

	if (InPin->bAdvancedView && InPin->GetOwningNode()->AdvancedPinDisplay == ENodeAdvancedPins::Hidden)
		return false;

	return bShowPin;
}

FString UBlueprintDoxygenCommandlet::GetPinLabel(UEdGraphPin* p)
{
	FString n;

	//FString type = GetPinType(p);
	//if (type == "Exec")	return "";

	n = p->GetOwningNode()->GetPinDisplayName(p).ToString();

	if (n.IsEmpty())	n = p->GetDisplayName().ToString();
	if (n.IsEmpty())	n = p->PinFriendlyName.ToString();
	if (n.IsEmpty())	n = p->PinName.ToString();
	if (n.IsEmpty())	n = p->GetName();

	if (n.ToLower() == "exec" || 
		n.ToLower() == "execute" ||
		n.ToLower() == "then")
		return " ";	//must be ' ' because of GetPinTooltip and prepNodePortRows.  <font></font> can't be empty!

	return n;
}

FString UBlueprintDoxygenCommandlet::GetPinTooltip(UEdGraphPin* p)
{
	UEdGraphNode* n = p->GetOwningNode();
	FString hover;
	n->GetPinHoverText(*p, hover);
	hover = hover.TrimStartAndEnd();
	hover = hover.Replace(TEXT("\r"), TEXT(""));
	hover = hover.Replace(TEXT("\n"), TEXT("&#013;"));
	hover = hover.Replace(TEXT("\\"), TEXT("\\\\"));
	return hover;
}

FString UBlueprintDoxygenCommandlet::GetPinType(UEdGraphPin* pin, bool useSchema)
{
	FEdGraphPinType y = pin->PinType;

	//FBlueprintEditorUtils::GetTypeForPin(*pin);

	//FString t = FString::Printf(
	//	TEXT("%s/%s"),
	//	*y.PinCategory.ToString(),
	//	*y.PinSubCategory.ToString()
	//);

	if (useSchema)
	{
		const UEdGraphSchema* Schema = pin->GetSchema();
		const UEdGraphSchema_K2* s = dynamic_cast<const UEdGraphSchema_K2*>(Schema);

		if (s)
		{
			//s->GetVariableTypeTree
			//s->GetAllMarks
			return s->GetCategoryText(pin->PinType.PinCategory).ToString();
		}
	}

	if (y.IsArray())
		return "array";

	if (y.IsMap())
		return "map";

	if (y.IsSet())
		return "set";

	if (y.IsContainer())			//no idea what a "container" is
		return "container";

	FString t = y.PinCategory.ToString();
	return FName::NameToDisplayString(t, false);
}

FString UBlueprintDoxygenCommandlet::GetPinPort(UEdGraphPin* p)
{
	UEdGraphNode* n = p->GetOwningNode();
	bool isRoute = GetNodeType(n, NodeType::node) == NodeType::route;

	if (isDelegatePin(p))	return "delegate";
	if (isRoute)			return "port";

	return "P_"+p->PinId.ToString();
}

FString UBlueprintDoxygenCommandlet::GetPinDefaultValue(UEdGraphPin* pin)
{
	FString v = pin->DefaultValue;

	//TODO: get proper default values!

	//FEdGraphPinType pt = pin->PinType;
	//FEdGraphPinType::StaticStruct;

	/*
	if (pin->DefaultObject)
	{
		FJsonSerializableKeyValueMap vmap;
		pin->DefaultObject->GetNativePropertyValues(vmap);

		wcout << "def: " << *GetPinLabel(pin) << endl;
		for (TTuple<FString, FString> vv : vmap)
		{
			wcout << "key: " << *vv.Key << " || " << *vv.Value << endl;
		}
	}
	*/

	//output is always hidden
	if (pin->Direction == EEdGraphPinDirection::EGPD_Output)
		return "";

	//bool
	if (v == "true")
		return "<font color=\"_VALCOLOR_\">&#9746;</font>";	//checkbox
	if (v == "false")
		return "<font color=\"_VALCOLOR_\">&#9744;</font>";	//checkbox

	//FString tmpl;
	//tmpl = "<table border=\"1\" cellpadding=\"1\" bgcolor=\"_VALCOLOR_\"><tr><td>%0.2f</td></tr></table>";

	if (FDefaultValueHelper::IsStringValidFloat(v))
	{
		float f;
		if (FDefaultValueHelper::ParseFloat(v, f))
			return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[%0.3f]</font>"), f);
		else
			//convert failed, not much else to do.
			return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[float]</font>"));
	}

	if (FDefaultValueHelper::IsStringValidInteger(v))
	{
		int i;
		if (FDefaultValueHelper::ParseInt(v, i))
			return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[%d]</font>"), i);
		else
			//convert failed, not much else to do.
			return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[int]</font>"));
	}

	FString cicon = "&#9724;";

	if (FDefaultValueHelper::IsStringValidLinearColor(v))
	{
		FLinearColor c;
		if (FDefaultValueHelper::ParseLinearColor(v, c))
		{
			return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *createColorString(c), *cicon);
			//return FString::Printf(TEXT("<font color=\"%0.2f %0.2f %0.2f\">%s</font>"), c.R, c.G, c.B, *cicon);
		}
		else
			//convert failed, not much else to do.
			return FString::Printf(TEXT("<font color=\"red\">%s</font>"),*cicon);
	}

	float r, g, b, a;
	if (swscanf_s(*v, TEXT("(R=%f,G=%f,B=%f,A=%f)"), &r, &g, &b, &a) == 4)
	{
		FLinearColor c(r, g, b, a);
		return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *createColorString(c), *cicon);
		//return FString::Printf(TEXT("<font color=\"%0.2f %0.2f %0.2f %0.2f\">%s</font>"), r, g, b, a, *cicon);
	}

	if (swscanf_s(*v, TEXT("(R=%f,G=%f,B=%f)"), &r, &g, &b) == 3)
	{
		FLinearColor c(r, g, b);
		return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *createColorString(c), *cicon);
		//return FString::Printf(TEXT("<font color=\"%0.2f %0.2f %0.2f\">%s</font>"), r, g, b, *cicon);
	}

	//pin->DefaultObject
	//FDefaultValueHelper::

	//TODO
	//rotator
	//vector
	//vector2d
	//vector4
	//Fcolor (already have FLinearColor)

	//default output
	v = v.TrimStartAndEnd();
	v = v.Replace(TEXT("\r"), TEXT(""));
	v = v.Replace(TEXT("\n"), TEXT("&#013;"));
	if (v.IsEmpty())
		return "";		//presumably a label or icon exists, so empty is ok.

	return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[%s]</font>"), *v);
}

void UBlueprintDoxygenCommandlet::LogResults()
{
	//if (outputMode & verbose)
	{
		//results output
		UE_LOG(LOG_DOT,
			Display,
			TEXT("\n\n"
				"======================================================================================\n"
				"Examined %d graphs(s).\n"
				"Ignored %d blueprints(s).\n"
				"Ignored %d materials(s).\n"
				"Report Completed with %d errors and %d warnings and %d assets that failed to load.\n"
				"======================================================================================\n"),
			TotalGraphsProcessed,
			TotalBlueprintsIgnored,
			TotalMaterialsIgnored,
			TotalNumFatalIssues,
			TotalNumWarnings,
			TotalNumFailedLoads);
	}

	//Assets with problems listing
	//if (outputMode & verbose)
	if (AssetsWithErrorsOrWarnings.Num() > 0)
	{
		UE_LOG(LOG_DOT, Warning, TEXT("\n===================================================================================\nAssets With Errors or Warnings:\n===================================================================================\n"));

		for (const FString& Asset : AssetsWithErrorsOrWarnings)
		{	UE_LOG(LOG_DOT, Warning, TEXT("%s"), *Asset);	}

		UE_LOG(LOG_DOT, Warning, TEXT("\n===================================================================================\nEnd of Asset List\n===================================================================================\n"));
	}
}


//////////////////
// Doxygen stuff
//////////////////

FString htmlentities(FString in)
{
	FString out;
	for (int i = 0; i < in.Len(); i++)
	{
		if (in[i] > 127)
			out += FString::Printf(TEXT("&#%d;"), (int)in[i]);
		else
			out += in[i];
	}

	return out;
}

//https://vocal.com/video/rgb-and-hsvhsihsl-color-space-conversion/

void RGBtoHSL(float R, float G, float B, float* h, float* s, float* l)
{
	float Cmax = fmax(R, fmax(G, B));
	float Cmin = fmin(R, fmin(G, B));
	float D = Cmax - Cmin;
	float epsilon = 0.0001;

	float H;
	if (D < epsilon)
		H = 0.0f;
	else if (Cmax == R)
		H = 60.0f * fmod(((G - B) / D), 6.0f);
	else if (Cmax == G)
		H = 60.0f * (2.0f + ((B - R) / D));
	else //if (Cmax == B)
		H = 60.0f * (4.0f + ((R - G) / D));

	float L = (Cmax + Cmin) / 2.0f;
	float S = (D < epsilon) ? 0.0f : D / (1-fabs(2.0f*L - 1.0f));

	*h = H;
	*s = S;
	*l = L;
}

void HSLtoRGB(float H, float S, float L, float* r, float* g, float* b)
{
	float c = (1.0f - fabs(2.0f * L - 1)) * S;
	float x = c * (1 - fabs(fmod(H/60.0f, 2.0f) - 1.0f));
	float m = L - c / 2.0f;

	float rp, gp, bp;
	if (H < 60.0f)
	{		rp = c;	gp = x; bp = 0;		}
	else if (H < 120.0f)
	{		rp = x;	gp = c; bp = 0;		}
	else if (H < 180.0f)
	{		rp = 0;	gp = c; bp = x;		}
	else if (H < 240.0f)
	{		rp = 0;	gp = x; bp = c;		}
	else if (H < 300.0f)
	{		rp = x;	gp = 0; bp = c;		}
	else
	{		rp = c;	gp = 0; bp = x;		}

	*r = rp + m;
	*g = gp + m;
	*b = bp + m;
}

FString UBlueprintDoxygenCommandlet::createColorString(FLinearColor color, float alpha, float exponent)
{
	//return "#"+color.ToFColor(true).ToHex();

	float R, G, B;
	float H, S, L;

	R = color.R;
	G = color.G;
	B = color.B;

//	printf("in: %0.2f %0.2f %0.2f ", R, G, B);

	RGBtoHSL(R, G, B, &H, &S, &L);

//	printf("hsl: %0.2f %0.2f %0.2f ", H, S, L);

	H = fmod(H + 180.0f, 360.0f);	// our hue rotation for the site

	//L = powf(L, 2.2f);			// to sRGB
	L = powf(L, 1/exponent);		// our gamma correct

	HSLtoRGB(H, S, L, &R, &G, &B);

	FLinearColor rgb(R, G, B, alpha);

//	printf("rgb: %0.2f %0.2f %0.2f ", rgb.R, rgb.G, rgb.B);
//	printf("\n");

	rgb = rgb.GetClamped();

	return "#"+rgb.ToFColor(true).ToHex();
}

FString UBlueprintDoxygenCommandlet::createVariableName(FString name, bool forceUnique)
{
	std::string g(TCHAR_TO_UTF8(*name));

	//std::replace_if(g.begin(), g.end(), isNotAlphaNum, ' ');
	for (auto& c : g)
		if (!isalnum(c)) c = '_';	//replace non-alnum with '_'

	FString variableName(g.c_str());

	if (forceUnique)
	if (ObjectNames.Contains(variableName))
	{
		int i = 0;
		FString sn;

		do
		{
			sn = FString::Printf(TEXT("%s_%d"),*variableName,i);
			i++;
		} while (ObjectNames.Contains(sn));

		variableName = sn;
		ObjectNames.Add(variableName);
	}

	return variableName;
}

bool UBlueprintDoxygenCommandlet::CreateThumbnailFile(UObject* object, FString pngPath)
{
	UPackage* package = object->GetPackage();
	FString fullName = object->GetFullName();

	//wcout << "FNAME: |" << *fullName << "|" << endl;

	FLinkerLoad* Linker = package->LinkerLoad;
	if (Linker->SerializeThumbnails(true))
	{
		if (Linker->LinkerRoot->HasThumbnailMap())
		{
			FThumbnailMap& LinkerThumbnails = Linker->LinkerRoot->AccessThumbnailMap();

			for (TMap<FName, FObjectThumbnail>::TIterator It(LinkerThumbnails); It; ++It)
			{
				FName& ObjectFullName = It.Key();
				FObjectThumbnail& Thumb = It.Value();

				int32 w = Thumb.GetImageWidth();
				int32 h = Thumb.GetImageHeight();
				int32 s = Thumb.GetUncompressedImageData().Num();

				//wcout << "RNAME: |" << *ObjectFullName.ToString() << "| " << w << "x" << h << " @ " << s << " bytes" << endl;

				if ( w && h && s )
				if (ObjectFullName.ToString() == fullName)
				{
					IImageWrapperModule& ImageWrapperModule = FModuleManager::Get().LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
					TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
					ImageWrapper->SetRaw(Thumb.GetUncompressedImageData().GetData(), Thumb.GetUncompressedImageData().Num(), w, h, ERGBFormat::BGRA, 8);

					if (ImageWrapper)
					{
						FString tpngPath = pngPath;
						tpngPath.RemoveFromStart(outputDir);
						tpngPath = "[OutputDir]" + tpngPath;

						wcout << "Writing to " << *tpngPath << endl;

						const TArray64<uint8>& CompressedByteArray = ImageWrapper->GetCompressed();
						if (!FFileHelper::SaveArrayToFile(CompressedByteArray, *pngPath))
						{
							UE_LOG(LOG_DOT, Error, TEXT("Could not save thumbnail: '%s'."), *pngPath);
							return false;
						}

						return true;
					}
				}
			}

			//UE_LOG(LOG_DOT, Warning, TEXT("Could not find thumbnail: '%s'."), *fullName);
		}
	}

	return false;
}

void UBlueprintDoxygenCommandlet::writeBlueprintHeader(
	UBlueprint *blueprint,
	FString group,
	FString qualifier,
	FString packageName,
	int graphCount
)
{
	FString className = GetClassName(blueprint->GeneratedClass);
	FString parentClass = GetClassName(blueprint->ParentClass);
	
	bool isDataOnly = FBlueprintEditorUtils::IsDataOnlyBlueprint(blueprint);
	FString imagetag = "";

	if (outputDir != "-")
	{
		//FGenericPlatformMisc::GetDefaultPathSeparator()
		FString pngName = className + ".png";
		FString pngPath = currentDir + "\\" + pngName;
		FPaths::MakePlatformFilename(pngPath);

		if (
				!CreateThumbnailFile(blueprint, pngPath)
		//	||	!CreateThumbnailFile(blueprint->ParentClass, pngPath)
		)
		{
			FString tpngPath = pngPath;
			tpngPath.RemoveFromStart(outputDir);
			tpngPath = "[OutputDir]" + tpngPath;

			//UE_LOG(LOG_DOT, Error, TEXT("Could not create thumbnail: '%s'."), *pngPath);
			wcout << "No image:  " << *tpngPath << endl;
		}
		else
		{
			int width = 256;
			FString fmt;

			fmt = "\\image{inline} html {0} \"{1}\" width={2}px";
			imagetag = FString::Format(*fmt, { pngName, className, width });

			// \link ABP_ActorTest_C &thinsp; \image html ABP_ActorTest_C.png "ABP_ActorTest_C" width=256px \endlink
			fmt = "\\link {1} &thinsp; \\image html {0} \"{1}\" width={2}px \\endlink";
			FString galleryEntry = FString::Format(*fmt, { pngName, className, width });
			GalleryList.Add(galleryEntry);
		}
	}

	// Gives the line places to break if needed.
	// TODO: (actually turns our doxygen won't do it, so left this here with
	// &thinsp; for when I do figure out how to do it with &#8203;)
	FString packageNameBreaks = packageName.Replace(TEXT("/"), TEXT("&thinsp;/"));
	//packageName = packageName.Replace(TEXT("/"), TEXT("&zwnj;/"));

	// not sure why they allow you to put a cosmetic name on a blueprintfor.
	FString displayName = blueprint->BlueprintDisplayName;

	//FName MetaDeprecated = TEXT("DeprecatedNode");
	bool isDeprecated = blueprint->bDeprecate;
	FString sDeprecated = "This blueprint will be removed in future versions.";
	//*object->GetMetaData("DeprecationMessage")

	//The (default) config file where public values can be saved/referenced.
	FString config = GetTrimmedConfigFilePath(blueprint->GetDefaultConfigFilename());

	FString description;
	//description += "	This class " + blueprint->GetDesc() + ".\n";
	//description += "\n";
	description += blueprint->BlueprintDescription;
	description = description.TrimStartAndEnd();
	description = description.Replace(TEXT("\n"), TEXT("\n	"));

	*out << "/**" << endl;
	//*out << "	\\class " << *className << endl;
	if (!qualifier.IsEmpty())
		*out << "	\\qualifier " << *qualifier << endl;
	*out << "	\\ingroup " << *group << endl;
	if (isDeprecated)
		*out << "	\\deprecated " << *sDeprecated << endl;

	//*out << "	\\brief UDF Path: <b>" << *packageName << "</b> "<< endl;
	*out << "	\\brief A blueprint class with " << graphCount << " graphs." << endl;
	*out << endl;

	if (!imagetag.IsEmpty())					//floats right... needs to be as early in the description as possible.
		*out << "	" << *imagetag << endl;

	*out << "	UDF Path: <b>" << *packageNameBreaks << "</b>" << endl;
	*out << "	<br/>Config: <b>" << *config << "</b>" << endl;

	if (!displayName.IsEmpty())
		*out << "	<br/>Display Name: <b>" << *displayName << "</b>" << endl;
	if (isDataOnly)
		*out << "	This blueprint is <b>Data Only</b>" << endl;
	if (!description.IsEmpty())
	{	*out << "	" << endl;
		*out << "	" << *description << endl;
	}

	*out << "	<div style='clear:both;'/>" << endl;

	*out << "	\\headerfile " << *className << ".h \"" << *packageName << "\"" << endl;
	*out << "*/" << endl;
	*out << "class " << *className << " : " << *parentClass << endl;
	*out << "{" << endl;
}

void UBlueprintDoxygenCommandlet::writeMaterialHeader(
	UMaterial* material,
	FString group,
	FString qualifier,
	FString className,
	FString packageName,
	FString packageDescription,
	FString parentClass,
	int graphCount
)
{
	FString imagetag = "";

	if (outputDir != "-")
	{
		//FGenericPlatformMisc::GetDefaultPathSeparator()
		FString pngName = className + ".png";
		FString pngPath = currentDir + "\\" + pngName;
		FPaths::MakePlatformFilename(pngPath);
		if (!CreateThumbnailFile(material, pngPath))
		{
			UE_LOG(LOG_DOT, Error, TEXT("Could not create thumbnail: '%s'."), *pngPath);
		}
		else
		{
			//FString fmt = "![{1}]({0}){}";
			//FString fmt = "![{1}]: {0}";
			int width = 256;
			FString fmt = "\\image{inline} html {0} \"{1}\" width={2}px";
			imagetag = FString::Format(*fmt, { pngName, className, width });
		}
	}

	// Gives the line places to break if needed.
	// TODO: (actually turns out doxygen won't do it, so left this here with
	// &thinsp; for when I do figure out how to do it with &#8203;)
	FString packageNameBreaks = packageName.Replace(TEXT("/"), TEXT("&thinsp;/"));
	//packageName = packageName.Replace(TEXT("/"), TEXT("&zwnj;/"));

	//UEdGraphNode::GetDeprecationResponse()
	//FName MetaDeprecated = TEXT("DeprecatedNode");
	//bool isDeprecated = blueprint->bDeprecate;		//TODO fixme!
	bool isDeprecated = false;
	FString sDeprecated = "This blueprint will be removed in future versions.";
	//*object->GetMetaData("DeprecationMessage")

	*out << "/**" << endl;
	//*out << "	\\class " << *className << endl;
	if (!qualifier.IsEmpty())
		*out << "	\\qualifier " << *qualifier << endl;
	*out << "	\\ingroup " << *group << endl;
	if (isDeprecated)
		*out << "	\\deprecated " << *sDeprecated << endl;
	//*out << "	\\brief UDF Path: <b>" << *packageName << "</b> "<< endl;
	*out << "	\\brief A blueprint class with " << graphCount << " graphs." << endl;
	if (!imagetag.IsEmpty())
		*out << "	" << *imagetag << endl;
	*out << "	UDF Path: <b>" << *packageNameBreaks << "</b>" << endl;
	*out << endl;
	//*out << "	Contains " << graphCount << " graphs and " << *packageDescription << endl;
	*out << "	" << *packageDescription << endl;
	*out << "	<div style='clear:both;'/>" << endl;
	*out << "	\\headerfile " << *className << ".h \"" << *packageName << "\"" << endl;
	*out << "*/" << endl;
	*out << "class " << *className << " : " << *parentClass << endl;
	*out << "{" << endl;
}

FString UBlueprintDoxygenCommandlet::getCppType(FProperty *prop)
{
	TArray<FString> containers{ "TMap","TSet","TArray" };

	FString extype;
	FString ctype = prop->GetCPPType();
	FString mtype = prop->GetCPPMacroType(extype);
	FString type = ctype;

	if (containers.Contains(ctype))
		type = FString::Printf(TEXT("%s<%s>"), *ctype, *extype);

	//TODO: macro type!

	//TODO: FUNCTION type!

	return type;
}

void UBlueprintDoxygenCommandlet::writeAssetMembers(UBlueprint* blueprint, FString what)
{
	//name				//name, fname, cppname
	//type				//cpp type?
	//editable			
	//readonly
	//tooltip
	//3d widget?
	//expose on spawn
	//private
	//cinematics
	//config variable -- can use config file?
	//category
	//replication
	//savegame?
	//deprecated
	//editoronly?

	/*
	UClass* objectClass = blueprint->GetClass();
	for (FProperty* p = objectClass->PropertyLink; p; p = p->PropertyLinkNext)
	{
		FString nm = p->GetName();
		FString fn = p->GetFName().ToString();
		FString n = p->GetNameCPP();
		FString t = p->GetCPPType();			//there's more here!!

		EPropertyFlags f = p->PropertyFlags;

		wcout << "PROP1: " << *nm << " " << *fn << " " << *n << " " << *t << endl;
		wcout << std::bitset<64>(f) << endl;
	}
	*/

	wcout << "TODO: add other vars.  They're not as important and make the documentation messy." << endl;

	/*
	TSet<FName> variables;
	FBlueprintEditorUtils::GetClassVariableList(blueprint, variables, false);
	//FBlueprintEditorUtils::GetClassVariableList(blueprint, variables, true);
	TArray<FName> nonPrivateVars;
	FBPVariableDescription *vd = NULL;
	for (FName n : variables)
	{
		nonPrivateVars.Add(n);
		wcout << *n.ToString() << endl;
	}
	*/

	//need:
	// private/public/protected			[x] not sure it's correct
	// type								[x]
	// name								[x]
	// default value (if not empty)		[/] TODO: need compound/complex/function object stuff
	// detail/description				[x] GetToolTipText() or friendlyName
	// category							[x] sort by this
	// replication						[x] there are more details in ELifetimeCondition
	// deprecated						[x] we use default a deprecation message sometimes

	TMap<FString, TMap<FString, TArray<FString> > > members;	//yeah man!

	members.Add("public", TMap<FString, TArray<FString> >());
	members.Add("protected", TMap<FString, TArray<FString> >());
	members.Add("private", TMap<FString, TArray<FString> >());

	//variables new to this class (added to it's parent class)
	TArray<FBPVariableDescription> vars = blueprint->NewVariables;
	for (FBPVariableDescription v : vars)
	{
		FProperty* fp = blueprint->GeneratedClass->FindPropertyByName(v.VarName);

		//FString extype;
		//wcout << "CPP: " << *v.VarName.ToString() << endl;
		//wcout << "CPP: " << *fp->GetCPPTypeForwardDeclaration() << endl;
		//wcout << "CPP: " << *fp->GetNameCPP() << endl;						//may include _DEPRECATED
		//wcout << "CPP: " << *fp->GetCPPType() << endl;
		//wcout << "CPP: " << *UEdGraphSchema_K2::TypeToText(fp).ToString() << endl;
		//wcout << "CPP: " << *fp->GetToolTipText().ToString() << endl;
		//wcout << "CPP: " << *fp->GetCPPMacroType(extype) << endl;
		//wcout << "CPP: " << *extype << endl;
		//wcout << "CPP: " << *fp->valu
		//wcout << "========" << endl;

		//FString varName = createVariableName(v.VarName.ToString(),false);
		FString variableName = createVariableName(fp->GetNameCPP(), false);
		FString friendlyName = v.FriendlyName;
		FString category = v.Category.ToString();
		FString type = getCppType(fp);

		FString initializer = v.DefaultValue;
		//if (isObject)
		{
			//TODO defaults!
		}

		FString description = fp->GetToolTipText().ToString();
		if (description.IsEmpty())
			description = friendlyName;
		description = description.Replace(TEXT("\r"), TEXT(""));
		description = description.Replace(TEXT("\n"), TEXT(" "));
		description = description.Replace(TEXT("\\"), TEXT("\\\\"));
		//wcout << "DESCRIPTION: " << *description << endl;

		bool isPrivate = true;
		bool isPublic = false;

		uint64 flags = v.PropertyFlags;
		bool isReadonly = flags & EPropertyFlags::CPF_BlueprintReadOnly;
		bool isConst = flags & EPropertyFlags::CPF_ConstParm;
		bool isInstanceEditable = !(flags & EPropertyFlags::CPF_DisableEditOnInstance);
		bool isConfigSaved = flags & EPropertyFlags::CPF_Config;

		//TODO: get more data	UEdGraphNode::GetDeprecationResponse
		bool isDeprecated = flags & EPropertyFlags::CPF_Deprecated;
		FString sDeprecated = fp->GetMetaData("DeprecationMessage");
		sDeprecated = sDeprecated.Replace(TEXT("\r"), TEXT(""));
		sDeprecated = sDeprecated.Replace(TEXT("\n"), TEXT("<br/>"));
		sDeprecated = sDeprecated.Replace(TEXT("\\"), TEXT("\\\\"));
		if (sDeprecated.IsEmpty()) sDeprecated = "This variable will be removed in future versions.";

		uint32 rflags = v.ReplicationCondition;
		bool isReplicated = rflags > 0;				//TODO: there's much more with ELifetimeCondition::*

		//wcout << std::bitset<64>(flags) << endl;
		//wcout << std::bitset<32>(rflags) << endl;

		isPublic = isInstanceEditable;				//this is probably incorrect or incomplete
		isPrivate = isReadonly;						//this is probably incorrect or incomplete

		//isPrivate = flags & EPropertyFlags::CPF_NativeAccessSpecifierPrivate;

		//FString e = FString::Printf(TEXT("%s%s %s = \"%s\";\t//!< %s"),
		//	isConst ? "const " : "",
		//	*type,
		//	*variableName,
		//	*initializer,
		//	*description
		//);

		FString e = FString::Printf(TEXT("%s%s %s;\t//!< %s"),
			isConst ? "const " : "",
			*type,
			*variableName,
			*description
		);

		if (isReplicated)
			e += " \\qualifier Replicated";
		if (isConfigSaved)
			e += " \\qualifier Config";
		if (isDeprecated)
			e += " \\deprecated " + sDeprecated;

		if (isPrivate)
			members["private"].FindOrAdd(category).Add(e);
		else if (isPublic)
			members["public"].FindOrAdd(category).Add(e);
		else
			members["protected"].FindOrAdd(category).Add(e);
	}

	if (members["public"].Num())
	{
		*out << "public:" << endl;
//		*out << "	/**" << endl;
//		*out << "		\\name Public " << *what << " Variables" << endl;
//		*out << "		Members exposed outside the graph editor and in subclasses." << endl;
//		*out << "	*/" << endl;
//		*out << "	///@{" << endl;
		//*out << "	//public members exposed to the editor." << endl;

		members["public"].KeySort([](FString A, FString B) { return A < B; });
		for (TPair<FString, TArray<FString> > k : members["public"])
		{
			if (k.Key != "Default")
			{
//				*out << "	/** \\name " << *k.Key << " (public) */" << endl;	//we have no description, just a category
//				*out << "	///@{" << endl;
			}

			for (FString v : k.Value)
				*out << "	" << *v << endl;

//			if (k.Key != "Default")
//				*out << "	///@}" << endl;
		}

//		*out << "	///@}" << endl;
	}

	if (members["protected"].Num())
	{
		*out << endl;
		*out << "protected:" << endl;
//		*out << "	/**" << endl;
//		*out << "		\\name Protected " << *what << " Variables" << endl;
//		*out << "		Members available inside the graph editor and in subclasses." << endl;
//		*out << "	*/" << endl;
//		*out << "	///@{" << endl;

		members["protected"].KeySort([](FString A, FString B) { return A < B; });
		for (TPair<FString, TArray<FString> > k : members["protected"])
		{
			if (k.Key != "Default")
			{
//				*out << "	/** \\name " << *k.Key << " (protected) */" << endl;	//we have no description, just a category
//				*out << "	///@{" << endl;
			}

			for (FString v : k.Value)
				*out << "	" << *v << endl;

//			if (k.Key != "Default")
//				*out << "	///@}" << endl;
		}

//		*out << "	///@}" << endl;
	}

	//always report private... that's where graphs go.
	*out << endl;
	*out << "private:" << endl;
//	*out << "	/**" << endl;
//	*out << "		\\name Private " << *what << " Variables" << endl;
//	*out << "		Members available inside the graph editor only." << endl;
//	*out << "	*/" << endl;
//	*out << "	///@{" << endl;

	members["private"].KeySort([](FString A, FString B) { return A < B; });
	for (TPair<FString, TArray<FString> > k : members["private"])
	{
		if (k.Key != "Default")
		{
//			*out << "	/** \\name " << *k.Key << " (private) */" << endl;	//we have no description, just a category
//			*out << "	///@{" << endl;
		}

		for (FString v : k.Value)
			*out << "	" << *v << endl;

//		if (k.Key != "Default")
//			*out << "	///@}" << endl;
	}

//	*out << "	///@}" << endl;
}

void UBlueprintDoxygenCommandlet::writeAssetFooter()
{
	*out << "};" << endl;
	*out << endl;
}

void UBlueprintDoxygenCommandlet::writeAssetCalls()
{
	*out << R"PixoVR(
/*
	This is fake C++ that doxygen will parse for call graph entries.

	In this pseudo-C++, everything is a function and gets called
	if it's mentioned in a node graph.  Doxygen parses it anyway!

	We use the URLs of the nodes to make this list.
*/
)PixoVR" << endl;

	for (const TPair<FString, TArray<FString> >& e : GraphCalls)
	{
		*out << *e.Key << endl;
		*out << "{" << endl;
		TArray<FString> list = e.Value;
		list.HeapSort();
		for (FString l : list)
		{
			if (!l.IsEmpty())
				*out << "	" << *l << "();" << endl;
		}
		*out << "}" << endl;
		*out << endl;
	}
}

FString UBlueprintDoxygenCommandlet::prepTemplateString(FString prefix, vmap style, FString string)
{
	FString h = prefix + string;

	for (auto& e : style)
		h = h.Replace(*e.Key, *e.Value);

	//do it twice, in case a value also has variables in it.
	for (auto& e : style)
		h = h.Replace(*e.Key, *e.Value);

	//replace empty font tag
	FRegexPattern emptyFont(TEXT("<font [^<]*><\\/font>"));
	FRegexMatcher matcher(emptyFont, h);
	TArray<FString> m;
	while (matcher.FindNext())
	{
		FString s = matcher.GetCaptureGroup(0);
		//wcout << "MATCH: [" << *s << "]" << endl;
		m.Add(s);
	}
	for (FString s : m)
		h = h.Replace(*s, TEXT(""));

	//clean up any empty <i> tags from replacement
	h = h.Replace(TEXT("<br/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i></i>"), TEXT(""));				//for nodes missing a second line

	h = h.Replace(TEXT("\r"), *(TEXT("") + prefix));
	h = h.Replace(TEXT("\n"), *(TEXT("\n") + prefix));
	//h = h.Replace(TEXT("\\"), *(TEXT("\\\\") + prefix));

	return h;
}

void UBlueprintDoxygenCommandlet::writeGraphHeader(FString prefix, UEdGraph* graph, FString qualifier)
{
	FString graphName = graph->GetName();
	FString graphNameVariable = createVariableName(graphName, false);
	FString graphNameHuman = FName::NameToDisplayString(graphName, false);

	FString brief = GraphDescriptions.FindOrAdd(graph, "");
	FString details = "";
	FString details2 = FBlueprintEditorUtils::GetGraphDescription(graph).ToString();
	if (details2 != "(None)")	//EventGraph never has any description and reports "(None)".
		details += details2;
	if (brief == details)		//don't let it repeat.
		details = "";

	*out << *prefix << "/**" << endl;
	*out << *prefix << *_tab << "\\qualifier " << *qualifier << endl;
	//*out << *prefix << *_tab << "\\private" << endl;
	if (!brief.IsEmpty())
		*out << *prefix << *_tab << "\\brief " << *brief << endl;
	if (!details.IsEmpty())
		*out << *prefix << *_tab << "\\details " << *details << endl;
	*out << *prefix << *_tab << "\\dot " << *graphNameHuman << endl;
	*out << *prefix << *_tab << "graph " << *graphNameVariable << " {";
	*out << *prepTemplateString(prefix + _tab + _tab, BlueprintStyle, R"PixoVR(
graph [
	layout="fdp"
	overlap="true"
	Xsplines="curved"
	layers="comments:edges:nodes:bubbles"
	fontname="_FONTNAME_"
	fontsize="_FONTSIZE_"
	fontcolor="_FONTCOLOR_"
	labelfontname="_FONTNAME_"
	labelfontsize="_FONTSIZE_"
	bgcolor="_GRAPHBG_"
	stylesheet="_STYLESHEET_"
	outputorder="edgesfirst"
];
edge [
	layer="edges"
	penwidth="_EDGETHICKNESS_"
	color="_EDGECOLOR_"
	arrowhead="dot"
	arrowtail="dot"
	arrowsize="0.5"
	headclip="false"
	tailclip="false"
	dir="none"
];
node [
	layer="nodes"
	fontname="_FONTNAME_"
	fontsize="_FONTSIZE_"
	fontcolor="_FONTCOLOR_"
	shape="plain"
	fixedsize="shape"
	color="_BORDERCOLOR_"
];)PixoVR");
	*out << endl;
}

void UBlueprintDoxygenCommandlet::writeGraphFooter(FString prefix, UEdGraph *graph)
{
	FString graphName = graph->GetName();
	FString graphNameVariable = createVariableName(graphName, false);
	FString graphNameHuman = FName::NameToDisplayString(graphName, false);

	//FString type = g->GetClass()->GetSuperClass()->GetFName().ToString();
	//FString type = GetClassName(graph->GetOuter()->GetClass());
	FString type = GetClassName(graph->GetClass());

	*out << *prefix << *_tab << "}" << endl;
	*out << *prefix << *_tab << "\\enddot" << endl;
	*out << *prefix << "*/" << endl;

	//bool isFunction = ??

	//if (isFunction)
	//	*out << *prefix << *type << " " << *functionName << "(" << *functionArguments << ");" << endl;
	//else

	FString cpp = GetGraphCPP(graph);
	*out << *prefix << *cpp << ";	//\"" << *graphNameHuman << "\"" << endl;
}

void UBlueprintDoxygenCommandlet::writeGraphConnections(FString prefix)
{
	*out << endl;

	for (const TPair<FString, FString>& c : PinConnections)
	{
		//FROM:port:_ -- TO:port:_ [ color="colorFmt" ]
		FString connection = FString::Printf(TEXT("%s [ color=\"%s\" layer=\"edges\" ];"), *c.Key, *c.Value);

		*out << *prefix << *_tab << *_tab << *connection << endl;
	}
}

TMap<FString, UBlueprintDoxygenCommandlet::NodeType> UBlueprintDoxygenCommandlet::NodeType_e = {
	{ "none",						NodeType::none			},
	{ "node",						NodeType::node			},
	{ "comment",					NodeType::comment		},
	{	"EdGraphNode_Comment",		NodeType::comment		},
	{ "bubble",						NodeType::bubble		},
	{ "path",						NodeType::path			},	//TODO: no instances of this yet
	{ "route",						NodeType::route			},
	{	"K2Node_Knot",				NodeType::route			},
	{ "variable",					NodeType::variable		},
	{	"K2Node_VariableGet",		NodeType::variable		},
	{ "variableset",				NodeType::variableset	},
	{	"K2Node_VariableSet",		NodeType::variableset	},
	{ "compact",					NodeType::compact		},
	{	"K2Node_PromotableOperator",NodeType::compact		},
	{ "composite",					NodeType::composite		},
	{	"K2Node_Composite",			NodeType::composite		},
	{ "function",					NodeType::function		},
	{	"K2Node_CallFunction",		NodeType::function		},
	{	"K2Node_CallParentFunction",NodeType::function		},	//NO: FunctionEntry
	{ "macro",						NodeType::macro			},	//dimmer header, but normal node
	{	"K2Node_MacroInstance",		NodeType::macro			},
	{ "tunnel",						NodeType::tunnel		},
	{	"K2Node_Tunnel",			NodeType::tunnel		},
	{ "event",						NodeType::event			},
	{	"K2Node_Event",				NodeType::event			},
	{	"K2Node_CustomEvent",		NodeType::event			},
	{ "spawn",						NodeType::spawn			},
	{	"K2Node_SpawnActor",		NodeType::spawn			},
	{	"K2Node_SpawnActorFromClass",NodeType::spawn		},
	{	"K2Node_GenericCreateObject",NodeType::spawn		},
	{	"K2Node_ConstructObjectFromClass",NodeType::spawn	},
	{	"K2Node_DynamicCast",		NodeType::spawn			},
	{	"K2Node_ClassDynamicCast",	NodeType::spawn			},
	{ "MAX",						NodeType::MAX			}
};

FString UBlueprintDoxygenCommandlet::getNodeTemplate(NodeType type, bool hasDelegate)
{
	//magic node types:
//	(all types, except those noted below) // regular node
//	EdGraphNode_Comment		// actually has size
//	[event thing]			// will have different urls
//	K2Node_Knot				// ShouldDrawNodeAsControlPointOnly = true
//	K2Node_Tunnel			// input or output
// 	K2Node_VariableGet		// variable, no header, tinted node
//	K2Node_Composite		// subgraph, will have different urls
//	[function]				// subgraph!
//	[macro]					// subgraph, but probably not openable

	FString t;
	switch (type)
	{
	case NodeType::composite:	//composite	//no icon, no header color, may be multi-line title
		t = R"PixoVR(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table style="rounded" bgcolor='_HEADERCOLORLIGHT_' border="1" cellborder="0" cellspacing="0" cellpadding="_COMPOSITEPADDING_">
				<tr>
					<td align="left" balign="left" colspan="4"><b>_NODETITLE_</b><br/><i>_NODETITLE2_</i></td>
				</tr>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)PixoVR";

		break;

	case NodeType::compact:		//compact
		t = R"PixoVR(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table style="rounded" align="left" bgcolor='_HEADERCOLORLIGHT_' border="1" cellborder="0" cellspacing="0" cellpadding="_COMPOSITEPADDING_">
				<tr>
					<td align="center" balign="center" fixedsize="true" width="_COMPACTWIDTH_" height="1" colspan="4"><font color="_COMPACTCOLOR_" point-size="_COMPACTSIZE_"><b>_NODETITLE_</b></font></td>
				</tr>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)PixoVR";

		break;

	case NodeType::event:		//event
	case NodeType::macro:		//macro		//dimmer header
	case NodeType::node:		//plain node

		if (hasDelegate)
		{
			t = R"PixoVR(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor='_NODECOLOR_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
				<tr>
					<td colspan="3" align="left"  balign="left"  width="1" height="0" bgcolor="_HEADERCOLORDIM_" port="icon"    ><b>_NODEICON__HEIGHTSPACER__NODETITLE__HEIGHTSPACER_</b><br/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>_NODETITLE2_</i></td>
					<td colspan="1" align="right" balign="right" width="1" height="0" bgcolor="_HEADERCOLORDIM_" port="delegate">_HEIGHTSPACER__NODEDELEGATE_</td>
				</tr>
				<hr/>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)PixoVR";
		}
		else
		{
			t = R"PixoVR(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor='_NODECOLOR_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
				<tr>
					<td colspan="4" align="left"  balign="left"  width="1" height="0" bgcolor="_HEADERCOLORDIM_" port="icon"    ><b>_NODEICON__HEIGHTSPACER__NODETITLE__HEIGHTSPACER_&nbsp;</b></td>
				</tr>
				<hr/>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)PixoVR";
		}

		break;

	case NodeType::spawn:
	case NodeType::function:
	case NodeType::tunnel:

		if (hasDelegate)
		{
			t = R"PixoVR(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor='_NODECOLOR_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
				<tr>
					<td colspan="3" align="left"  balign="left"  width="1" height="0" bgcolor="_HEADERCOLOR_" port="icon"    ><b>_NODEICON__HEIGHTSPACER__NODETITLE__HEIGHTSPACER_</b><br/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>_NODETITLE2_</i></td>
					<td colspan="1" align="right" balign="right" width="1" height="0" bgcolor="_HEADERCOLOR_" port="delegate">_HEIGHTSPACER__NODEDELEGATE_</td>
				</tr>
				<hr/>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)PixoVR";
		}
		else
		{
			t = R"PixoVR(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor='_NODECOLOR_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
				<tr>
					<td colspan="4" align="left"  balign="left"  width="1" height="0" bgcolor="_HEADERCOLOR_" port="icon"    ><b>_NODEICON__HEIGHTSPACER__NODETITLE__HEIGHTSPACER_&nbsp;</b><br/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>_NODETITLE2_</i></td>
				</tr>
				<hr/>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)PixoVR";
		}

		break;

	case NodeType::comment:		//comment node
		t = R"PixoVR(
_NODENAME_ [
	layer="comments"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor="_HEADERCOLORTRANS_" border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_" width="_WIDTH_" height="_HEIGHT_">
				<tr>
					<td align="left" height="0" width="_WIDTH_" bgcolor="_HEADERCOLOR_" port="comment"><font point-size="_FONTSIZECOMMENT_"><b>_NODECOMMENT_</b></font></td>
				</tr>
				<hr/>
				<tr>
					<td colspan="3" align="left" port="body"></td>
				</tr>
			</table>
		</td></tr></table>
	>
];)PixoVR";

		break;

	case NodeType::variable:	//variable node
		t = R"PixoVR(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table align="right" style="rounded" bgcolor='_HEADERCOLORLIGHT_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)PixoVR";

		break;

	case NodeType::variableset:		//variableset
		t = R"PixoVR(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table style="rounded" align="left" bgcolor='_HEADERCOLORLIGHT_' border="1" cellborder="0" cellspacing="0" cellpadding="_COMPOSITEPADDING_">
				<tr>
					<td align="center" balign="center" fixedsize="true" width="_COMPACTWIDTH_" height="1" colspan="4"><font color="_COMPACTCOLOR_" point-size="_COMPACTSIZE_"><b>SET</b></font></td>
				</tr>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)PixoVR";

		break;

	case NodeType::bubble:		//bubble node

		t = R"PixoVR(
_NODENAME__comment [
	layer=bubbles
	pos="_POS_"
	tooltip="_TOOLTIP_"
	fontcolor="_FONTCOLORBUBBLE_"
	class="_CLASS_"
			
	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table style="rounded" bgcolor="_NODECOLORTRANS_" border="1" cellborder="0" cellspacing="0" cellpadding="_COMMENTPADDING_">
				<tr>
					<td align="left" balign="left" port="comment"><font point-size="_FONTSIZEBUBBLE_">_NODECOMMENT_</font></td>
				</tr>
				<tr>
					<td align="left" cellpadding="0" cellspacing="0" fixedsize="true" width="50" height="2"><font color="_BORDERCOLOR_" point-size="12">&nbsp;&nbsp;&nbsp;&nbsp;&#9660;</font></td>
				</tr>
			</table>
		</td></tr></table>
	>
];)PixoVR";

		break;

	case NodeType::route:		//route node
		t = R"PixoVR(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table border="0" cellborder="0" cellspacing="0" cellpadding="0">
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)PixoVR";

		break;

	default:
		wcout << *FString::Printf(TEXT("//Unknown node type: '%d'. Can't create output."), type) << endl;
		return FString::Printf(TEXT("comment=\"Unknown node type: '%d'. Can't create output.\""), type);

		break;
	}

	return t;
}

TMap<FString, FString> UBlueprintDoxygenCommandlet::NodeIcons = {
	{ "default",	"&#10765;"	},		// function symbol
	{ "event",		"&#10070;"	},		// diamond		&#10070; &#9672; &#11030; &#11031;
	{ "container",	"&#8651;"	},		// arrows?
	{ "switch",		"&#8997;"	},		// switch
	{ "path",		"&#8916;"	},		// fork in the road
	{ "io",			"&#10140;"	},		// input/output circle arrow		&#8658; &#128468;
	{ "spawn",		"&#10038;"	}		// star			&#9733; &#10038; //https://www.amp-what.com/unicode/search/star
};

TMap<FString, TPair<FString,FString> > UBlueprintDoxygenCommandlet::PinIcons = {
	//{ "example",	{ "conicon", "disicon" } },		// connected/disconnected, or closed/open
	{ "data",		{ "&#9678;", "&#9673;" } },		// open/closed circle
	{ "exec",		{ "&#9655;&#65038;", "&#9654;&#65038;" } },					// open/closed right arrow. Uses &#65038; after to request the non-emoji representation.
	{ "delegate",	{ "&#9634;", "&#9635;" } },		// open/closed box
	{ "addpin",		{"&#10753;", "&#10753;"} },		// open/closed circle		//TODO &#8853;	//https://www.isthisthingon.org/unicode/index.phtml?page=02&subpage=A
	{ "container",	{ "&#9920;", "&#9923;" } },		// open/closed barrel thing
	//{ "array",		{"&#10214;&#9678;&#10215;","&#10214;&#9673;&#10215;"}},	// open/closed square braces
	{ "array",		{"[&#9678;]","[&#9673;]"}},		// open/closed square braces	&#128992; grid circle thing
	{ "map",		{"&#10218;&#9638;&#10219;","&#10218;&#9641;&#10219;" } },	// open/closed angle bracket
	{ "set",		{"&#10100;&#9678;&#10101;","&#10100;&#9673;&#10101;"}},		// open/closed curly braces
	{ "skull",		{ "&#9760;", "&#9760;" } },		// skull
	{ "coffee",		{ "&#9982;", "&#9982;" } },		// coffee
	{ "dotthing",	{ "&#10690;","&#10690;"} }		// circle dot thing
};

FString UBlueprintDoxygenCommandlet::GetPinColor(UEdGraphPin* pin)
{
	FLinearColor c = FLinearColor::Black;

	if (pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate)
	{
		//c = FLinearColor::Red*.5;
		return createColorString(FLinearColor::Red,1.0f,1.5f);
	}
	else if (pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
	{
		if (const UEdGraphSchema* Schema = pin->GetSchema())
			c = Schema->GetPinTypeColor(pin->PinType);
	}
	else
		c = FLinearColor::Black;

	return createColorString(c);
}

FString UBlueprintDoxygenCommandlet::GetPinIcon(UEdGraphPin* pin)
{
	bool isConnected = pin->HasAnyConnections();
	FString type = GetPinType(pin);

	if (type == "Exec")
		return isConnected ? PinIcons["exec"].Value : PinIcons["exec"].Key;
	else if (type == "Delegate")
		return isConnected ? PinIcons["delegate"].Value : PinIcons["delegate"].Key;
	else if (PinIcons.Contains(type))
		return isConnected ? PinIcons[type].Value : PinIcons[type].Key;

	//Object
	//Real
	//Struct
	//Wildcard
	//Bool
	//String
	//LinearColor
	//...?

	return isConnected ? PinIcons["data"].Value : PinIcons["data"].Key;
}

UBlueprintDoxygenCommandlet::NodeType UBlueprintDoxygenCommandlet::GetNodeType(UEdGraphNode* node, NodeType defaultType)
{
	FString stype = node->GetClass()->GetFName().ToString();
	NodeType type = NodeType_e.FindOrAdd(stype, defaultType);

	UK2Node* n2 = dynamic_cast<UK2Node*>(node);
	if (n2 && n2->ShouldDrawCompact())
			return NodeType::compact;

	//if (FBlueprintEditorUtils::IsTunnelInstanceNode(node))			//is this needed?
	//	return NodeType::tunnel;

	if (type == NodeType::none)
		if (outputMode & OutputMode::debug)
			UE_LOG(LOG_DOT, Error, TEXT("Unknown node type: '%s'. Returning NodeType::none."), *stype);

	return type;
}

FString UBlueprintDoxygenCommandlet::GetNodeTypeGroup(UBlueprintDoxygenCommandlet::NodeType type)
{
	return *NodeType_e.FindKey(type);
}

FString UBlueprintDoxygenCommandlet::GetNodeTooltip(UEdGraphNode* node)
{
	FString tooltip = node->GetTooltipText().ToString();
	tooltip = tooltip.Replace(TEXT("\\"), TEXT("\\\\"));
	tooltip = tooltip.ReplaceQuotesWithEscapedQuotes();
	tooltip = tooltip.TrimStartAndEnd();
	//tooltip = tooltip.Replace(TEXT("\n"), TEXT("<BR/>"));
	//tooltip = tooltip.Replace(TEXT("\n"), TEXT("\\n"));			//this tooltip is never in html context
	//tooltip = tooltip.Replace(TEXT("\n"), TEXT("&#10;"));
	tooltip = tooltip.Replace(TEXT("\r"), TEXT(""));
	tooltip = tooltip.Replace(TEXT("\n"), TEXT("&#013;"));
	//tooltip = tooltip.Replace(TEXT("\\"), TEXT("\\\\"));

	return tooltip;
}

FString UBlueprintDoxygenCommandlet::GetPinURL(UEdGraphPin* pin)
{
	return "";

	//TODO: do we ever actually need a pinurl?
	//FString url = GetNodeURL(pin->GetOwningNode(),pin->Direction);

	//...

	//return url;
}

FString UBlueprintDoxygenCommandlet::GetNodeURL(UEdGraphNode* node, EEdGraphPinDirection direction)
{
	//subgraphs							X
	//inputs/outputs (tunnels)			X
	//spawn nodes
	//variables							X
	//function nodes					X
	//macro								X

	NodeType type = GetNodeType(node, NodeType::node);
	FString url = "";

	UBlueprint* blueprint = FBlueprintEditorUtils::FindBlueprintForNode(node);
	FString blueprintClassName = GetClassName(blueprint->GeneratedClass);
	FString variableName;				// leave uninitialized

	UEdGraph* originalGraph = node->GetGraph();
	FString originalGraphName = createVariableName(originalGraph->GetName(), false);
	FString originalBlueprintClassName = blueprintClassName;

	switch (type)
	{
		case NodeType::composite:		// subgraph
		case NodeType::tunnel:			// subgraph
		{
			UK2Node_Tunnel* tunnel = dynamic_cast<UK2Node_Tunnel*>(node);

			if (!tunnel)
			{
				UE_LOG(LOG_DOT, Error, TEXT("Can't cast to tunnel."));
				break;
			}

			UK2Node_Tunnel* inode = tunnel->GetInputSink();
			UK2Node_Tunnel* onode = tunnel->GetOutputSource();

			UEdGraph* graph = NULL;
			UEdGraph* igraph = inode ? inode->GetGraph() : NULL;
			UEdGraph* ograph = onode ? onode->GetGraph() : NULL;

			switch (direction)
			{
			case EEdGraphPinDirection::EGPD_Input:
				graph = igraph;
				break;

			case EEdGraphPinDirection::EGPD_Output:
				graph = ograph;
				break;

			case EEdGraphPinDirection::EGPD_MAX:
			default:
				if (igraph && ograph && igraph != ograph)
					wcerr << "Hm, input node doesn't match output node!" << endl;

				graph = igraph ? igraph : ograph;	// we prioritize igraph over ograph
				break;
			}

			if (graph)
			{
				blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(graph);

				//if not on the no-fly list...
				if (ShouldReportObject(blueprint))
				{
					blueprintClassName = GetClassName(blueprint->GeneratedClass);
					variableName = createVariableName(graph->GetName(), false);
				}
			}
		}
		break;

	case NodeType::variableset:
	case NodeType::variable:		// local variables
		{
			//Local Variables don't get a url.  Let's get a list of those.
			TArray<UK2Node_FunctionEntry*> fe_nodes;
			TArray<FName> locals;
			node->GetGraph()->GetNodesOfClass<UK2Node_FunctionEntry>(fe_nodes);
			if (fe_nodes.Num() == 1)
			for (FBPVariableDescription& LocalVar : fe_nodes[0]->LocalVariables)
			{
				locals.Add(LocalVar.VarName);
				//wcout << "FBP: " << *LocalVar.VarName.ToString() << " node: " << *node->GetName() << endl;
			}

			UK2Node_Variable* variable = dynamic_cast<UK2Node_Variable*>(node);
			//UK2Node_VariableSet

			if (variable)
			{
				FProperty* fp = variable->GetPropertyForVariable();

				//wcout << "FPp: " << *fp->NamePrivate.ToString() << endl;
				if (locals.Contains(fp->NamePrivate))	//local variables don't get a URL.
					return "";

				UClass *blueprintClass = variable->VariableReference.GetMemberParentClass();
				if (blueprintClass)
				{
					blueprintClassName = GetClassName(blueprintClass);
				}
				else
				{
					blueprint = variable->GetBlueprint();

					//if not on the no-fly list...
					if (ShouldReportObject(blueprint))
					{	blueprintClassName = GetClassName(blueprint->GeneratedClass);
						variableName = createVariableName(fp->GetNameCPP(), false);
					}
				}
			}
		}
		break;

	//case NodeType::cast:			// UK2Node_DynamicCast	//UK2Node_ClassDynamicCast
	case NodeType::spawn:			// spawn TODO!  Only makes sense if the selected class/actor is in a package we're documenting.
		{
			/*
			UK2Node_SpawnActor* avariable = dynamic_cast<UK2Node_SpawnActor*>(node);
			UK2Node_ConstructObjectFromClass* cvariable = dynamic_cast<UK2Node_ConstructObjectFromClass*>(node);
			
			UEdGraph* graph = NULL;

			//if (avariable)
			//	graph = avariable->GetMacroGraph();
			//	graph = avariable->graph
			if (cvariable)
			{
				UClass *oclass = cvariable->GetClassToSpawn();
				if (oclass->GetTypedOuter(UBlueprint::StaticClass()))

				graph = 
			}

			if (!graph)
			{
				//wcout << "no graph! " << *node->GetClass()->GetFName().ToString() << endl;
				break;
			}

			//blueprint = variable->GetBlueprint();
			blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(graph);

			//if not on the no-fly list...
			if (ShouldReportObject(blueprint))
			{	blueprintClassName = GetClassName(blueprint->GeneratedClass);
				variableName = createVariableName(graph->GetName(), false);
			}
			*/
		}
		break;

	case NodeType::compact:
	case NodeType::macro:
		{
			UK2Node_MacroInstance* variable = dynamic_cast<UK2Node_MacroInstance*>(node);
			UEdGraph* graph = NULL;

			if (variable)
				graph = variable->GetMacroGraph();

			if (!graph)
			{
				//wcout << "no graph! " << *node->GetClass()->GetFName().ToString() << endl;
				break;
			}

			//blueprint = variable->GetBlueprint();
			blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(graph);

			//if not on the no-fly list...
			if (ShouldReportObject(blueprint))
			{	blueprintClassName = GetClassName(blueprint->GeneratedClass);
				variableName = createVariableName(graph->GetName(), false);
			}
		}
		break;

	case NodeType::function:
		{
			UK2Node_CallFunction* fvariable = dynamic_cast<UK2Node_CallFunction*>(node);
			UEdGraph* graph = NULL;
			const UEdGraphNode* evt;		//we ignore this

			if (fvariable)
				graph = fvariable->GetFunctionGraph(evt);

			if (!graph)
			{
				//wcout << "no graph! " << *node->GetClass()->GetFName().ToString() << endl;
				break;
			}

			//blueprint = variable->GetBlueprint();
			blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(graph);

			//if not on the no-fly list...
			if (ShouldReportObject(blueprint))
			{	blueprintClassName = GetClassName(blueprint->GeneratedClass);
				variableName = createVariableName(graph->GetName(), false);
			}
		}
		break;

	default:
		break;
	}

	// we don't want a node to point to its own graph entry as a url.
	if (originalGraphName == variableName &&
		originalBlueprintClassName == blueprintClassName)
		return "";

	FString format = "\\ref {0}::{1}";

	if (!variableName.IsEmpty())
		url = FString::Format(*format, { blueprintClassName, variableName });
	//else
	//	UE_LOG(LOG_DOT, Error, TEXT("No variable name present!"));

	//FString nn = node->GetNodeTitle(ENodeTitleType::MenuTitle).ToString();
	//wcout << *originalBlueprintClassName << " || " << *blueprintClassName << " || " << *originalGraphName << " || " << *variableName << " >> ";
	//wcout << *nn << " :: " << *node->GetName() << " :: URL: " << *url << endl;

	FString cpp = GetGraphCPP(originalGraph, originalBlueprintClassName);
	if (!url.IsEmpty())
	{
		FString curl = blueprintClassName + "::" + variableName;
		GraphCalls.FindOrAdd(cpp).AddUnique(curl);
	}
	else
		GraphCalls.FindOrAdd(cpp).AddUnique("");

	return url;
}

FString UBlueprintDoxygenCommandlet::GetGraphCPP(UEdGraph* graph, FString _namespace)
{
	FString graphName = graph->GetName();
	FString graphNameVariable = createVariableName(graphName, false);
	//FString graphNameHuman = FName::NameToDisplayString(graphName, false);

	//FString type = g->GetClass()->GetSuperClass()->GetFName().ToString();
	//FString type = GetClassName(graph->GetOuter()->GetClass());
	FString type = GetClassName(graph->GetClass());
	type = "void";

	//TODO ... function signatures?
	//bool isFunction = ??
	//if (isFunction)
	//	*out << *prefix << *type << " " << *functionName << "(" << *functionArguments << ");" << endl;
	//else
	//	*out ...

	FString cpp;
	
	if (!_namespace.IsEmpty())
		cpp = type + " " + _namespace + "::" + graphNameVariable + "()";
	else
		cpp = type + " " + graphNameVariable + "()";

	return cpp;
}

FString UBlueprintDoxygenCommandlet::GetClassName(UClass* _class)
{
	FString className = _class->GetPrefixCPP() + _class->GetFName().ToString();
	return className;
}

FString UBlueprintDoxygenCommandlet::GetNodeIcon(UEdGraphNode* node)
{
	NodeType type = GetNodeType(node);

	switch (type)
	{
		case NodeType::event:
			return NodeIcons["event"];		//event

		case NodeType::composite:
		case NodeType::function:
			return NodeIcons["container"];	//container

		case NodeType::macro:
			return NodeIcons["switch"];		//switch

		case NodeType::path:
			return NodeIcons["path"];		//path

		case NodeType::tunnel:
			return NodeIcons["io"];			//arrows

		case NodeType::spawn:
			return NodeIcons["spawn"];		//star

		//spawn

		case NodeType::route:
		case NodeType::variable:
		case NodeType::variableset:
		case NodeType::comment:
		case NodeType::bubble:
			return "";						//no icon needed

		default: 
			return NodeIcons["default"];	//the function icon
			break;
	}
}

FString UBlueprintDoxygenCommandlet::GetDelegateIcon(UEdGraphNode* node, bool *hasDelegate)
{
	const TArray< UEdGraphPin* > pins = node->Pins;
	//const TArray< UEdGraphPin* > pins = n->GetAllPins();

	*hasDelegate = false;

	for (UEdGraphPin* p : pins)
	{
		if (isDelegatePin(p))
		{
			*hasDelegate = true;

			FString dc = GetPinColor(p);
			if (p->HasAnyConnections())
				return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *dc, *PinIcons["delegate"].Value);
			else
				return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *dc, *PinIcons["delegate"].Key);
		}
	}

	return "&nbsp;";
}

bool UBlueprintDoxygenCommandlet::isDelegatePin(UEdGraphPin* pin)
{
	if (pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{	//if (p->PinType == FEdGraphPinType::
		if (GetPinType(pin) == "Delegate")
			return true;
	}

	return false;
}

FString UBlueprintDoxygenCommandlet::prepNodePortRows(FString prefix, UEdGraphNode* node)
{
	//both
	FString nodeTemplate_11 = R"PixoVR(<tr>
	<td colspan="2" align="left"  balign="left"  href="_PINURL_" title="_INTOOLTIP_"  port="_INPORT_"><font point-size="_FONTSIZEPORT_" color="_INCOLOR_">_INICON_</font> <font point-size="_FONTSIZEPORT_" color="_FONTCOLOR_">_INLABEL_</font>_HEIGHTSPACER__INVALUE_</td>
	<td colspan="2" align="right" balign="right" href="_PINURL_" title="_OUTTOOLTIP_" port="_OUTPORT_">_OUTVALUE__HEIGHTSPACER_<font point-size="_FONTSIZEPORT_" color="_FONTCOLOR_">_OUTLABEL_</font> <font point-size="_FONTSIZEPORT_" color="_OUTCOLOR_">_OUTICON_</font></td>
</tr>)PixoVR";

	//left
	FString nodeTemplate_10 = R"PixoVR(<tr>
	<td colspan="2" align="left" balign="left" href="_PINURL_" title="_INTOOLTIP_"  port="_INPORT_"><font point-size="_FONTSIZEPORT_" color="_INCOLOR_">_INICON_</font> <font point-size="_FONTSIZEPORT_" color="_FONTCOLOR_">_INLABEL_</font>_HEIGHTSPACER__INVALUE_</td>
	<td colspan="2"></td>
</tr>)PixoVR";

	FString nodeTemplate_01 = R"PixoVR(<tr>
	<td colspan="2"></td>
	<td colspan="2" align="right" balign="right" href="_PINURL_" title="_OUTTOOLTIP_" port="_OUTPORT_">_OUTVALUE__HEIGHTSPACER_<font point-size="_FONTSIZEPORT_" color="_FONTCOLOR_">_OUTLABEL_</font> <font point-size="_FONTSIZEPORT_" color="_OUTCOLOR_">_OUTICON_</font></td>
</tr>)PixoVR";

/*
	FString routeTemplate2 = R"PixoVR(<tr>
	<td port="port" href="_PINURL_" title="_NODECOMMENT_">&#11044;</td>
</tr>)PixoVR";
*/
	//&reg;
	//&#10122;
	FString routeTemplate = R"PixoVR(<tr>
	<td port="port" href="_PINURL_" title="_NODECOMMENT_">&#9899;</td>
</tr>)PixoVR";

	//min size 80
	FString variableTemplate = R"PixoVR(<tr>
	<td colspan="2"></td>
	<td colspan="2" width="80" align="right" balign="right" href="_PINURL_" title="_OUTTOOLTIP_" port="_OUTPORT_">_OUTVALUE__HEIGHTSPACER_<font point-size="_FONTSIZEPORT_" color="_FONTCOLOR_">_OUTLABEL_</font> <font point-size="_FONTSIZEPORT_" color="_OUTCOLOR_">_OUTICON_</font></td>
</tr>)PixoVR";

	//min size 80
	FString variableTemplate2 = R"PixoVR(<tr>
	<td colspan="2" width="40" align="left"  balign="left"  href="_PINURL_" title="_INTOOLTIP_"  port="_INPORT_"><font point-size="_FONTSIZEPORT_" color="_INCOLOR_">_INICON_</font> <font point-size="_FONTSIZEPORT_" color="_FONTCOLOR_">_INLABEL_</font>_HEIGHTSPACER__INVALUE_</td>
	<td colspan="2" width="40" align="right" balign="right" href="_PINURL_" title="_OUTTOOLTIP_" port="_OUTPORT_">_OUTVALUE__HEIGHTSPACER_<font point-size="_FONTSIZEPORT_" color="_FONTCOLOR_">_OUTLABEL_</font> <font point-size="_FONTSIZEPORT_" color="_OUTCOLOR_">_OUTICON_</font></td>
</tr>)PixoVR";

	FString variablesetTemplate = R"PixoVR(<tr>
	<td colspan="2" align="left"  balign="left"  href="_PINURL_" title="_INTOOLTIP_"  port="_INPORT_"><font point-size="_FONTSIZEPORT_" color="_INCOLOR_">_INICON_</font> <font point-size="_FONTSIZEPORT_" color="_FONTCOLOR_">_INLABEL_</font>_HEIGHTSPACER__INVALUE_</td>
	<td colspan="2" align="right" balign="right" href="_PINURL_" title="_OUTTOOLTIP_" port="_OUTPORT_">_HEIGHTSPACER_<font point-size="_FONTSIZEPORT_" color="_OUTCOLOR_">_OUTICON_</font></td>
</tr>)PixoVR";

	bool isRoute = GetNodeType(node, NodeType::node) == NodeType::route;
	bool isVariable = GetNodeType(node, NodeType::node) == NodeType::variable;
	bool isVariableset = GetNodeType(node, NodeType::node) == NodeType::variableset;
	bool isCompact = GetNodeType(node, NodeType::node) == NodeType::compact;
	bool isConnected = false;		//changed below
	bool otherIsRoute = false;		//updated below

	FString rows;

	vmap pindata;
	pindata.Add("_INPORT_", "");
	pindata.Add("_INICON_", "&nbsp;");
	pindata.Add("_INLABEL_", "&nbsp;");
	pindata.Add("_INCOLOR_", "_PINDEFAULTCOLOR_");
	pindata.Add("_INVALUE_", "");
	pindata.Add("_INTOOLTIP_", "");

	pindata.Add("_OUTPORT_", "");
	pindata.Add("_OUTICON_", "&nbsp;");
	pindata.Add("_OUTLABEL_", "&nbsp;");
	pindata.Add("_OUTCOLOR_", "_PINDEFAULTCOLOR_");
	pindata.Add("_OUTVALUE_", "");			// ...probably never used
	pindata.Add("_OUTTOOLTIP_", "");

	const TArray< UEdGraphPin* > pins = node->Pins;
	//const TArray< UEdGraphPin* > pins = n->GetAllPins();

	//TODO: addpin?
	/*
	IK2Node_AddPinInterface* n = Cast<IK2Node_AddPinInterface>(node);
	if (n && n->CanAddPin())
	{
		//add add pin on output, which is purely cosmetic.
		n->AddInputPin();
	}
	*/

	TArray< UEdGraphPin* > ins;
	TArray< UEdGraphPin* > outs;
	for (UEdGraphPin* p : pins)
	{
		//don't show hidden pins
		if (!PinShouldBeVisible(p))
			continue;

		if (p->Direction == EEdGraphPinDirection::EGPD_Input)
			ins.Add(p);
		else
			if (!isDelegatePin(p))
				outs.Add(p);
	}

	FString sname, sport, sside, dname, dport, dside, color, connection;
	FString rowTemplate;
	UEdGraphPin *i=NULL, *o=NULL;
	while (ins.Num() > 0 || outs.Num() > 0)
	{
		if (ins.Num())
		{
			i = ins[0];
			ins.RemoveAt(0);
		}
		else
			i = NULL;

		if (outs.Num())
		{
			o = outs[0];
			outs.RemoveAt(0);
		}
		else
			o = NULL;

		if (isRoute)
			rowTemplate = routeTemplate;
		else if (isVariable)
			rowTemplate = i ? variableTemplate2 : variableTemplate;
		else if (isVariableset)
			rowTemplate = variablesetTemplate;
		else if (i&&o)
			rowTemplate = nodeTemplate_11;
		else if (i&&!o)
			rowTemplate = nodeTemplate_10;
		else if (!i&&o)
			rowTemplate = nodeTemplate_01;
		else
			UE_LOG(LOG_DOT, Error, TEXT("No row in or out.. this should never happen."));

		if (i)
		{
			dport = GetPinPort(i);
			color = GetPinColor(i);
			isConnected = i->HasAnyConnections();

			pindata["_INPORT_"] = dport;
			pindata["_INICON_"] = GetPinIcon(i);
			pindata["_INLABEL_"] = isCompact ? "" : GetPinLabel(i);
			pindata["_INCOLOR_"] = color;
			pindata["_INVALUE_"] = isConnected ? "" : GetPinDefaultValue(i);
			pindata["_INTOOLTIP_"] = GetPinTooltip(i);

			//create connection(s)
			dname = i->GetOwningNode()->GetName();		//the source name, from this output
			for (UEdGraphPin* s : i->LinkedTo)
			{
				otherIsRoute = GetNodeType(s->GetOwningNode(), NodeType::node) == NodeType::route;
				sname = s->GetOwningNode()->GetName();	//the destination node, from this output
				sport = GetPinPort(s);

				sside = otherIsRoute	? "c" : "e";
				dside = isRoute			? "c" : "w";

				connection = FString::Printf(TEXT("%s:%s:%s -- %s:%s:%s"), *sname, *sport, *sside, *dname, *dport, *dside);
				PinConnections.Add(connection, color);
			}
		}
		else
		{
			pindata["_INPORT_"] = "";
			pindata["_INICON_"] = "&nbsp;";
			pindata["_INLABEL_"] = "&nbsp;";
			pindata["_INCOLOR_"] = "_PINDEFAULTCOLOR_";
			pindata["_INVALUE_"] = "";
			pindata["_INTOOLTIP_"] = "";
		}

		if (o)
		{
			sport = GetPinPort(o);
			color = GetPinColor(o);
			isConnected = o->HasAnyConnections();

			pindata["_OUTPORT_"] = sport;
			pindata["_OUTICON_"] = GetPinIcon(o);
			pindata["_OUTLABEL_"] = isCompact ? "" : GetPinLabel(o);
			pindata["_OUTCOLOR_"] = color;
			pindata["_OUTVALUE_"] = isConnected ? "" : GetPinDefaultValue(o);	//ever used?
			pindata["_OUTTOOLTIP_"] = GetPinTooltip(o);

			//create connection(s)
			sname = o->GetOwningNode()->GetName();		//the source name, from this output
			for (UEdGraphPin* d : o->LinkedTo)
			{
				otherIsRoute = GetNodeType(d->GetOwningNode(), NodeType::node) == NodeType::route;
				dname = d->GetOwningNode()->GetName();	//the destination node, from this output
				dport = GetPinPort(d);

				sside = isRoute			? "c" : "e";
				dside = otherIsRoute	? "c" : "w";

				connection = FString::Printf(TEXT("%s:%s:%s -- %s:%s:%s"), *sname, *sport, *sside, *dname, *dport, *dside);
				PinConnections.Add(connection, color);
			}
		}
		else
		{
			pindata["_OUTPORT_"] = "";
			pindata["_OUTICON_"] = "&nbsp;";
			pindata["_OUTLABEL_"] = "&nbsp;";
			pindata["_OUTCOLOR_"] = "_PINDEFAULTCOLOR_";
			pindata["_OUTVALUE_"] = "";
			pindata["_OUTTOOLTIP_"] = "";
		}

		rows += prepTemplateString("", pindata, rowTemplate)+"\n";
	}

	rows.TrimEndInline();

	rows = prepTemplateString(prefix, BlueprintStyle, rows);

	return rows;
}

void UBlueprintDoxygenCommandlet::writeNodeBody(FString prefix, UEdGraphNode* n)
{
	//should we be casting to UK2Node instead of using UEdGraphNode?

	FString nodename = n->GetName();

	NodeType type = GetNodeType(n, NodeType::node);
	FString typeGroup = GetNodeTypeGroup(type);
	FString url = GetNodeURL(n);
	FString tooltip = GetNodeTooltip(n);
	FString title = n->GetNodeTitle(ENodeTitleType::ListView).ToString();
	FString title2 = "";

	UEdGraphNode_Comment* comment = dynamic_cast<UEdGraphNode_Comment*>(n);
	FLinearColor titleColor = (comment) ? comment->CommentColor : n->GetNodeTitleColor();
	int commentSize = (comment) ? comment->GetFontSize() : 18;

	//if (type == NodeType::composite)
	{
		FString ftitle = n->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		ftitle = ftitle.Replace(TEXT("\r"),TEXT(""));
		ftitle.Split(TEXT("\n"), &title, &title2);
		if (!title2.IsEmpty())
			title2 = "<font point-size=\"_FONTSIZE2_\">" + title2 + "</font>";
	}

	if (type == NodeType::compact)
	{
		int titleLen = title.Len();		//before compact is htmlentities-ed

		UK2Node* n2 = dynamic_cast<UK2Node*>(n);
		if (n2)
		{
			//titleColor = n2->GetNodeTitleColor();
			//GetPinHoverText

			if (n2->ShouldDrawCompact())
			{
				FString compact = n2->GetCompactNodeTitle().ToString();
				titleLen = compact.Len();
				title = htmlentities(compact);				// dot/graphviz doesn't like different char encodings.
			}
		}

		//wcout << "COMPACT: " << *title << " - " << titleLen << endl;

		if (titleLen > 2)
		{
			int charWidth = 10;
			BlueprintStyle.Add("_COMPACTSIZE_", "17");
			BlueprintStyle.Add("_COMPACTWIDTH_", FString::Printf(TEXT("%d"), 70 + charWidth * titleLen));
		}
		else
		{
			BlueprintStyle.Add("_COMPACTSIZE_", "36");
			BlueprintStyle.Add("_COMPACTWIDTH_", "100");
		}
	}

	if (type == NodeType::variableset)
	{
		BlueprintStyle.Add("_COMPACTSIZE_", "16");
		BlueprintStyle.Add("_COMPACTWIDTH_", "90");
	}

	float posx = (float)n->NodePosX / _dpi;
	float posy = (float)n->NodePosY / -_dpi;						// y is inverted
	float width = (float)n->NodeWidth * _scale;
	float height = (float)n->NodeHeight * _scale;

	TArray<FString> lines;											// we throw this away
	FString comment_ = n->NodeComment.TrimStartAndEnd();
	float numLines = comment_.ParseIntoArrayLines(lines, false);	// for bubble
	comment_ = comment_.Replace(TEXT("\r"), TEXT(""));
	comment_ = comment_.Replace(TEXT("\n"), TEXT(" <br/>"));		// after counting lines

	bool hasDelegate = false;

	BlueprintStyle.Add("_NODENAME_", nodename);
	BlueprintStyle.Add("_NODEGUID_", n->NodeGuid.ToString());
	BlueprintStyle.Add("_NODEICON_", GetNodeIcon(n));
	BlueprintStyle.Add("_NODEDELEGATE_", GetDelegateIcon(n,&hasDelegate));	//TODO: add node delegate tooltip
	BlueprintStyle.Add("_NODETITLE_", title);
	BlueprintStyle.Add("_NODETITLE2_", title2);
	//BlueprintStyle.Add("_NODECOLOR_", createColorString(n->GetNodeBodyTintColor()));
	BlueprintStyle.Add("_NODECOMMENT_", comment_);
	BlueprintStyle.Add("_POS_", FString::Printf(TEXT("%0.2f,%0.2f!"), posx, posy));
	BlueprintStyle.Add("_WIDTH_", FString::Printf(TEXT("%0.2f"), width));
	BlueprintStyle.Add("_HEIGHT_", FString::Printf(TEXT("%0.2f"), height));
	BlueprintStyle.Add("_TOOLTIP_", tooltip);
	BlueprintStyle.Add("_HEADERCOLOR_", createColorString(titleColor));
	BlueprintStyle.Add("_HEADERCOLORDIM_", createColorString(titleColor * 0.5f, 1.0f, 1.0f));
	BlueprintStyle.Add("_HEADERCOLORLIGHT_", createColorString(titleColor, 1.0f, 3.0f));
	BlueprintStyle.Add("_HEADERCOLORTRANS_", createColorString(titleColor, 0.5f));
	BlueprintStyle.Add("_CLASS_", typeGroup);
	BlueprintStyle.Add("_URL_", url);				//URL = "\ref SomeSubgraph"
	BlueprintStyle.Add("_PORTROWS_", prepNodePortRows(prefix+_tab+_tab,n));
	BlueprintStyle.Add("_FONTSIZECOMMENT_", FString::FromInt(commentSize));

	FString nodeTemplate = getNodeTemplate(type, hasDelegate);

	*out << *prepTemplateString(prefix + _tab, BlueprintStyle, nodeTemplate) << endl;

	//if a comment is visible, add it as a node and connect the arrow
	if (n->bCommentBubbleVisible)
	{
		float lineHeight = 22.0f;
		float margin = 29.0f;

		//float cposx = posx + (type==NodeType::route ? -.228f : 0.0f);	//&reg;
		float cposx = posx + (type == NodeType::route ? -.226f : 0.0f);	//&#10752;
		float cposy = posy + ((numLines * lineHeight) + margin) / _dpi;

		BlueprintStyle.Add("_POS_", FString::Printf(TEXT("%0.2f,%0.2f!"), cposx, cposy));

		FString commentString = getNodeTemplate(NodeType::bubble);
		//FString connection = FString::Printf(TEXT("%s %s [ color=\"_NODECOLOR_\" layer=\"edges\" arrowhead=\"normal\" direction=\"forward\" penwidth=\"5\" ];"), *c.Key, *c.Value);

		*out << *prepTemplateString(prefix + _tab, BlueprintStyle, commentString) << endl;
		//*out << *prefix << *_tab << *connection << endl;		//don't need an edge here now because of the arrow/triangle
	}
}

