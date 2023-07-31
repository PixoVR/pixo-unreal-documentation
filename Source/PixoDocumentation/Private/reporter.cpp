// (c) 2023 PixoVR

#include "reporter.h"

#include "Runtime/Launch/Resources/Version.h"

#include "AssetRegistry/AssetRegistryModule.h"

#include "Engine/ObjectLibrary.h"
#include "Engine/Texture2D.h"

#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"

#include "K2Node_AddPinInterface.h"

#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"

#include "ThumbnailRendering/ThumbnailManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
//#include "UObject/UObjectThreadContext.h"
#include "Misc/FileHelper.h"


TArray<FString> reporter::IgnoreFolders;
TArray<FString> reporter::IncludeFolders;
TArray<FString> reporter::GroupList;

/**
 * @brief The base class for reporters.
 * @param _reportType
 * @param _outputDir
 * @param _stylesheet
 * @param _groups
 *
 * This will set initial style parameters and prep for execution using report().
 *
 * \sa blueprintReporter
 * \sa materialReporter
 */

reporter::reporter(FString _reportType, FString _outputDir, FString _stylesheet, FString _groups)
: reportClassName("")
, reportType(_reportType)
, stylesheet(_stylesheet)
, groups(_groups)
, outputDir(_outputDir)
{
	NodeStyle.Empty();
	NodeStyle.Add("_STYLESHEET_", stylesheet);
	NodeStyle.Add("_GRAPHBG_", "transparent");
	//NodeStyle.Add("_GRAPHBG_", "#F1F1F1");	//can't do it this way.  Use css instead.
	NodeStyle.Add("_FONTNAME_", "Arial");
	//NodeStyle.Add("_FONTNAME_", "Helvetica");
	NodeStyle.Add("_FONTSIZE_", "10");		//for header/title
	NodeStyle.Add("_FONTSIZE2_", "9");		//for header/title2
	NodeStyle.Add("_FONTCOLOR_", "black");		//for header/title
	NodeStyle.Add("_FONTSIZEPORT_", "10");		//for pins
	NodeStyle.Add("_FONTSIZECOMMENT_", "18");	//comment size
	NodeStyle.Add("_FONTSIZEBUBBLE_", "11");	//bubble text size
	NodeStyle.Add("_FONTCOLORBUBBLE_", "#888888");	//bubble font color
	NodeStyle.Add("_NODECOLOR_", "#F8F9FA");
	NodeStyle.Add("_NODECOLORTRANS_", "#F8F9FAEE");
	NodeStyle.Add("_VALCOLOR_", "#888888");
	NodeStyle.Add("_BORDERCOLOR_", "#999999");
	NodeStyle.Add("_EDGECOLOR_", "#444444");
	NodeStyle.Add("_EDGETHICKNESS_", "2");
	NodeStyle.Add("_PINDEFAULTCOLOR_", "#444444");
	NodeStyle.Add("_PINURL_", "");
	NodeStyle.Add("_CELLPADDING_", "3");
	NodeStyle.Add("_COMMENTPADDING_", "7");
	NodeStyle.Add("_COMPOSITEPADDING_", "4");	// 5 ?
	NodeStyle.Add("_COMPACTCOLOR_", "#888888");
	NodeStyle.Add("_COMPACTSIZE_", "17");
	NodeStyle.Add("_COMPACTWIDTH_", "100");

	NodeStyle.Add("_HEIGHTSPACER_", "<font color=\"transparent\" point-size=\"12\">&thinsp;</font>");

	//outputDir = "-";			//comment this line when not debugging
	if (outputDir == "-")
		out = &std::wcout;
	//else
	//	LOG("Output Directory: "+outputDir);

	//loadAssets();
}

reporter::~reporter()
{
}

void reporter::loadAssets(FName loadClass)
{
	LOG("Loading Asset Class: "+loadClass.ToString()+"...");

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
	AssetRegistryModule.Get().SearchAllAssets(/*bSynchronousSearch =*/true);

	AssetRegistryModule.Get().GetAssetsByClass(loadClass, assetList, true);

	LOG(FString::Printf(TEXT("Found %d %s instances in asset registry."), assetList.Num(),*loadClass.ToString()));
}

