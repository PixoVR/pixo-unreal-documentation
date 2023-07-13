// (c) 2023 PixoVR

#pragma once

#include "blueprintReporter.h"

#include "Runtime/Launch/Resources/Version.h"

blueprintReporter::blueprintReporter(FString _outputDir, FString _stylesheet, FString _groups)
: reporter("blueprints", _outputDir, _stylesheet, _groups)
{
	//BlueprintBaseClassName
	loadAssets(UBlueprint::StaticClass()->GetFName());
}

void blueprintReporter::report(int &graphCount, int &ignoredCount, int &failedCount)
{
	LOG( "Parsing Blueprints..." );
	//wcout << "Parsing Blueprints..." << endl;

	for (FAssetData const& Asset : assetList)
	{
		if (shouldReportAsset(Asset))
		{
#if ENGINE_MAJOR_VERSION >= 5
			FString const AssetPath = Asset.GetObjectPathString();// .ObjectPath.ToString();
#else
			FString const AssetPath = Asset.ObjectPath().ToString();
#endif

			//FString const AssetName = Asset.AssetName.ToString();
			//FString const PackagePath = Asset.PackagePath.ToString();

			//Load with LOAD_NoWarn and LOAD_DisableCompileOnLoad.
			UBlueprint* LoadedBlueprint = Cast<UBlueprint>(StaticLoadObject(Asset.GetClass(), /*Outer =*/nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_DisableCompileOnLoad));
			if (LoadedBlueprint == nullptr
				|| !LoadedBlueprint->IsValidLowLevel()
				|| !LoadedBlueprint->ParentClass->IsValidLowLevel()
				)
			{
				failedCount++;
				wcerr << "Failed to load: " << *AssetPath << endl;
				continue;
			}
			else
			{
				int r = reportBlueprint(_tab, LoadedBlueprint);

				if (r)
					graphCount += r;
				else
					return;
			}
		}
		else
			ignoredCount++;
	}

	reportGroup(
		"blueprints",
		"Blueprints",
		"Blueprints found in the **$(PROJECT_NAME)** library.",
		"	The blueprint system may contain multiple graph classes(Event Graph, Construction Graph, etc)\n"
		"	per blueprint, and each graph may contain subgraphs.  Links to subgraphs can be accessed by\n"
		"	clicking on their collapsed node or by navigating to the additional entries in the class."
	);
}

int blueprintReporter::reportBlueprint(FString prefix, UBlueprint* blueprint)
{
	const UPackage* package = blueprint->GetPackage();
	FString className = getClassName(blueprint->GeneratedClass);
	FString path, subDir;

	//TODO is this useful?
	//bool isLevelScriptBlueprint = FBlueprintEditorUtils::IsLevelScriptBlueprint(blueprint);
	//bool isParentClassABlueprint = FBlueprintEditorUtils::IsParentClassABlueprint(blueprint);

	//each blueprint will have different graphs.  This is just to save some memory on a large build.
	GraphDescriptions.Empty();

	//each graph will make calls to nodes and variables.  Clear these before we report each blueprint.
	GraphCalls.Empty();

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
			return -1;
		}
	}

	path = currentDir + "/" + className + ".h";
	FPaths::MakePlatformFilename(path);				//clean slashes
	if (!openFile(path))
	{
		UE_LOG(LOG_DOT, Error, TEXT("Could not open '%s' for writing."), *path);
		return -1;
	}

	TArray<UEdGraph*> graphs;
	blueprint->GetAllGraphs(graphs);

	writeBlueprintHeader(blueprint, "blueprints", "Blueprint", package->GetName(), graphs.Num());
	writeBlueprintMembers(blueprint, "Blueprint");

	if (graphs.Num())
	{
//		*out << "	/**" << endl;
//		*out << "	\\name Blueprint Graphs" << endl;
//		//*out << "	\\privatesection" << endl;
//		*out << "	*/" << endl;
//		*out << "	///@{" << endl;
	}

	for (UEdGraph* g : graphs)
		reportGraph(prefix, g);

	if (graphs.Num())
	{
//		*out << "	///@}" << endl;
	}

	writeAssetFooter();

	closeFile();		//close .h file

	path = currentDir + "/" + className + ".cpp";
	FPaths::MakePlatformFilename(path);				//clean slashes
	if (!openFile(path))
	{
		UE_LOG(LOG_DOT, Error, TEXT("Could not open '%s' for writing."), *path);
		return -1;
	}

	writeAssetCalls(className);

	closeFile();		//close .cpp file

	return graphs.Num();
}

void blueprintReporter::reportGraph(FString prefix, UEdGraph* g)
{
	pinConnections.Empty();		//clear out all connections for each new graph

	writeGraphHeader(prefix, g, "Blueprint");

	for (UEdGraphNode* n : g->Nodes)
		reportNode(prefix + _tab, n);

	writeGraphConnections(prefix);
	writeGraphFooter(prefix, g);

	//TODO: local variables for a function (graph)
	//
	// this is very specific to functions,
	// and we don't really have a place to put them
	// because functions are members, which cannot
	// contain other members.
	//
	// if n is UK2Node_Function (Entry, etc)
	//	//find local variables
	//	FBlueprintEditorUtils::FindLocalVariable
	//	FBlueprintEditorUtils::GetLocalVariablesOfType
	//		cast to UK2Node_FunctionEntry (node)
	//		node->LocalVariables
	//		//iterate from there
}

