// (c) 2023 PixoVR

#pragma once

#include "materialReporter.h"

#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "MaterialGraph/MaterialGraphNode_Composite.h"
#include "MaterialGraph/MaterialGraphNode_PinBase.h"
#endif

#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionComment.h"

materialReporter::materialReporter(FString _outputDir, FString _stylesheet, FString _groups)
: reporter("materials", _outputDir, _stylesheet, _groups)
{
	//MaterialBaseClassName
#if ENGINE_MAJOR_VERSION >= 5
	loadAssetsByPath(FTopLevelAssetPath(UMaterialInterface::StaticClass()->GetPathName()));
#else
	loadAssets(UMaterialInterface::StaticClass()->GetFName());
#endif
}

void materialReporter::report(int &graphCount, int &ignoredCount, int &failedCount)
{
	LOG( "Parsing Materials..." );
	//wcout << "Parsing Materials..." << endl;

	for (FAssetData const& Asset : assetList)
	{
		if (shouldReportAsset(Asset))
		{
#if ENGINE_MAJOR_VERSION >= 5
			FString const AssetPath = Asset.GetObjectPathString(); // .ObjectPath.ToString();
#else
			FString const AssetPath = Asset.ObjectPath.ToString();
#endif
			//FString const AssetName = Asset.AssetName.ToString();
			//FString const PackagePath = Asset.PackagePath.ToString();

			//Load with LOAD_NoWarn and LOAD_DisableCompileOnLoad.
			UMaterialInterface * LoadedMaterial = Cast<UMaterialInterface>(StaticLoadObject(Asset.GetClass(), /*Outer =*/nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_DisableCompileOnLoad));
			//UMaterialInterface* LoadedMaterial = Cast<UMaterialInterface>(StaticLoadObject(Asset.GetClass(), /*Outer =*/nullptr, *AssetPath, nullptr, LOAD_None));
			if (LoadedMaterial == nullptr
				|| !LoadedMaterial->IsValidLowLevel()
				|| !LoadedMaterial->GetClass()->IsValidLowLevel()
				)
			{
				failedCount++;
				wcerr << "Failed to load: " << *AssetPath << endl;
				continue;
			}
			else
			{
				int r = reportMaterial(_tab, LoadedMaterial);

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
		"materials",	//groupname
		"Materials",	//cosmetic groupname
		"Materials found in the **$(PROJECT_NAME)** library.",		//group brief
		"	The material system allows for subgraphs within a material node network.\n"
		"	Links to subgraphs can be accessed by clicking on their collapsed node or by navigating to the\n"
		"	additional entries in the class."
	);
}

int materialReporter::reportMaterial(FString prefix, UMaterialInterface* materialInterface)
{
	currentMaterialInterface = materialInterface;
	const UPackage* package = materialInterface->GetPackage();
	UMaterial* material = materialInterface->GetMaterial();
	FString className = materialInterface->GetName();
	FString path, subDir;
	FString imageTag = "";

	//each material will have different graphs.  This is just to save some memory on a large build.
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
			UE_LOG(LOG_DOT, Error, TEXT("Could not create folder: %s"), *currentDir);
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

	//create image, if possible
	FString pngName = className + ".png";
	FString pngPath = currentDir + "\\" + pngName;
	FPaths::MakePlatformFilename(pngPath);

	FString tpngPath = pngPath;
	tpngPath.RemoveFromStart(outputDir);

	if (!createThumbnailFile(materialInterface, pngPath))
	{
		//tpngPath = "[OutputDir]" + tpngPath;
		//LOG( " (missing) " + tpngPath );
		//wcout << " (missing) " << *tpngPath << endl;
	}
	else
	{
		int width = 256;
		FString fmt;

		fmt = "\\image{inline} html {0} \"{1}\" width={2}px";
		//imageTag = FString::Format(*fmt, { pngName, className, width });
		imageTag = FString::Format(*fmt, { tpngPath, className, width });

		// \link ABP_ActorTest_C &thinsp; \image html ABP_ActorTest_C.png "ABP_ActorTest_C" width=256px \endlink
		fmt = "\\link {1} &thinsp; \\image html {0} \"{1}\" width={2}px \\endlink";
		//FString galleryEntry = FString::Format(*fmt, { pngName, className, width });
		FString galleryEntry = FString::Format(*fmt, { tpngPath, className, width });
		GalleryList.Add(galleryEntry);
	}

	TArray<EdGraphPtr> graphs;

	//UEdGraph* graph = material->MaterialGraph;
	UMaterialGraph* graph = material->MaterialGraph;
	if (!graph)
	{
		//graph = material->MaterialGraph = CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(material, NAME_None, UMaterialGraph::StaticClass(), UMaterialGraphSchema::StaticClass()));
		graph = material->MaterialGraph = CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(material, FName(*className), UMaterialGraph::StaticClass(), UMaterialGraphSchema::StaticClass()));
		material->MaterialGraph->Material = material;
		//material->MaterialGraph->RootNode = material->root						//NO?
		//material->MaterialGraph->MaterialFunction = material->materialFunction;	//NOT POSSIBLE!
		material->MaterialGraph->RebuildGraph();
		//material->MaterialGraph->RootNode->ReconstructNode();
		//material->MaterialGraph->RootNode->CreateInputPins();						//will duplicate pins!
		//graph->OriginalMaterialFullName;
	}

	graphs.Add(graph);
	addAllGraphs(graphs, graph->SubGraphs);

	writeMaterialHeader(materialInterface, "materials", "Material", package->GetName(), imageTag, graphs.Num());
	writeMaterialMembers(material, "Material");

	if (graphs.Num())
	{
//		*out << "	/**" << endl;
//		*out << "	\\name Material Graphs" << endl;
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

	currentMaterialInterface = NULL;

	return graphs.Num();
}

void materialReporter::reportGraph(FString prefix, UEdGraph* g)
{
	pinConnections.Empty();		//clear out all connections for each new graph

	writeGraphHeader(prefix, g, "Material");

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

void materialReporter::reportNode(FString prefix, UEdGraphNode* n)
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

#if ENGINE_MAJOR_VERSION >= 5
	UMaterialGraphNode_Composite* subgraph_materialnode = dynamic_cast<UMaterialGraphNode_Composite*>(n);
	if (subgraph_materialnode)
	{
		UEdGraph* subgraph = subgraph_materialnode->BoundGraph;
		//FString brief = subgraph_materialnode->GetDesc().TrimStartAndEnd();
		FString brief = subgraph_materialnode->GetTooltipText().ToString().TrimStartAndEnd();
		TArray<FString> tips;
		subgraph_materialnode->MaterialExpression->GetExpressionToolTip(tips);
		if (tips.Num() > 1)
			brief = tips[1].TrimStartAndEnd();
		brief = brief.Replace(TEXT("\r"), TEXT(""));
		//brief = brief.Replace(TEXT("\n"), TEXT("&#013;"));
		brief = brief.Replace(TEXT("\n"), TEXT(" "));

		GraphDescriptions.Add(subgraph, brief);
	}
#endif

	writeNodeBody(prefix, n);
}

FString materialReporter::getGraphCPP(UEdGraph* graph, FString _namespace)
{
	FString graphName = graph->GetName();
	FString graphNameVariable = createVariableName(graphName);
	//FString graphNameHuman = FName::NameToDisplayString(graphName, false);

	//FString type = g->GetClass()->GetSuperClass()->GetFName().ToString();
	//FString type = getClassName(graph->GetOuter()->GetClass());
	FString type = getClassName(graph->GetClass());
	type = "void";

	if (currentMaterialInterface)
	{
		UMaterialGraph* mg = dynamic_cast<UMaterialGraph*>(graph);
		UMaterial* mat = mg ? mg->Material : NULL;

		if (mat->MaterialGraph == graph)
		{
			//UMaterialInstance* mi = mat ? dynamic_cast<UMaterialInstance*>(mat) : NULL;
			//if (mi && currentMaterialInterface)
			//{
				graphName = currentMaterialInterface->GetName();
				graphNameVariable = createVariableName(graphName);
			//}
		}
	}

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

bool materialReporter::getNodeHasBubble(UEdGraphNode *node)
{
	bool hasBubble = reporter::getNodeHasBubble(node);

	UMaterialGraphNode* mn = dynamic_cast<UMaterialGraphNode*>(node);
	hasBubble |= mn && mn->MaterialExpression->bCommentBubbleVisible;
	hasBubble &= (node->GetClass()->GetFName() != "MaterialGraphNode_Comment");	// a comment can't show a comment

	return hasBubble;
}

TMap<FString, FString> materialReporter::getVisiblePins(UEdGraphNode* node)
{
	TMap<FString, FString> visiblePins;								// if empty, all pins are allowed.  Otherwise only these pins are allowed.  This is set during node reporting.
	UMaterialGraphNode_Root* materialRoot = dynamic_cast<UMaterialGraphNode_Root*>(node);
	if (materialRoot)
	{
		//filter parameters of this node/material/class
		UMaterial* material = materialRoot->Material;
		TArray<FMaterialInputInfo> minfo = material->MaterialGraph->MaterialInputs;
		for (FMaterialInputInfo info : minfo)
		{
			//wcout << *info.GetName().ToString() << " :: " << (info.IsVisiblePin(material) ? "true" : "false") << " :: " << *info.GetToolTip().ToString() << endl;
			if (info.IsVisiblePin(material))
			{
				//UE4
				TPair<FString, FString> v(info.GetName().ToString(), info.GetToolTip().ToString());
				visiblePins.Add(v);

				//UE5
				//visiblePins.Add({ info.GetName().ToString(), info.GetToolTip().ToString() });
			}
		}
	}

	return visiblePins;
}

FString materialReporter::getNodeTitle(UEdGraphNode *n, FString &title2)
{
	FString title = reporter::getNodeTitle(n,title2);

	//UMaterialGraphNode* mn = dynamic_cast<UMaterialGraphNode*>(n);
	UMaterialGraphNode_Root* materialRoot = dynamic_cast<UMaterialGraphNode_Root*>(n);
	if (materialRoot)
	{
		//title = mn ? mn->MaterialExpression->GetName() : currentMaterialInterface->GetName();
		title = currentMaterialInterface->GetName();
	}

	return title;
}

void materialReporter::writeMaterialHeader(
	UMaterialInterface* materialInterface,
	FString group,
	FString qualifier,
	FString packageName,
	FString imageTag,
	int graphCount
)
{
	UMaterial* material = materialInterface->GetMaterial();
	UMaterialInstanceDynamic* materialInstance = dynamic_cast<UMaterialInstanceDynamic *>(material);
	FString className = materialInterface->GetName();

	UClass* parent;
	if (materialInstance)
	{
		UMaterialInterface *p = materialInstance->Parent;
		parent = p->GetClass();
		//parent = materialInstance->GetClass();
	}
	else
	{
		//UClass* parent = material->GetBaseMaterial()->GetClass();
		parent = material->GetClass();
	}

	FString parentClass = getClassName(parent);
	//FMaterialInheritanceChain chain;
	//material->GetMaterialInheritanceChain(chain);

	// Gives the line places to break if needed.
	// TODO: (actually turns out doxygen won't do it, so left this here with
	// &thinsp; for when I do figure out how to do it with &#8203;)
	FString packageNameBreaks = packageName.Replace(TEXT("/"), TEXT("&thinsp;/"));
	//packageName = packageName.Replace(TEXT("/"), TEXT("&zwnj;/"));

	bool isDeprecated = false;
	FString sDeprecated = "This material will be removed in future versions.";

	// The (default) config file where public values can be saved/referenced.
	FString config = getTrimmedConfigFilePath(material->GetDefaultConfigFilename());

	// material domain
	FString domain;
	UEnum::GetValueAsString(material->MaterialDomain,domain);
	domain.RemoveFromStart("MD_");

	// TODO: no information is available as provided by the user/package.
	// the only place I found is as a rootNode comment bubble, but it always seems empty.
	FString description;
	description += material->GetDesc() + "\n";
	//description += "	" + material->GetDetailedInfo();
	description += "	" + material->MaterialGraph->RootNode->NodeComment;
	description = description.TrimStartAndEnd();

	*out << "#pragma once" << endl;
	*out << "/**" << endl;
	//*out << "	\\class " << *className << endl;
	if (!qualifier.IsEmpty())
		*out << "	\\qualifier " << *qualifier << endl;
	*out << "	\\ingroup " << *group << endl;

	//if (isDeprecated)
	//	*out << "	\\deprecated " << *sDeprecated << endl;

	*out << "	\\brief A material with " << graphCount << " graphs." << endl;
	*out << endl;

	if (!imageTag.IsEmpty())
		*out << "	" << *imageTag << endl;

	*out << "	UDF Path: <b>" << *packageNameBreaks << "</b>" << endl;
	*out << "	<br/>Config: <b>" << *config << "</b>" << endl;
	*out << "	<br/>Domain: <b>" << *domain << "</b>" << endl;

	if (!description.IsEmpty())
	{
		*out << "	" << endl;
		*out << "	" << *description << endl;
	}

	//*out << "	<div style='clear:both;'/>" << endl;	//now a css thing elsewhere

	*out << "	\\headerfile " << *className << ".h \"" << *packageName << "\"" << endl;
	*out << "*/" << endl;
	*out << "class " << *className << " : public " << *parentClass << endl;
	*out << "{" << endl;
}

void materialReporter::writeMaterialMembers(UMaterial* material, FString what)
{
	//TODO: this needs a cleanup!

	//need:
	// private/public/protected			[X] Always public
	// type								[x] Kinda, through the enum
	// name								[X]
	// default value (if not empty)		[ ] Can't get the syntax
	// detail/description				[X]
	// category							[X] sort by this

	TMap<FString, TMap<FString, TArray<FString> > > members;	//yeah man!

	members.Add("public", TMap<FString, TArray<FString> >());
	members.Add("protected", TMap<FString, TArray<FString> >());
	members.Add("private", TMap<FString, TArray<FString> >());

	material->BuildEditorParameterList();
	TMap<FName, TArray<UMaterialExpression*> > params = material->EditorParameters;
	for (TTuple<FName, TArray<UMaterialExpression*> > pn : params)
	{
		//wcout << "ASSET: " << *pn.Key.ToString() << endl;

		FString friendlyName = pn.Key.ToString();
		FString variableName = createVariableName(pn.Key.ToString());
		FString type;
		FString category;
		FString description;
		FString initializer;

		TArray<UMaterialExpression*> exps = pn.Value;
		if (exps.Num())
		{
			UMaterialExpression* exp = exps[0];

#if ENGINE_MAJOR_VERSION >= 5
			EMaterialParameterType ptype = exp->GetParameterType();
			//UEnum::GetValueAsString(ptype, type);
#else
			EMaterialParameterType ptype = EMaterialParameterType::Scalar;
			//FMaterialCachedExpressionData cexp = exp->Material->GetCachedExpressionData();
			//cexp.Parameters.GetParameterTypeEntry(
			//FMaterialParameterInfo Meta;
			//exp->GetParameterValue(Meta)
			//EMaterialParameterType ptype = Meta.Value.Type;
#endif
			switch (ptype)
			{
			case EMaterialParameterType::Scalar:
				type = "float";		break;

			case EMaterialParameterType::Vector:
				type = "Vector3d";	break;

#if ENGINE_MAJOR_VERSION >= 5
			case EMaterialParameterType::DoubleVector:
#endif
			case EMaterialParameterType::Texture:
			case EMaterialParameterType::StaticComponentMask:
			case EMaterialParameterType::RuntimeVirtualTexture:
				type = "Texture2d";	break;

#if ENGINE_MAJOR_VERSION >= 5
			//case EMaterialParameterType::StaticSwitch:
			//case EMaterialParameterType::NumRuntime:
			case EMaterialParameterType::Num:
#else
			case EMaterialParameterType::Count:
			case EMaterialParameterType::StaticSwitch:
#endif
				type = "int";		break;

			case EMaterialParameterType::Font:
				type = "Font";		break;

			default:
				type = "void";		break;
			}

			TArray<FText> categories = exp->MenuCategories;
			category = categories.Num() ? categories[0].ToString() : "Default";

			FGuid expid = exp->GetParameterExpressionId();

			//TArray<FMaterialParameterInfo> info;
			//TArray<FGuid> guids;
			//material->Expressions
			//material->GetAllParameterInfo(info, guids);

			//FString initializer = "";
			//material->GetScalarParameterSliderMinMax(;
			//material->GetScalarParameterDefaultValue

			description = exp->Desc;
			FString descriptionV, descriptionD;
			description.Split(TEXT(":"), &descriptionV, &descriptionD);
			if (!descriptionD.IsEmpty())
				description = descriptionD.TrimStartAndEnd();
			//if (description.IsEmpty())
			//	description = friendlyName;
			description = description.Replace(TEXT("\r"), TEXT(""));
			description = description.Replace(TEXT("\n"), TEXT(" "));
			description = description.Replace(TEXT("\\"), TEXT("\\\\"));
			//wcout << "DESCRIPTION: " << *description << endl;
		}
		else
			LOG("Error", "No UMaterialExpression!  Can't add parameter.");

		bool isPrivate = false;
		bool isPublic = true;

		//FString e = FString::Printf(TEXT("%s %s = \"%s\";\t//!< %s"),
		//	*type,
		//	*variableName,
		//	*initializer,
		//	*description
		//);

		FString e = FString::Printf(TEXT("%s %s;\t//!< %s"),
			*type,
			*variableName,
			*description
		);

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
				*out << *FString("	") << *v << endl;

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

bool materialReporter::urlCheck1(UEdGraph* &igraph, UEdGraph* &ograph, UEdGraph *originalGraph, UEdGraphNode *node)
{
#if ENGINE_MAJOR_VERSION >= 5
		UMaterialGraphNode_Composite* matcomp = dynamic_cast<UMaterialGraphNode_Composite*>(node);
		if (matcomp)
		{
			TArray<UEdGraph*> graphs = matcomp->GetSubGraphs();
			if (graphs.Num() > 1)
				ograph = graphs[1];
			if (graphs.Num() > 0)
				igraph = graphs[0];
			else
				UE_LOG(LOG_DOT, Error, TEXT("Can't get a graph."));
		}

		UMaterialGraphNode_PinBase* matpin = dynamic_cast<UMaterialGraphNode_PinBase*>(node);
		if (matpin)
		{
			UMaterialGraph* mg = dynamic_cast<UMaterialGraph*>(originalGraph);
			UMaterial* mat = NULL;

			if (mg)	mat = mg->Material;

			if (mat)
			{
				TArray<EdGraphPtr> graphs;
				UEdGraph* g = mat->MaterialGraph;
				graphs.Add(g);
				addAllGraphs(graphs, g->SubGraphs);
				for (UEdGraph* gg : graphs)
				{
					if (gg->SubGraphs.Contains(originalGraph))
					{	ograph = igraph = gg;
						break;
					}
				}
			}
			else
				UE_LOG(LOG_DOT, Error, TEXT("Pin can't get a graph or material."));
		}
#else
		void* matcomp = NULL;
		void* matpin = NULL;
#endif

	if (!matcomp && !matpin)
		return false;

	return true;
}

bool materialReporter::urlCheck2(FString &blueprintClassName, FString &variableName, UEdGraph *graph, UEdGraphNode *node)
{
#if ENGINE_MAJOR_VERSION >= 5
	UMaterialGraphNode_Composite* matcomp = dynamic_cast<UMaterialGraphNode_Composite*>(node);
	UMaterialGraphNode_PinBase* matpin = dynamic_cast<UMaterialGraphNode_PinBase*>(node);

	if (matcomp || matpin)
	{
		blueprintClassName = currentMaterialInterface->GetName();

		//special faux constructor case
		if (currentMaterialInterface->GetMaterial()->MaterialGraph == graph)
			variableName = blueprintClassName;
		else
			variableName = createVariableName(graph->GetName());

		return true;
	}
#else
	//void* matcomp = NULL;
	//void* matpin = NULL;
#endif

	return false;
}

void materialReporter::LOG(FString message)
{
	LOG("Display",message);
}

void materialReporter::LOG(FString verbosity, FString message)
{
	if (verbosity == "Display")
	{
		wcout << *message << endl;
	}
	else if (verbosity == "Warning")
	{
		//wcout << "WARNING: " << *message << endl;
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
