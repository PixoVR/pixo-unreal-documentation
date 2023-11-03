// (c) 2023 PixoVR

#pragma once

#include <iostream>
#include <fstream>
using namespace std;

#include "DocUtils.h"
using namespace DocUtils;

#include "CoreMinimal.h"
//#include "AssetRegistry/AssetData.h"

#include "K2Node.h"
#include "K2Node_Composite.h"
#include "K2Node_Variable.h"
//#include "K2Node_VariableSet.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
//#include "K2Node_SpawnActor.h"
//#include "K2Node_ConstructObjectFromClass.h"

#include "Kismet2/BlueprintEditorUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LOG_DOT, Log, All);

/**
 * @brief The reporter base class
 *
 * This base class, if called directly with parameters, will output
 * debug reports that are not useful to doxygen.
 *
 * Better to use the PixoDocumentation access method.
 *
 * \sa blueprintReporter
 * \sa materialReporter
 */

class reporter
{
public:
	reporter(FString type, FString _outputDir, FString _stylesheet, FString _groups);
	virtual ~reporter();

#if ENGINE_MAJOR_VERSION >= 5
	virtual void loadAssetsByPath(FTopLevelAssetPath loadPath);	//UE5.1
#else
	virtual void loadAssets(FName loadClass);		//UE4.27
#endif

	virtual void report(int &graphCount, int &ignoredCount, int &failedCount);

	static TArray<FString>		GroupList;		//the list of groups reported
	static TArray<FString>		IgnoreFolders;		//folders to ignore
	static TArray<FString>		IncludeFolders;		//folders to include

protected:
	FName				reportClassName;
	FString				reportType;
	FString				stylesheet;
	FString				groups;
	FString				outputDir = "-";	//use "-" for stdout
	FString				currentDir = "";

	TArray<FAssetData>		assetList;

	vmap				NodeStyle;

	TMap<UEdGraph*, FString>	GraphDescriptions;	//assuming parent graphs are parsed before children.  This is the description provided in the collapse node of the parent.
	TMap<FString, TArray<FString>>	GraphCalls;		//any node (url) mentioned in a graph is appended to the call graph.
	TArray<FString>			GalleryList;		//list of image entries for the gallery.  Should be cleared before each group (Blueprint/Material/etc.)
	TMap<FString, FString>		pinConnections;		//intended to be [SOURCE:port:_ -- DEST:port:_] [color], which forces uniqueness of connections despite direction

	virtual void LOG(FString message);
	virtual void LOG(FString verbosity,FString message);

	virtual bool shouldReportAsset(FAssetData const& Asset);
	virtual bool shouldReportObject(UObject* object);

	virtual bool createThumbnailFile(UObject* object, FString pngPath);

	virtual int reportBlueprint(FString prefix, UBlueprint* Blueprint);
	virtual int reportMaterial(FString prefix, UMaterialInterface* materialInterface);
	virtual void reportGroup(FString groupName, FString groupNamePretty, FString brief, FString details);

	virtual void reportGraph(FString prefix, UEdGraph* g);
	virtual void reportNode(FString prefix, UEdGraphNode* Node);
	virtual void writeNodeBody(FString prefix, UEdGraphNode *node);

	virtual FString getGraphCPP(UEdGraph* graph, FString _namespace="");
	virtual bool getNodeHasBubble(UEdGraphNode *node);
	virtual FString getNodeTitle(UEdGraphNode *node, FString &title2);
	virtual TMap<FString, FString> getVisiblePins(UEdGraphNode* node);

	virtual void writeGraphHeader(FString prefix, UEdGraph* graph, FString qualifier);
	virtual void writeGraphFooter(FString prefix, UEdGraph *graph);
	virtual void writeGraphConnections(FString prefix);

	virtual FString prepNodePortRows(FString prefix, UEdGraphNode* node, TMap<FString, FString> visiblePins = TMap<FString, FString>());

	virtual void writeAssetFooter();
	virtual void writeAssetCalls(FString className);

	//node stuff
	FString getNodeURL(UEdGraphNode* node, EEdGraphPinDirection direction = EEdGraphPinDirection::EGPD_MAX);
	virtual bool urlCheck1(UEdGraph* &igraph, UEdGraph* &ograph, UEdGraph *originalGraph, UEdGraphNode *node);
	virtual bool urlCheck2(FString &blueprintClassName, FString &variableName, UEdGraph *graph, UEdGraphNode *node);

	//files
	virtual bool openFile(FString fpath, bool append=false);
	virtual bool closeFile();

	FString		_tab = "\t";				// depth for indent.  Should be '\t'.
	float		_dpi = 96.0f;				// dpi scaling, but only for POS, not SIZE
	float		_scale = 72.0f / 96.0f;			// scaling for width and height, which are presented in different DPI

	std::wostream	*out = NULL;				// our output stream, which will default to std::wcout
	std::wofstream	*outfile = NULL;			// our output file, which will default to NULL unless opened

private:
	//verbose only
	void reportPin(FString prefix, UEdGraphPin* p);
};