void blueprintReporter::reportNode(FString prefix, UEdGraphNode* n)
{
	//FString type = n->GetSchema
	FString type = n->GetClass()->GetFName().ToString();

	//add the subgraph node's description here, for when we are parsing that graph
	UK2Node_Composite* subgraph_node = dynamic_cast<UK2Node_Composite*>(n);
	//if (type == "K2Node_Composite")
	if (subgraph_node)
	{
		UEdGraph *subgraph = subgraph_node->BoundGraph;
		//FString brief = subgraph_node->GetDesc().TrimStartAndEnd();
		//FString brief = subgraph_node->GetTooltipText().ToString().TrimStartAndEnd();
		FString brief = FBlueprintEditorUtils::GetGraphDescription(subgraph).ToString();
		brief = brief.Replace(TEXT("\r"), TEXT(""));
		//brief = brief.Replace(TEXT("\n"), TEXT("&#013;"));
		brief = brief.Replace(TEXT("\n"), TEXT(" "));

		GraphDescriptions.Add(subgraph, brief);
	}

	writeNodeBody(prefix, n);
}

void blueprintReporter::writeBlueprintHeader(
	UBlueprint *blueprint,
	FString group,
	FString qualifier,
	FString packageName,
	int graphCount
)
{
	FString className = getClassName(blueprint->GeneratedClass);
	FString parentClass = getClassName(blueprint->ParentClass);

	bool isDataOnly = FBlueprintEditorUtils::IsDataOnlyBlueprint(blueprint);

	FString imagetag = "";
	if (outputDir != "-")
	{
		//FGenericPlatformMisc::GetDefaultPathSeparator()
		FString pngName = className + ".png";
		FString pngPath = currentDir + "\\" + pngName;
		FPaths::MakePlatformFilename(pngPath);

		FString tpngPath = pngPath;
		tpngPath.RemoveFromStart(outputDir);

		if (!createThumbnailFile(blueprint, pngPath))
		{
			tpngPath = "[OutputDir]" + tpngPath;
			//LOG( " (missing) " + tpngPath );
			//wcout << " (missing) " << *tpngPath << endl;
		}
		else
		{
			int width = 256;
			FString fmt;

			fmt = "\\image{inline} html {0} \"{1}\" width={2}px";
			//imagetag = FString::Format(*fmt, { pngName, className, width });
			imagetag = FString::Format(*fmt, { tpngPath, className, width });

			// \link ABP_ActorTest_C &thinsp; \image html ABP_ActorTest_C.png "ABP_ActorTest_C" width=256px \endlink
			fmt = "\\link {1} &thinsp; \\image html {0} \"{1}\" width={2}px \\endlink";
			//FString galleryEntry = FString::Format(*fmt, { pngName, className, width });
			FString galleryEntry = FString::Format(*fmt, { tpngPath, className, width });
			GalleryList.Add(galleryEntry);
		}
	}

	// Gives the line places to break if needed.
	// TODO: (actually turns out doxygen won't do it, so left this here with
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
	FString config = getTrimmedConfigFilePath(blueprint->GetDefaultConfigFilename());

	FString description;
	//description += "	This class " + blueprint->GetDesc() + ".\n";
	//description += "\n";
	description += blueprint->BlueprintDescription;
	description = description.TrimStartAndEnd();
	description = description.Replace(TEXT("\n"), TEXT("\n	"));

	*out << "#pragma once" << endl;
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
		*out << "	<br/>This blueprint is <b>Data Only</b>" << endl;
	if (!description.IsEmpty())
	{	*out << "	" << endl;
		*out << "	" << *description << endl;
	}

	//*out << "	<div style='clear:both;'/>" << endl;	//now a css thing elsewhere

	*out << "	\\headerfile " << *className << ".h \"" << *packageName << "\"" << endl;
	*out << "*/" << endl;
	*out << "class " << *className << " : public " << *parentClass << endl;
	*out << "{" << endl;
}

