// (c) 2023 PixoVR

#pragma once

#include "CoreMinimal.h"

/**
 * @brief The PixoDocumentation class
 *
 * This will examine loaded .uasset files for blueprints and materials,
 * generating dot-syntax pseudo code for doxygen to parse, creating
 * a .dot and therefore an svg representation of these graphs.
 *
 * Once, generated, internal links to other aspects of the code in a plugin
 * will be linked and browsable.
 *
 * The intention is for this to be run on a single plugin, however, by
 * providing multiple plugins (or even a whole project), one could document
 * much more in a single go.
 *
 * The thing(s) to publish are provided in the constructor parameter (FString)
 * reporter::IncludeFolders.  reporter::IgnoreFolders can be
 * used to exclude items.  Usually you would include a root folder and exclude
 * subfolders, as the default is to exclude.
 */

class PixoDocumentation
{
public:
	PixoDocumentation(
		int		_outputMode,
		FString	_outputDir,
		TArray<FString> _includes,
		FString	_stylesheet,
		FString	_groups
	);
	virtual ~PixoDocumentation();

	int32 report();

protected:
	virtual bool clearGroups();
	virtual void reportResults();

private:
	//command line variables
	int			outputMode;
	FString		outputDir = "-";			//use "-" for stdout
	FString		currentDir = "";
	TArray<FString> includes;
	FString		stylesheet = "doxygen-pixo.css";	// style applied to dot/svg
	FString		groups = "groups.dox";			// filename for groups file

	/** Variables to store overall results */
	int totalGraphsProcessed;
	int totalBlueprintsIgnored;
	int totalMaterialsIgnored;
	int totalNumFailedLoads;

	//TODO: activate this?
	TArray<FString> AssetsWithErrorsOrWarnings;
};