void reporter::report(int &graphCount, int &ignoredCount, int &failedCount)
{
	LOG( "Parsing " + reportType + "..." );

	loadAssets(UBlueprint::StaticClass()->GetFName());			//BlueprintBaseClassName
	loadAssets(UMaterialInterface::StaticClass()->GetFName());	//MaterialBaseClassName

	for (FAssetData const& Asset : assetList)
	{
		if (shouldReportAsset(Asset))
		{
#if ENGINE_MAJOR_VERSION >= 5
			FString const AssetPath = Asset.GetObjectPathString();
#else
			FString const AssetPath = Asset.ObjectPath.ToString();
#endif
			FString const AssetName = Asset.AssetName.ToString();
			FString const PackagePath = Asset.PackagePath.ToString();

			UE_LOG(LOG_DOT, Warning, TEXT("Loading Asset:   '%s'..."), *AssetPath);
			UE_LOG(LOG_DOT, Warning, TEXT("Loading Package: '%s'..."), *PackagePath);
			UE_LOG(LOG_DOT, Warning, TEXT("Asset name: '%s'..."), *AssetName);

			//Load with LOAD_NoWarn and LOAD_DisableCompileOnLoad.
			UBlueprint* LoadedBlueprint = Cast<UBlueprint>(StaticLoadObject(Asset.GetClass(), /*Outer =*/nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_DisableCompileOnLoad));
			UMaterialInterface* LoadedMaterial = Cast<UMaterialInterface>(StaticLoadObject(Asset.GetClass(), /*Outer =*/nullptr, *AssetPath, nullptr, LOAD_None));

			if (!LoadedBlueprint && !LoadedMaterial)
			{
				failedCount++;
				UE_LOG(LOG_DOT, Error, TEXT("%sFailed to Load : '%s'."), *_tab, *AssetPath);
				continue;
			}
			else
			{
				if (LoadedBlueprint)
					reportBlueprint(_tab, LoadedBlueprint);

				if (LoadedMaterial)
					reportMaterial(_tab, LoadedMaterial);
			}
		}
		else
			ignoredCount++;
	}
}

void reporter::reportGroup(FString groupName, FString groupNamePretty, FString brief, FString details)
{
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

	FString path = outputDir + "/" + groups;
	FPaths::MakePlatformFilename(path);
	if (!openFile(path,true))
	{
		UE_LOG(LOG_DOT, Error, TEXT("Could not open '%s' for writing."), *path);
		return;
	}

	*out << *groupdata << endl;

	closeFile();
}

