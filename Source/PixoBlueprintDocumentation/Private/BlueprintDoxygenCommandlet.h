// (c) 2023 PixoVR

#pragma once

#include <iostream>
#include <fstream>
#include <bitset>		//TODO: remove after debugging
using namespace std;

#include "AssetData.h"
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "BlueprintDoxygenCommandlet.generated.h"

/**
 * Commandlet for generating dot syntax diagrams for all blueprints
 * found in a .uproject.
 * 
 * We want to document plugin blueprints, so a plugin will need to be
 * placed in a dummy project for the documentation to take place.
 * 
 * This is based (loosely) on:
 *  - `Engine\Source\Editor\UnrealEd\Private\Commandlets\CompileAllBlueprintsCommandlet.cpp`
 *  - `Engine\Source\Editor\Kismet\Private\BlueprintCompilationManager.cpp`
 * 
 * Tweak your build settings
 * https://www.oneoddsock.com/2020/07/08/ue4-how-to-write-a-commandlet/
 * 
 * Use (for example):
 * ./UnrealEditor-Cmd.exe "X:\PixoVR\Documentation_5_0\Documentation_5_0.uproject" -nullrhi -nopause -nosplash -run=BlueprintDoxygen -LogCmds="global none, LOG_DOT all" -NoLogTimes -OutputMode=doxygen -OutputDir="X:\PixoVR\documentation\docs-root\documentation\pages\blueprints2"
 * ... and without the end parameters for usage message.
 * 
 * ./UnrealEditor-Cmd.exe "X:\PixoVR\Documentation_5_0\Documentation_5_0.uproject" -run=BlueprintDoxygen -LogCmds="global none, LOG_DOT all" -NoLogTimes -OutputMode=doxygen -OutputDir="X:\PixoVR\documentation\docs-root\documentation\pages\blueprints2"
 */
UCLASS()
class UBlueprintDoxygenCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface

