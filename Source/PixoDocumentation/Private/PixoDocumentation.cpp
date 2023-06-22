
#include "PixoDocumentation.h"

#include "reporter.h"
#include "blueprintReporter.h"
#include "materialReporter.h"

using namespace PixoUtils;

/**
 * @brief Constructor for PixoDocumentation.
 * @param _outputMode The output mode: one of: `doxygen`, `verbose`, `debug`.
 * @param _outputDir The path to an output destination.  Use '-' for stdout.
 * @param _includes The array of allowed asset paths.
 * @param _stylesheet The path/name of a css stylesheet, which doxygen will embed a link to in svg files.
 * @param _groups The name of the groups file, which will default to 'groups.dox'
 *
 * After constructing this object, you will need to call report() to execute it.
 */

PixoDocumentation::PixoDocumentation(
	int _outputMode,
	FString _outputDir,
	TArray<FString> _includes,
	FString _stylesheet,
	FString _groups
)
: outputMode(_outputMode)
, outputDir(_outputDir)
, includes(_includes)
, stylesheet(_stylesheet)
, groups(_groups)
, totalGraphsProcessed(0)
, totalBlueprintsIgnored(0)
, totalMaterialsIgnored(0)
, totalNumFailedLoads(0)
{
	reporter::IgnoreFolders.Empty();
	reporter::IgnoreFolders.AddUnique("/Engine");
	reporter::IgnoreFolders.AddUnique("/Game");

	//IgnoreFolders.Add("/PixoDocumentation");	//ignore our own plugin when not debugging

	reporter::IncludeFolders.Empty();					//clear this on each run
	for (FString i : includes)
	{
		reporter::IncludeFolders.AddUnique(i);
	}

	//reporter::IgnoreFolders.Add("/Bridge");				//plugin
	//reporter::IgnoreFolders.Add("/SpeedtreeImporter");	//plugin
	//reporter::IgnoreFolders.Add("/ResonanceAudio");		//plugin
	//reporter::IgnoreFolders.Add("/MediaCompositing");		//plugin
	//reporter::IgnoreFolders.Add("/AnimationSharing");		//plugin

	if (outputMode & doxygen)
		wcout << "Output Directory: " << TCHAR_TO_ANSI(*outputDir) << endl;
	else
		UE_LOG(LOG_DOT, Display, TEXT("Output Directory: %s"), *outputDir);
}

PixoDocumentation::~PixoDocumentation()
{
}

/**
 * @brief PixoDocumentation::report
 * @return The status of reporting.  Will be 0 on success or the number of read errors.
 *
 * This will use the variables defined in the constructor.
 */

int32 PixoDocumentation::report()
{
	blueprintReporter br(outputDir,stylesheet,groups);
	materialReporter mr(outputDir,stylesheet,groups);

	if (!clearGroups())
		return 1;

	if (outputMode & doxygen)
	{
		br.report(totalGraphsProcessed,totalBlueprintsIgnored,totalNumFailedLoads);
		mr.report(totalGraphsProcessed,totalMaterialsIgnored,totalNumFailedLoads);
	}

	if (outputMode & verbose ||
		outputMode & debug)
	{
		reporter r("verbose",outputDir,stylesheet,groups);

		r.report(totalGraphsProcessed,totalBlueprintsIgnored,totalNumFailedLoads);
	}

	reportResults();

	return totalNumFailedLoads;
}

void PixoDocumentation::reportResults()
{
	FString results = FString::Printf(
	TEXT(
		"======================================================================================\n"
		"Examined %d graphs(s).\n"
		"Ignored %d blueprints(s).\n"
		"Ignored %d materials(s).\n"
		"Report Completed with %d assets that failed to load.\n"
		"======================================================================================\n"),
		totalGraphsProcessed,
		totalBlueprintsIgnored,
		totalMaterialsIgnored,
		totalNumFailedLoads
	);

	if (outputMode & doxygen)
		wcout << *results;

	if (outputMode & verbose)
	{
		//results output
		UE_LOG(LOG_DOT,
			Display,
			TEXT("%s"),
			*results
		);
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

bool PixoDocumentation::clearGroups()
{
	//only in doxygen mode
	if (!(outputMode & OutputMode::doxygen))
		return true;

	if (outputDir == "-")
		return true;

	reporter::GroupList.Empty();

	FString fpath = outputDir + "/" + groups;
	FPaths::MakePlatformFilename(fpath);

	std::ofstream::openmode mode = std::ofstream::out | std::ofstream::trunc;
	std::wofstream stream;

	//stream.open(*fpath, mode);

	std::string sfpath(TCHAR_TO_UTF8(*fpath));
	stream.open(sfpath, mode);

	if (stream.is_open())
	{
		FString tpath = fpath;
		tpath.RemoveFromStart(outputDir);
		tpath = "[OutputDir]" + tpath;

		//UE_LOG(LOG_DOT, Warning, TEXT("Clearing '%s'..."), *fpath);
		wcout << " Clearing  " << *tpath << endl;
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
