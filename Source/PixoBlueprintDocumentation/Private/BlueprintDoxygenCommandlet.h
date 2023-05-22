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
 * Use (for example):
 * ./UnrealEditor-Cmd.exe "X:\PixoVR\Documentation_5_0\Documentation_5_0.uproject" -nullrhi -nopause -nosplash -run=BlueprintDoxygen -LogCmds="global none, LOG_DOT all" -NoLogTimes -OutputMode=doxygen -OutputDir="X:\PixoVR\documentation\docs-root\documentation\pages\blueprints2"
 * ... and without the end parameters for usage message.
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
		compact,
		composite,
		function,
		macro,
		tunnel,
		event,
		path,
		MAX
	};

	/** Handles setting config variables from the command line
	* @Param Params command line parameter string
	*/
	virtual void InitCommandLine(const FString& Params);

	/** Stores FAssetData for every BlueprintAsset inside of the BlueprintAssetList member variable */
	virtual void BuildAssetLists();

	/** Loads and reports node information about all blueprints in the BlueprintAssetList member variable */
	virtual void ReportBlueprints();

	/** Loads and reports node information about all materials in the MaterialAssetList member variable */
	virtual void ReportMaterials();

	/** Determines if we should try and Load / Report the asset based on config variables and module path
	* @Param Asset What asset to check
	* @Return True if we should build the asset
	*/
	virtual bool ShouldReportAsset(FAssetData const& Asset) const;

	/** Handles creating and reporting a group to groups.dox.
	* Will only add groups once to the output.
	* @Param FString groupName to report.
	*/
	virtual void ReportGroup(FString groupName, FString groupNamePretty, FString brief, FString details);
	virtual bool ClearGroups();

	/** Handles attempting to report an individual blueprint asset.
	* @Param Blueprint Asset to report.
	*/
	virtual void ReportBlueprint(FString prefix, FString assetName, UBlueprint* Blueprint);

	/** Handles attempting to report an individual material asset.
	* @Param Material Asset to report.
	*/
	virtual void ReportMaterial(FString prefix, FString assetName, UMaterial* Material);

	/** Handles attempting to report an individual graph.
	* @Param UEdGraph Graph to report.
	* @Param FString prefix to prepend.
	*/
	virtual void ReportGraph(UEdGraph* Graph, FString prefix);

	/** Handles attempting to report an individual node.
	* @Param UEdGraphNode Node to report.
	* @Param FString prefix to prepend.
	*/
	virtual void ReportNode(UEdGraphNode* Node, FString prefix);

	/** Handles attempting to report an individual pin from a node.
	* @Param UEdGraphPin Pin to report.
	*/
	virtual void ReportPin(UEdGraphPin* Pin, FString prefix);
	virtual void ReportSplittablePin(UEdGraphPin* Pin, FString prefix);

	/** Opens a file for writing.  Doxygen Mode Only.  This points 
	* 'out' to the file, and closes 'outfile' if it is currently open.
	* @Param FString fpath to open.
	*/
	virtual bool OpenFile(FString fpath, bool append=false);

	/** Closes 'outfile' if it is open.  Doxygen Mode Only.	*/
	virtual bool CloseFile();

	/** Handles retrieving data about a blueprint */
	bool CreateThumbnailFile(UObject *object, FString pngPath);

	/** Handles retrieving data about a node
	* @Param UEdGraphNode Node to report.
	*/
	NodeType GetNodeType(UEdGraphNode* node, NodeType defaultType=NodeType::none);
	FString GetNodeTypeGroup(NodeType type);
	FString GetNodeIcon(UEdGraphNode* node);
	FString GetNodeURL(UEdGraphNode* node);
	FString GetNodeTooltip(UEdGraphNode* node);
	FString GetDelegateIcon(UEdGraphNode* node);

	/** Handles retrieving data about a pin
	* @Param UEdGraphPin Pin to report.
	*/
	bool PinShouldBeVisible(UEdGraphPin* pin);
	bool isDelegatePin(UEdGraphPin* pin);
	FString GetPinLabel(UEdGraphPin* pin);
	FString GetPinType(UEdGraphPin* pin, bool useSchema=false);
	FString GetPinTooltip(UEdGraphPin* pin);
	FString GetPinIcon(UEdGraphPin* pin);
	FString GetPinColor(UEdGraphPin* pin);
	FString GetPinPort(UEdGraphPin* pin);
	FString GetPinDefaultValue(UEdGraphPin* pin);

	/** Handles outputting the results to the log */
	virtual void LogResults();

	static TMap<FString, NodeType>					NodeType_e;
	static TMap<FString, FString>					NodeIcons;
	static TMap<FString, TPair<FString, FString> >	PinIcons;

	/** CommandLine Config Variables */
	int			outputMode;
	FString		outputDir = "-";					//use "-" for stdout
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

	void writeBlueprintHeader(UBlueprint *blueprint, FString group, FString qualifier, FString assetName, FString packageName, FString packageDescription, FString parentClass, int graphCount);
	void writeMaterialHeader(UMaterial* material, FString group, FString qualifier, FString assetName, FString packageName, FString packageDescription, FString parentClass, int graphCount);

	void writeAssetMembers(UBlueprint *object, FString which);
	void writeAssetFooter();

	void writeGraphHeader(FString prefix, UEdGraph *graph, FString qualifier);
	void writeGraphFooter(FString prefix, UEdGraph *graph);
	void writeGraphConnections(FString prefix);

	FString getNodeTemplate(NodeType type);
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