protected:

	typedef TMap<FString, FString> vmap;	//name value pairs

	//... could use this instead of vmap
	//FFormatNamedArguments Args;
	//Args.Add(TEXT("NodeName"), FText::FromString(GraphNode->GetName()));
	//Args.Add(TEXT("TooltipText"), TooltipText);
	//TooltipText = FText::Format(NSLOCTEXT("GraphEditor", "GraphNodeTooltip", "{NodeName}\n\n{TooltipText}"), Args);

	//modes for reporting output
	enum OutputMode
	{
		none = 0,
		verbose = 0x1 << 0,
		debug = 0x1 << 1,
		doxygen = 0x1 << 2,
		MAX = 0x1 << 3
	};

	TMap<FString, OutputMode> OutputMode_e = {
		{ "none",		OutputMode::none		},
		{ "verbose",	OutputMode::verbose		},
		{ "debug",		OutputMode::debug		},
		{ "doxygen",	OutputMode::doxygen		},
		{ "MAX",		OutputMode::MAX			}
	};

	//different nodes have to output differently
	enum class NodeType : int
	{
		none = 0,
		node,
		comment,
		bubble,
		route,
		variable,
		variableset,
		compact,
		composite,
		function,
		macro,
		tunnel,
		event,
		path,
		spawn,
		MAX
	};

	//main control
	virtual void InitCommandLine(const FString& Params);
	virtual void BuildAssetLists();
	virtual bool ShouldReportAsset(FAssetData const& Asset) const;
	virtual bool ShouldReportObject(UObject* object) const;
	virtual void LogResults();

	//groups
	virtual void ReportGroup(FString groupName, FString groupNamePretty, FString brief, FString details);
	virtual bool ClearGroups();

	//reporting
	virtual void ReportBlueprints();
	virtual void ReportBlueprint(FString prefix, UBlueprint* Blueprint);

	virtual void ReportMaterials();
	virtual void ReportMaterial(FString prefix, FString assetName, UMaterial* Material);

	virtual void ReportGraph(FString prefix, UEdGraph* Graph);
	virtual void ReportNode(FString prefix, UEdGraphNode* Node);
	virtual void ReportPin(FString prefix, UEdGraphPin* Pin);
	//virtual void ReportSplittablePin(UEdGraphPin* Pin, FString prefix);

	//files
	virtual bool OpenFile(FString fpath, bool append=false);
	virtual bool CloseFile();
	bool CreateThumbnailFile(UObject *object, FString pngPath);
	FString GetTrimmedConfigFilePath(FString path);

	//blueprint stuff
	FString GetClassName(UClass* blueprint);
	FString GetGraphCPP(UEdGraph* graph, FString _namespace="");

	//node stuff
	NodeType GetNodeType(UEdGraphNode* node, NodeType defaultType=NodeType::none);
	FString GetNodeTypeGroup(NodeType type);
	FString GetNodeIcon(UEdGraphNode* node);
	FString GetNodeURL(UEdGraphNode* node, EEdGraphPinDirection direction = EEdGraphPinDirection::EGPD_MAX);
	FString GetNodeTooltip(UEdGraphNode* node);
	FString GetDelegateIcon(UEdGraphNode* node, bool *hasDelegate=NULL);

	//pin stuff
	bool PinShouldBeVisible(UEdGraphPin* pin);
	bool isDelegatePin(UEdGraphPin* pin);
	FString GetPinLabel(UEdGraphPin* pin);
	FString GetPinType(UEdGraphPin* pin, bool useSchema=false);
	FString GetPinTooltip(UEdGraphPin* pin);
	FString GetPinIcon(UEdGraphPin* pin);
	FString GetPinColor(UEdGraphPin* pin);
	FString GetPinPort(UEdGraphPin* pin);
	FString GetPinURL(UEdGraphPin* pin);
	FString GetPinDefaultValue(UEdGraphPin* pin);

	//types and maps
	static TMap<FString, NodeType>					NodeType_e;
	static TMap<FString, FString>					NodeIcons;
	static TMap<FString, TPair<FString, FString> >	PinIcons;

	//command line variables
	int			outputMode;
	FString		outputDir = "-";					//use "-" for stdout
	FString		currentDir = "";
	FString		stylesheet = "doxygen-pixo.css";

	/** runtime */
	TArray<FString> IgnoreFolders;
	FName BlueprintBaseClassName;
	FName MaterialBaseClassName;
	vmap BlueprintStyle;

	/** Registries */
	TArray<FString> ObjectNames;
	TMap<FString, FString>	PinConnections;			//intended to be [SOURCE:port:_ -- DEST:port:_] [color], which forces uniqueness of connections despite direction
	TMap<UEdGraph*, FString> GraphDescriptions;		//assuming parent graphs are parsed before children.  This is the description provided in the collapse node of the parent.
	TMap<FString, TArray<FString>> GraphCalls;				//any node (url) mentioned in a graph is appended to the call graph.

	/** Variables to store overall results */
	int TotalGraphsProcessed;
	int TotalBlueprintsIgnored;
	int TotalMaterialsIgnored;
	int TotalNumFailedLoads;
	int TotalNumFatalIssues;
	int TotalNumWarnings;
	TArray<FString> AssetsWithErrorsOrWarnings;
	TArray<FAssetData> BlueprintAssetList;
	TArray<FAssetData> MaterialAssetList;
	TArray<FString> GroupList;

	// material stuff
	void addAllGraphs(TArray<UEdGraph*> &container, TArray<UEdGraph*> &graphs);

	// doxygen output
	FString createVariableName(FString name, bool forceUnique=true);
	FString createColorString(FLinearColor color, float alpha=1.0f, float exponent=1.0f);
	FString prepTemplateString(FString prefix, vmap vars, FString string);
	FString getCppType(FProperty *prop);

	void writeAssetMembers(UBlueprint* object, FString which);
	void writeAssetFooter();
	void writeAssetCalls();

	void writeBlueprintHeader(UBlueprint *blueprint, FString group, FString qualifier, FString packageName, int graphCount);
	void writeMaterialHeader(UMaterial* material, FString group, FString qualifier, FString assetName, FString packageName, FString packageDescription, FString parentClass, int graphCount);

	void writeGraphHeader(FString prefix, UEdGraph *graph, FString qualifier);
	void writeGraphFooter(FString prefix, UEdGraph *graph);
	void writeGraphConnections(FString prefix);

	FString getNodeTemplate(NodeType type, bool hasDelegate=false);
	FString prepNodePortRows(FString prefix, UEdGraphNode *node);
	void writeNodeBody(FString prefix, UEdGraphNode *node);

private:
	FString _groups = "groups.dox";					// filename for groups file
	FString _tab = "\t";							// depth for indent.  Should be '\t'.
	float _dpi = 96.0f;								// dpi scaling, but only for POS, not SIZE
	float _scale = 72.0f / 96.0f;					// scaling for width and height, which are presented in different DPI

	std::wostream *out = NULL;		// our output stream, which will default to std::wcout
	std::wofstream *outfile = NULL;	// our output file, which will default to NULL unless opened
};