FString reporter::getGraphCPP(UEdGraph* graph, FString _namespace)
{
	FString graphName = graph->GetName();
	FString graphNameVariable = createVariableName(graphName);
	//FString graphNameHuman = FName::NameToDisplayString(graphName, false);

	//FString type = g->GetClass()->GetSuperClass()->GetFName().ToString();
	//FString type = getClassName(graph->GetOuter()->GetClass());
	FString type = getClassName(graph->GetClass());
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

/**
 * @brief Based on IgnoreFolders and IncludeFolders, check this items path for reporting.
 * @param Asset The asset to check
 * @return Boolean if it should be reported
 *
 * Ignore is used to do a first cull, and is set in the PixoDocumentation ctor.
 *
 * Perhaps in a future version, include will trump ignore, but not for now.
 * This would imply that someone could include "/Game".
 */

bool reporter::shouldReportAsset(FAssetData const& Asset)
{
	//FString path = Asset.GetPackage
#if ENGINE_MAJOR_VERSION >= 5
	FString path = Asset.GetObjectPathString();
#else
	FString path = Asset.ObjectPath.ToString();
#endif



	//wcout << "Checking " << *path << endl;

	for (const FString& IgnoreFolder : IgnoreFolders)
	{
		if (path.StartsWith(IgnoreFolder))
			return false;
	}

	for (const FString& IncludeFolder : IncludeFolders)
	{
		if (path.StartsWith(IncludeFolder))
			return true;
	}

	return false;
}

/**
 * @brief Based on IgnoreFolders and IncludeFolders, check this object's path for reporting.
 * @param object The object to check
 * @return Boolean if it should be reported
 *
 * Ignore is used to do a first cull, and is set in the PixoDocumentation ctor.
 *
 * Perhaps in a future version, include will trump ignore, but not for now.
 * This would imply that someone could include "/Game".
 */

bool reporter::shouldReportObject(UObject* object)
{
	if (!object)
		return false;

	FString path = object->GetPathName();
	//wcout << "SRO path: " << *path << endl;

	for (const FString& IgnoreFolder : IgnoreFolders)
	{
		if (path.StartsWith(IgnoreFolder))
			return false;
	}

	for (const FString& IncludeFolder : IncludeFolders)
	{
		if (path.StartsWith(IncludeFolder))
			return true;
	}

	return false;
}

void reporter::reportGraph(FString prefix, UEdGraph* g)
{
	pinConnections.Empty();		//clear out all connections for each new graph

	FString graphName;
	g->GetName(graphName);

	FString graphNameVariable = createVariableName(graphName);
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

	for (UEdGraphNode* n : g->Nodes)
		reportNode(prefix + _tab, n);

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

void reporter::reportNode(FString prefix, UEdGraphNode* n)
{
	//FString type = n->GetSchema
	FString type = n->GetClass()->GetFName().ToString();

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
		UE_LOG(LOG_DOT, Display, TEXT("%spins (%d):"), *prefix2, pins.Num());

		for (UEdGraphPin* p : pins)
		{
			reportPin(prefix2 + _tab, p);
		}
	}
}


void reporter::writeGraphHeader(FString prefix, UEdGraph* graph, FString qualifier)
{
	FString graphName = graph->GetName();
	FString graphNameVariable = createVariableName(graphName);
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
	*out << *prepTemplateString(prefix + _tab + _tab, NodeStyle, R"PixoVR(
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

void reporter::writeGraphFooter(FString prefix, UEdGraph *graph)
{
	FString graphName = graph->GetName();
	FString graphNameVariable = createVariableName(graphName);
	FString graphNameHuman = FName::NameToDisplayString(graphName, false);

	//FString type = g->GetClass()->GetSuperClass()->GetFName().ToString();
	//FString type = getClassName(graph->GetOuter()->GetClass());
	//FString type = getClassName(graph->GetClass());

	*out << *prefix << *_tab << "}" << endl;
	*out << *prefix << *_tab << "\\enddot" << endl;
	*out << *prefix << "*/" << endl;

	//bool isFunction = ??
	//if (isFunction)
	//	*out << *prefix << *type << " " << *functionName << "(" << *functionArguments << ");" << endl;
	//else

	FString cpp = getGraphCPP(graph);
	*out << *prefix << *cpp << ";	//\"" << *graphNameHuman << "\"" << endl;
}

void reporter::writeGraphConnections(FString prefix)
{
	*out << endl;

	for (const TPair<FString, FString>& c : pinConnections)
	{
		//FROM:port:_ -- TO:port:_ [ color="colorFmt" ]
		FString connection = FString::Printf(TEXT("%s [ color=\"%s\" layer=\"edges\" ];"), *c.Key, *c.Value);

		*out << *prefix << *_tab << *_tab << *connection << endl;
	}
}

bool reporter::urlCheck1(UEdGraph* &igraph, UEdGraph* &ograph, UEdGraph *originalGraph, UEdGraphNode *node)
{
	return false;
}

bool reporter::urlCheck2(FString &blueprintClassName, FString &variableName, UEdGraph *graph, UEdGraphNode *node)
{
	return false;
}

FString reporter::prepNodePortRows(FString prefix, UEdGraphNode* node, TMap<FString, FString> visiblePins)
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

	//&reg;
	//&#10122;
	FString routeTemplate = R"PixoVR(<tr>
	<td port="port" href="_PINURL_" title="_NODECOMMENT_"><font color="_PINCOLOR_">&#9673;</font></td>
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

	bool isRoute = getNodeType(node, NodeType::node) == NodeType::route;
	bool isVariable = getNodeType(node, NodeType::node) == NodeType::variable;
	bool isVariableset = getNodeType(node, NodeType::node) == NodeType::variableset;
	bool isCompact = getNodeType(node, NodeType::node) == NodeType::compact;
	bool isConnected = false;		//changed below
	bool otherIsRoute = false;		//updated below

	FString rows;

	vmap pindata;
	pindata.Add("_PINCOLOR_", "_PINDEFAULTCOLOR_");

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

	bool NeedsAddPin = false;
	IK2Node_AddPinInterface* n = Cast<IK2Node_AddPinInterface>(node);
	if (n && n->CanAddPin())
	{
		//add add pin on output, which is purely cosmetic.
		NeedsAddPin = true;
		//n->AddInputPin();		//not what we want... will add to the left with no control
	}

	const TArray< UEdGraphPin* > pins = node->Pins;
	//const TArray< UEdGraphPin* > pins = n->GetAllPins();

	TArray< UEdGraphPin* > ins;
	TArray< UEdGraphPin* > outs;
	for (UEdGraphPin* p : pins)
	{
		//don't show hidden pins
		if (!pinShouldBeVisible(p,visiblePins))
			continue;

		if (p->Direction == EEdGraphPinDirection::EGPD_Input)
			ins.Add(p);
		else
			if (!isDelegatePin(p))
				outs.Add(p);
	}

	TMap<FString, FString> vp;
	FString sname, sport, sside, dname, dport, dside, color, connection;
	FString rowTemplate;
	UEdGraphPin *i=NULL, *o=NULL;
	int c = 0;
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
		else if (i && (o || NeedsAddPin))
			rowTemplate = nodeTemplate_11;
		else if (i && !o)
			rowTemplate = nodeTemplate_10;
		else if (!i && (o || NeedsAddPin))
			rowTemplate = nodeTemplate_01;
		else
			UE_LOG(LOG_DOT, Error, TEXT("No row in or out.. this should never happen."));

		if (i)
		{
			dport = getPinPort(i);
			color = getPinColor(i);
			isConnected = i->HasAnyConnections();

			pindata["_PINCOLOR_"] = color;
			pindata["_INPORT_"] = dport;
			pindata["_INICON_"] = getPinIcon(i);
			pindata["_INLABEL_"] = isCompact ? "" : getPinLabel(i);
			pindata["_INCOLOR_"] = color;
			pindata["_INVALUE_"] = isConnected ? "" : getPinDefaultValue(i);
			pindata["_INTOOLTIP_"] = getPinTooltip(i,visiblePins);

			//create connection(s)
			dname = i->GetOwningNode()->GetName();		//the source name, from this output
			for (UEdGraphPin* s : i->LinkedTo)
			{
				vp = getVisiblePins(s->GetOwningNode());
				if (!pinShouldBeVisible(s, vp))
					continue;

				otherIsRoute = getNodeType(s->GetOwningNode(), NodeType::node) == NodeType::route;
				sname = s->GetOwningNode()->GetName();	//the destination node, from this output
				sport = getPinPort(s);

				sside = otherIsRoute	? "c" : "e";
				dside = isRoute			? "c" : "w";

				connection = FString::Printf(TEXT("%s:%s:%s -- %s:%s:%s"), *sname, *sport, *sside, *dname, *dport, *dside);
				pinConnections.Add(connection, color);
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
			sport = getPinPort(o);
			color = getPinColor(o);
			isConnected = o->HasAnyConnections();

			pindata["_PINCOLOR_"] = color;
			pindata["_OUTPORT_"] = sport;
			pindata["_OUTICON_"] = getPinIcon(o);
			pindata["_OUTLABEL_"] = isCompact ? "" : getPinLabel(o);
			pindata["_OUTCOLOR_"] = color;
			pindata["_OUTVALUE_"] = isConnected ? "" : getPinDefaultValue(o);	//ever used?
			pindata["_OUTTOOLTIP_"] = getPinTooltip(o);

			//create connection(s)
			sname = o->GetOwningNode()->GetName();		//the source name, from this output
			for (UEdGraphPin* d : o->LinkedTo)
			{
				vp = getVisiblePins(d->GetOwningNode());
				if (!pinShouldBeVisible(d, vp))
					continue;

				otherIsRoute = getNodeType(d->GetOwningNode(), NodeType::node) == NodeType::route;
				dname = d->GetOwningNode()->GetName();	//the destination node, from this output
				dport = getPinPort(d);

				sside = isRoute			? "c" : "e";
				dside = otherIsRoute	? "c" : "w";

				connection = FString::Printf(TEXT("%s:%s:%s -- %s:%s:%s"), *sname, *sport, *sside, *dname, *dport, *dside);
				pinConnections.Add(connection, color);
			}
		}
		else if (NeedsAddPin)
		{
			pindata["_OUTPORT_"] = "";
			pindata["_OUTICON_"] = PinIcons["addpin"].Key;
			pindata["_OUTLABEL_"] = "<font color=\"_VALCOLOR_\">Add pin</font>";
			pindata["_OUTCOLOR_"] = "_BORDERCOLOR_";
			pindata["_OUTVALUE_"] = "";
			pindata["_OUTTOOLTIP_"] = "";

			NeedsAddPin = false;
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

		c++;
	}

	if (c==0)
	{
		rows = "<tr><td></td></tr>";
	}

	rows.TrimEndInline();

	rows = prepTemplateString(prefix, NodeStyle, rows);

	return rows;
}

void reporter::reportPin(FString prefix, UEdGraphPin* p)
{
	//don't show hidden pins
	if (!pinShouldBeVisible(p))
		return;

	FString flabel = getPinLabel(p);
	FString type = getPinType(p);
	FString tooltip = getPinTooltip(p);
	FString dir = (p->Direction == EEdGraphPinDirection::EGPD_Input) ? TEXT("<===") : TEXT("===>");
	//FString pid = p->PinId.ToString();

	if (p->HasAnyConnections())
	{
		for (UEdGraphPin* d : p->LinkedTo)
		{
			FString tlabel = getPinLabel(d);

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
	else
	{
		FString dval = p->DefaultValue;
		//UObject *dobj = p->DefaultObject;

		FString tlabel = dval.IsEmpty() ? "(*:NULL)" : "(*:'"+dval+"')";

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

FString reporter::getNodeTitle(UEdGraphNode* n, FString& title2)
{
	FString title = n->GetNodeTitle(ENodeTitleType::ListView).ToString();	//kinda useless

	//if (type == NodeType::composite)
	{
		FString ftitle = n->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		ftitle = ftitle.Replace(TEXT("\r"), TEXT(""));
		ftitle.Split(TEXT("\n"), &title, &title2);
		if (!title2.IsEmpty())
			title2 = "<font point-size=\"_FONTSIZE2_\">" + title2 + "</font>";
		else
			title = ftitle.TrimStartAndEnd();

		//wcout << "FTITLE: " << *ftitle << endl;
		//wcout << "FTITLE: " << *title << " || " << *title2 << endl;
	}

	return title;
}

bool reporter::getNodeHasBubble(UEdGraphNode *n)
{
	bool hasBubble = n->bCommentBubbleVisible;
	hasBubble |= n->bCommentBubblePinned;
	hasBubble |= n->ShouldMakeCommentBubbleVisible();

	UEdGraphNode_Comment* commentNode = dynamic_cast<UEdGraphNode_Comment*>(n);
	if (commentNode)
		hasBubble = false;

	return hasBubble;
}

TMap<FString, FString> reporter::getVisiblePins(UEdGraphNode* node)
{
	TMap<FString, FString> visiblePins;								// if empty, all pins are allowed.  Otherwise only these pins are allowed.  This is set during node reporting.
	return visiblePins;
}


void reporter::writeAssetFooter()
{
	*out << "};" << endl;
	*out << endl;
}

void reporter::writeAssetCalls(FString className)
{
	*out << endl;
	*out << "#include \"" << *className << ".h\"" << endl;

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

FString reporter::getNodeURL(UEdGraphNode* node, EEdGraphPinDirection direction)
{
	//subgraphs							X
	//inputs/outputs (tunnels)			X
	//spawn nodes						TODO
	//variables							X
	//function nodes					X
	//macro								X
	//material nodes?

	NodeType type = getNodeType(node, NodeType::node);
	FString url = "";

	UBlueprint* blueprint = FBlueprintEditorUtils::FindBlueprintForNode(node);
	FString blueprintClassName = blueprint ? getClassName(blueprint->GeneratedClass) : "";
	FString variableName;				// leave uninitialized

	UEdGraph* originalGraph = node->GetGraph();
	FString originalGraphName = createVariableName(originalGraph->GetName());
	FString originalBlueprintClassName = blueprintClassName;

	switch (type)
	{
		case NodeType::materialcomposite:	// subgraph
		case NodeType::materialtunnel:		// subgraph
		case NodeType::composite:			// subgraph
		case NodeType::tunnel:				// subgraph
		{
			UEdGraph* graph = NULL;
			UEdGraph* igraph = NULL;
			UEdGraph* ograph = NULL;

			//UK2Node_Tunnel* tunnel = Cast<UK2Node_Tunnel*,

			UK2Node_Tunnel* tunnel = dynamic_cast<UK2Node_Tunnel*>(node);
			if (tunnel)
			{
				UK2Node_Tunnel* inode = tunnel->GetInputSink();
				UK2Node_Tunnel* onode = tunnel->GetOutputSource();

				igraph = inode ? inode->GetGraph() : NULL;
				ograph = onode ? onode->GetGraph() : NULL;
			}

			urlCheck1(igraph, ograph, originalGraph, node);

			//if (!tunnel && !matcomp && !matpin)
			//{
			//	UE_LOG(LOG_DOT, Error, TEXT("Can't cast to tunnel/composite/materialcomposite."));
			//	break;
			//}

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
				//if not on the no-fly list...
				bool report = shouldReportObject(graph);

				blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(graph);
				//if (shouldReportObject(blueprint))

				if (report)
				{
					if (blueprint)
					{
						blueprintClassName = getClassName(blueprint->GeneratedClass);
						variableName = createVariableName(graph->GetName());
					}
					else if (
						urlCheck2(blueprintClassName,variableName,graph,node)
					) {}
					// else
					//	no url!
				}
			}
			//else
			//	UE_LOG(LOG_DOT, Error, TEXT("Composite can't get a graph."));
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

				if (!fp)
				{	//wcout << "NO FP!" << endl;
					//the blueprint was probably renamed, or some other link was broken.  Normally this shouldn't happen.
					break;
				}

				//wcout << "FPp: " << *fp->NamePrivate.ToString() << endl;
				if (locals.Contains(fp->NamePrivate))	//local variables don't get a URL.
					return "";

				UClass *blueprintClass = variable->VariableReference.GetMemberParentClass();
				if (blueprintClass)
				{
					blueprintClassName = getClassName(blueprintClass);
				}
				else
				{
					blueprint = variable->GetBlueprint();

					//if not on the no-fly list...
					if (shouldReportObject(blueprint))
					{	blueprintClassName = getClassName(blueprint->GeneratedClass);
						variableName = createVariableName(fp->GetNameCPP());
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
			if (shouldReportObject(blueprint))
			{	blueprintClassName = getClassName(blueprint->GeneratedClass);
				variableName = createVariableName(graph->GetName());
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
			if (shouldReportObject(blueprint))
			{	blueprintClassName = getClassName(blueprint->GeneratedClass);
				variableName = createVariableName(graph->GetName());
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
			if (shouldReportObject(blueprint))
			{	blueprintClassName = getClassName(blueprint->GeneratedClass);
				variableName = createVariableName(graph->GetName());
			}
		}
		break;

	default:
		//wcout << "unknown url type: " << *node->GetName() << endl;
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
	//wcout << *node->GetName() << " :: URL: " << *url << endl;

	FString cpp = getGraphCPP(originalGraph, originalBlueprintClassName);
	if (!url.IsEmpty())
	{
		FString curl = blueprintClassName + "::" + variableName;
		GraphCalls.FindOrAdd(cpp).AddUnique(curl);
	}
	else
		GraphCalls.FindOrAdd(cpp).AddUnique("");

	return url;
}

void reporter::writeNodeBody(FString prefix, UEdGraphNode* n)
{
	//should we be casting to UK2Node instead of using UEdGraphNode?

	FString nodename = n->GetName();

	NodeType type = getNodeType(n, NodeType::node);
	FString typeGroup = getNodeTypeGroup(type);
	FString url = getNodeURL(n);
	FString tooltip = getNodeTooltip(n);

	FString title,title2;
	title = getNodeTitle(n,title2);

	bool hasBubble = getNodeHasBubble(n);

	UEdGraphNode_Comment* commentNode = dynamic_cast<UEdGraphNode_Comment*>(n);
	FLinearColor titleColor = (commentNode) ? commentNode->CommentColor : n->GetNodeTitleColor();

	FLinearColor titleTextColor = FLinearColor::Black;
	if (titleColor.LinearRGBToHSV().B < .6f)
		titleTextColor = FLinearColor::White;

#if ENGINE_MAJOR_VERSION >= 5
	int commentSize = (commentNode) ? commentNode->GetFontSize() : 18;
#else
	int commentSize = 18;
#endif

	TMap<FString, FString> visiblePins = getVisiblePins(n);						// if empty, all pins are allowed.  Otherwise only these pins are allowed.  This is set during node reporting.

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
			//NodeStyle.Add("_COMPACTSIZE_", "17");
			NodeStyle.Add("_COMPACTSIZE_", "16");
			NodeStyle.Add("_COMPACTWIDTH_", FString::Printf(TEXT("%d"), 70 + charWidth * titleLen));
		}
		else
		{
			//NodeStyle.Add("_COMPACTSIZE_", "36");
			NodeStyle.Add("_COMPACTSIZE_", "16");
			NodeStyle.Add("_COMPACTWIDTH_", "100");
		}
	}

	if (type == NodeType::variableset)
	{
		NodeStyle.Add("_COMPACTSIZE_", "16");
		NodeStyle.Add("_COMPACTWIDTH_", "90");
	}

	float posx = (float)n->NodePosX / _dpi;
	float posy = (float)n->NodePosY / -_dpi;						// y is inverted
	float width = (float)n->NodeWidth * _scale;
	float height = (float)n->NodeHeight * _scale;

	if (type == NodeType::route)
	{	posx += 14.0f / _dpi;
		posy += 2.0f / _dpi;
	}

	TArray<FString> lines;													// we throw this away
	FString comment = n->NodeComment.TrimStartAndEnd();					// if (mn)	comment_ = mn->MaterialExpression->Desc;
	float numLines = comment.ParseIntoArrayLines(lines, false);			// for bubble
	//if (numLines == 1)
	//	comment = comment + "&nbsp;";										//spaces out the bubble
	comment = htmlentities(comment);
	comment = comment.Replace(TEXT("\r"), TEXT(""));
	comment = comment.Replace(TEXT("\n"), TEXT("&nbsp;<br/>"));			// after counting lines
	//comment = comment.IsEmpty() ? "&nbsp;" : comment;					// hasBubble is always true... don't do this!
	hasBubble &= !comment.IsEmpty();									// if the comment is empty, it won't show.

	//wcout << "FTITLE: " << *title << " ## " << hasBubble << " * " << (mn?1:0) << " ## " << *comment << endl;

	bool hasDelegate = false;

	NodeStyle.Add("_NODENAME_", nodename);
	NodeStyle.Add("_NODEGUID_", n->NodeGuid.ToString());
	NodeStyle.Add("_NODEICON_", getNodeIcon(n));
	NodeStyle.Add("_NODEDELEGATE_", getDelegateIcon(n,&hasDelegate));	//TODO: add node delegate tooltip
	NodeStyle.Add("_NODETITLE_", title);
	NodeStyle.Add("_NODETITLE2_", title2);
	//NodeStyle.Add("_NODECOLOR_", createColorString(n->GetNodeBodyTintColor()));
	NodeStyle.Add("_NODECOMMENT_", comment);
	NodeStyle.Add("_POS_", FString::Printf(TEXT("%0.2f,%0.2f!"), posx, posy));
	NodeStyle.Add("_WIDTH_", FString::Printf(TEXT("%0.2f"), width));
	NodeStyle.Add("_HEIGHT_", FString::Printf(TEXT("%0.2f"), height));
	NodeStyle.Add("_TOOLTIP_", tooltip);
	NodeStyle.Add("_HEADERCOLOR_", createColorString(titleColor));
	NodeStyle.Add("_HEADERCOLORDIM_", createColorString(titleColor * 0.5f, 1.0f, 1.0f));
	NodeStyle.Add("_HEADERCOLORLIGHT_", createColorString(titleColor, 1.0f, 3.0f));
	NodeStyle.Add("_HEADERCOLORTRANS_", createColorString(titleColor, 0.5f));
	NodeStyle.Add("_HEADERTEXTCOLOR_", createColorString(titleTextColor));
	NodeStyle.Add("_CLASS_", typeGroup);
	NodeStyle.Add("_URL_", url);				//URL = "\ref SomeSubgraph"
	NodeStyle.Add("_PORTROWS_", prepNodePortRows(prefix+_tab+_tab,n,visiblePins));
	NodeStyle.Add("_FONTSIZECOMMENT_", FString::FromInt(commentSize));

	FString nodeTemplate = getNodeTemplate(type, hasDelegate);

	*out << *prepTemplateString(prefix + _tab, NodeStyle, nodeTemplate) << endl;

	//if a comment is visible, add it as a node and connect the arrow
	if (hasBubble)
	{
		float lineHeight = 11.0f;
		float margin = 41.0f;

		//float cposx = posx + (type==NodeType::route ? -.228f : 0.0f);	//&reg;
		float cposx = posx + (type == NodeType::route ? -.226f : 0.0f);	//&#10752;
		float cposy = posy + ((numLines * lineHeight) + margin) / _dpi;

		NodeStyle.Add("_POS_", FString::Printf(TEXT("%0.2f,%0.2f!"), cposx, cposy));

		FString commentString = getNodeTemplate(NodeType::bubble);
		//FString connection = FString::Printf(TEXT("%s %s [ color=\"_NODECOLOR_\" layer=\"edges\" arrowhead=\"normal\" direction=\"forward\" penwidth=\"5\" ];"), *c.Key, *c.Value);

		*out << *prepTemplateString(prefix + _tab, NodeStyle, commentString) << endl;
		//*out << *prefix << *_tab << *connection << endl;		//don't need an edge here now because of the arrow/triangle
	}
}

int reporter::reportBlueprint(FString prefix, UBlueprint* blueprint)
{
	const UPackage* package = blueprint->GetPackage();
	FString className = getClassName(blueprint->GeneratedClass);
	FString path, subDir;

	//each blueprint will have different graphs.  This is just to save some memory on a large build.
	GraphDescriptions.Empty();

	//each graph will make calls to nodes and variables.  Clear these before we report each blueprint.
	GraphCalls.Empty();

	const UClass* parent = blueprint->ParentClass;
	FString description = "This class " + blueprint->GetDesc() + ".\n\n" + prefix + blueprint->BlueprintDescription;
	description = description.TrimStartAndEnd();

	UE_LOG(LOG_DOT, Display, TEXT("%sParsing: %s"),	*prefix, *blueprint->GetPathName());
#if ENGINE_MAJOR_VERSION >= 5
	UE_LOG(LOG_DOT, Display, TEXT("%sPackage: %s"), *prefix, *package->GetLoadedPath().GetLocalFullPath());
#endif
	UE_LOG(LOG_DOT, Display, TEXT("%sName: %s"), *prefix, *package->GetName());
	UE_LOG(LOG_DOT, Display, TEXT("%sExtends: %s"), *prefix, *(parent->GetPrefixCPP()+parent->GetName()));
	UE_LOG(LOG_DOT, Display, TEXT("%sDescription: %s"), *prefix, *(description.Replace(TEXT("\\n"),TEXT(" || "))));

	TArray<UEdGraph*> graphs;
	blueprint->GetAllGraphs(graphs);

	UE_LOG(LOG_DOT, Display, TEXT("%sFound %d graph%s..."), *prefix, graphs.Num(), graphs.Num() != 1 ? TEXT("s") : TEXT(""));

	for (UEdGraph* g : graphs)
		reportGraph(prefix, g);

	return graphs.Num();
}

int reporter::reportMaterial(FString prefix, UMaterialInterface* materialInterface)
{
	//currentMaterialInterface = materialInterface;
	const UPackage* package = materialInterface->GetPackage();
	UMaterial* material = materialInterface->GetMaterial();
	FString className = materialInterface->GetName();
	FString path, subDir;
	FString imageTag = "";

	//each material will have different graphs.  This is just to save some memory on a large build.
	GraphDescriptions.Empty();

	//each graph will make calls to nodes and variables.  Clear these before we report each blueprint.
	GraphCalls.Empty();

	UClass* parent = material->GetBaseMaterial()->GetClass();
	//UClass* parent = material->StaticClass();// ->GetSuperClass();
	FString parentClass = getClassName(parent);

	//FString description = "This class " + material->GetDesc() + ".\n\n" + prefix + material->GetDesc() + "\n" +material->GetDetailedInfo();
	//description = description.TrimStartAndEnd();

	UE_LOG(LOG_DOT, Display, TEXT("%sParsing: %s"), *prefix, *material->GetPathName());
#if ENGINE_MAJOR_VERSION >= 5
	UE_LOG(LOG_DOT, Display, TEXT("%sPackage: %s"), *prefix, *package->GetLoadedPath().GetLocalFullPath());
#endif
	UE_LOG(LOG_DOT, Display, TEXT("%sName: %s"), *prefix, *package->GetName());
	UE_LOG(LOG_DOT, Display, TEXT("%sExtends: %s"), *prefix, *parentClass);
	//UE_LOG(LOG_DOT, Display, TEXT("%sDescription: %s"), *prefix, *(description.Replace(TEXT("\\n"), TEXT(" || "))));

	TArray<UEdGraph*> graphs;

	//UEdGraph* graph = material->MaterialGraph;
	UMaterialGraph* graph = material->MaterialGraph;
	if (!graph)
	{
		graph = material->MaterialGraph = CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(material, FName(*className), UMaterialGraph::StaticClass(), UMaterialGraphSchema::StaticClass()));
		material->MaterialGraph->Material = material;
		material->MaterialGraph->RebuildGraph();
	}

	graphs.Add(graph);
	addAllGraphs(graphs, graph->SubGraphs);

	UE_LOG(LOG_DOT, Display, TEXT("%sFound %d graph%s..."), *prefix, graphs.Num(), graphs.Num() != 1 ? TEXT("s") : TEXT(""));

	for (UEdGraph* g : graphs)
		reportGraph(prefix, g);

	return graphs.Num();
}

bool reporter::createThumbnailFile(UObject* object, FString pngPath)
{
	UPackage* package = object->GetPackage();
	FString fullName = object->GetFullName();

	//wcout << "FNAME: |" << *fullName << "|" << endl;

#if ENGINE_MAJOR_VERSION >= 5
	FLinkerLoad* Linker = package->GetLinker();	//NO! Won't work.
#else
	FLinkerLoad* Linker = package->LinkerLoad;
#endif

	if (Linker && Linker->SerializeThumbnails(true))
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

						LOG(" Writing   " + tpngPath);

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
	else
	{
		//UE_LOG(LOG_DOT, Error, TEXT("No linker: '%s'."), *pngPath);
	}

	return false;
}

bool reporter::openFile(FString fpath, bool append)
{
	closeFile();

	std::ofstream::openmode mode = std::ofstream::out;
	if (append)
		mode |= std::ofstream::app;

	std::wofstream* stream = new std::wofstream();
	//stream->open(*fpath, mode);

	std::string sfpath(TCHAR_TO_UTF8(*fpath));
	stream->open(sfpath, mode);

	if (stream->is_open())
	{
		out = outfile = stream;

		FString tpath = fpath;
		tpath.RemoveFromStart(outputDir);
		tpath = "[OutputDir]" + tpath;

		if (append)
		{
			LOG( " Appending " + tpath );
			//wcout << " Appending " << *tpath << endl;
		}
		else
		{
			LOG( " Writing   " + tpath );
			//wcout << " Writing   " << *tpath << endl;
		}
	}
	else
	{
		out = &std::wcout;
		wcerr << "Error opening file: '" << *fpath << "'" << endl;
		return false;
	}

	return true;
}

bool reporter::closeFile()
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

void reporter::LOG(FString message)
{
	LOG("Display",message);
}

void reporter::LOG(FString verbosity, FString message)
{
	if (verbosity == "Display")
	{
		UE_LOG(LOG_DOT, Display, TEXT("%s"), *message);
	}
	else if (verbosity == "Warning")
	{
		UE_LOG(LOG_DOT, Warning, TEXT("%s"), *message);
	}
	else if (verbosity == "Error")
	{
		UE_LOG(LOG_DOT, Error, TEXT("%s"), *message);
	}
	else
	{
		UE_LOG(LOG_DOT, Warning, TEXT("%s"), *message);
	}
}
