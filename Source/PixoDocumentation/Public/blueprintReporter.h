// (c) 2023 PixoVR

#pragma once

#include "reporter.h"

class blueprintReporter : public reporter
{
public:
	blueprintReporter(FString _outputDir, FString _stylesheet, FString _groups);

	virtual void report(int &graphCount, int &ignoredCount, int &failedCount) override;

protected:

	virtual void LOG(FString verbosity) override;
	virtual void LOG(FString verbosity,FString message) override;

	virtual int reportBlueprint(FString prefix, UBlueprint* Blueprint) override;
	virtual void reportGraph(FString prefix, UEdGraph* g) override;
	virtual void reportNode(FString prefix, UEdGraphNode* Node) override;

private:
	void writeBlueprintHeader(
		UBlueprint *blueprint,
		FString group,
		FString qualifier,
		FString packageName,
		int graphCount
	);
	void writeBlueprintMembers(UBlueprint* blueprint, FString what);
};
