// (c) 2023 PixoVR

#pragma once

#include "reporter.h"

class materialReporter : public reporter
{
public:
	materialReporter(FString _outputDir, FString _stylesheet, FString _groups);

	virtual void report(int &graphCount, int &ignoreCount, int &failedCount) override;

protected:

	virtual void LOG(FString verbosity) override;
	virtual void LOG(FString verbosity,FString message) override;

	virtual int reportMaterial(FString prefix, UMaterialInterface* materialInterface) override;
	virtual void reportGraph(FString prefix, UEdGraph* g) override;
	virtual void reportNode(FString prefix, UEdGraphNode* Node) override;

	virtual FString getGraphCPP(UEdGraph* graph, FString _namespace="");
	virtual bool getNodeHasBubble(UEdGraphNode *node);
	virtual FString getNodeTitle(UEdGraphNode *node, FString &title2);
	virtual TMap<FString, FString> getVisiblePins(UEdGraphNode* node) override;

	virtual bool urlCheck1(UEdGraph* &igraph, UEdGraph* &ograph, UEdGraph *originalGraph, UEdGraphNode *node) override;
	virtual bool urlCheck2(FString &blueprintClassName, FString &variableName, UEdGraph *graph, UEdGraphNode *node) override;

private:
	UMaterialInterface* currentMaterialInterface = NULL;

	void writeMaterialHeader(
		UMaterialInterface* materialInterface,
		FString group,
		FString qualifier,
		FString packageName,
		FString imageTag,
		int graphCount
	);
	void writeMaterialMembers(UMaterial* material, FString what);
};