void blueprintReporter::writeBlueprintMembers(UBlueprint* blueprint, FString what)
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

	TArray<FProperty*> variables;

	TSet<FName> varnames;
	FBlueprintEditorUtils::GetClassVariableList(blueprint, varnames, false);
	//FBlueprintEditorUtils::GetClassVariableList(blueprint, varnames, true);
	FBPVariableDescription *vd = NULL;
	for (FName n : varnames)
	{
		//wcout << "MM " << *n.ToString() << endl;

		FProperty* fp = blueprint->GeneratedClass->FindPropertyByName(n);
		if (fp)
		{
		//	//const FBlueprintNodeSignature::FSignatureSet *meta = fp->GetMetaDataMap();
		//	const TMap<FName, FString>* meta = fp->GetMetaDataMap();
		//	for (TTuple<FName, FString> m : *meta)
		//	{	wcout << "meta: " << *m.Key.ToString() << " => " << *m.Value << endl;	}

			FString category = fp->GetMetaData("Category");
			//wcout << " :: " << *category << endl;
			if (category == "Default")
				variables.AddUnique(fp);
		}
		//else
		//	wcout << " :: [NO FP!]" << endl;
	}

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

	// this may no longer be needed...
	// variables new to this class (added to its parent class)
	TArray<FBPVariableDescription> vars = blueprint->NewVariables;
	for (FBPVariableDescription v : vars)
	{
		FProperty* fp = blueprint->GeneratedClass->FindPropertyByName(v.VarName);
		variables.AddUnique(fp);
	}

	for (FProperty *fp : variables)
	{
		/*
		//const FBlueprintNodeSignature::FSignatureSet *meta = fp->GetMetaDataMap();
		const TMap<FName, FString>* meta = fp->GetMetaDataMap();
		for (TTuple<FName, FString> m : *meta)
		{	wcout << "meta: " << *m.Key.ToString() << " => " << *m.Value << endl;	}
		*/

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

		//FString varName = createVariableName(v.VarName.ToString());
		FString variableName = createVariableName(fp->GetNameCPP());
		//FString friendlyName = v.FriendlyName;
		FString friendlyName = fp->GetMetaData("DisplayName");

		//FString category = v.Category.ToString();
		FString category = fp->GetMetaData("Category");

		FString type = getCppType(fp);

		//FString initializer = v.DefaultValue;
		FString initializer = "";
		//if (isObject)
		{
			//TODO defaults!
			//fp->GetCPPType();
			//fp->GetCPPMacroType();
			//fp->GetCPPTypeForwardDeclaration();
			//fp->ContainerPtrToValuePtrForDefaults(
			//fp->ContainerPtrToValuePtr(
		}

		// Can't seem to find a description that doesn't include the "Varname: description" format.
		// we want just description without the varname.
		FString description = fp->GetToolTipText(true).ToString();
		//FString description = fp->GetMetaData(TEXT("ShortToolTip"));

		FString descriptionV, descriptionD;
		description.Split(TEXT(":"), &descriptionV, &descriptionD);
		if (!descriptionD.IsEmpty())
			description = descriptionD.TrimStartAndEnd();
		if (description.IsEmpty())
			description = friendlyName;
		description = description.Replace(TEXT("\r"), TEXT(""));
		description = description.Replace(TEXT("\n"), TEXT(" "));
		description = description.Replace(TEXT("\\"), TEXT("\\\\"));
		//wcout << "DESCRIPTION: " << *description << endl;

		bool isPrivate = true;
		bool isPublic = false;

		//uint64 flags = v.PropertyFlags;
		EPropertyFlags flags = fp->PropertyFlags;
		bool isReadonly = bool(flags & EPropertyFlags::CPF_BlueprintReadOnly);
		bool isConst = bool (flags & EPropertyFlags::CPF_ConstParm);
		bool isInstanceEditable = bool(!(flags & EPropertyFlags::CPF_DisableEditOnInstance));
		bool isConfigSaved = bool (flags & EPropertyFlags::CPF_Config);

		//TODO: get more data	UEdGraphNode::GetDeprecationResponse
		bool isDeprecated = bool(flags & EPropertyFlags::CPF_Deprecated);
		FString sDeprecated = fp->GetMetaData("DeprecationMessage");
		sDeprecated = sDeprecated.Replace(TEXT("\r"), TEXT(""));
		sDeprecated = htmlentities(sDeprecated);
		if (sDeprecated.IsEmpty()) sDeprecated = "This variable will be removed in future versions.";

		//uint32 rflags = v.ReplicationCondition;
		ELifetimeCondition rflags = fp->GetBlueprintReplicationCondition();
		bool isReplicated = rflags > 0;				//TODO: there's much more with ELifetimeCondition::*

		//wcout << std::bitset<64>(flags) << endl;
		//wcout << std::bitset<32>(rflags) << endl;

		isPublic = isInstanceEditable;				//this is probably incorrect or incomplete
		isPrivate = isReadonly;						//this is probably incorrect or incomplete

		//isPrivate = flags & EPropertyFlags::CPF_NativeAccessSpecifierPrivate;

		//FString e = FString::Printf(TEXT("%s%s %s = \"%s\";\t//!< %s"),
		//	isConst ? TEXT("const ") : TEXT(""),
		//	*type,
		//	*variableName,
		//	*initializer,
		//	*description
		//);

		FString e = FString::Printf(TEXT("%s%s %s;\t//!< %s"),
			isConst ? TEXT("const ") : TEXT(""),
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

void blueprintReporter::LOG(FString message)
{
	LOG("Display",message);
}

void blueprintReporter::LOG(FString verbosity, FString message)
{
	if (verbosity == "Display")
	{
		wcout << *message << endl;
	}
	else if (verbosity == "Warning")
	{
		//for messages like "Ignoring..."
		wcout << "WARNING: " << *message << endl;
	}
	else if (verbosity == "Error")
	{
		wcerr << "ERROR: " << *message << endl;
	}
	else
	{
		wcout << *message << endl;
	}
}
