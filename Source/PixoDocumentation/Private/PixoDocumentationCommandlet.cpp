// (c) 2023 PixoVR

#include "PixoDocumentationCommandlet.h"

#include "Runtime/Launch/Resources/Version.h"

//#include "ObjectTools.h"
//#include "Misc/Paths.h"

#include "PixoDocumentation.h"

/**
 * @brief UPixoDocumentationCommandlet::UPixoDocumentationCommandlet
 * @param ObjectInitializer
 *
 * The commandlet wrapper for the PixoDocumentation plugin
 */

UPixoDocumentationCommandlet::UPixoDocumentationCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, outputMode(OutputMode::none)
	, outputDir("-")
{
	//IsClient = false;
	//IsEditor = false;
	//IsServer = false;
	//LogToConsole = true;
}

/**
 * @brief UPixoDocumentationCommandlet::Main
 * @param Params The commandline parameters used to call the commandlet.
 * @return The status of the commandlet when it is run.  Will return 0 on success, or some other value if there was a failure.
 *
 * The parameters can be listed by calling the commandlet with the -help flag, or -usage, or -h, or '?'.
 */

int32 UPixoDocumentationCommandlet::Main(const FString& Params)
{
	HelpUsage = "Commandlet for generating doxygen dot syntax files.\n\n"
		"Once Generated, these can be processed by doxygen for publishing.  "
		"You will want to point your OutputDir to a folder being parsed by "
		"doxygen with the rest of your plugin source code, as links in blueprints "
		"will point to other areas of your code.";

	HelpParamNames = {
		"OutputMode",
		"OutputDir",
		"Stylesheet",
		"Groups"
	};

	HelpParamDescriptions = {
		"One of: doxygen|verbose|debug.  If not present, execution will halt.",
		"Path to the output directory, which must exist when this is run.  If not provided, '-' will be used, which means stdout.",
		"The name of a css stylesheet for dot files, which will be embedded into the resulting dot-syntax comments. (default: 'doxygen-pixo.css')",
		"The name of the groups file, which will contain a gallery of images parsed from the .uasset files. (default: 'groups.dox')"
	};

	HelpWebLink = "https://docs.pixovr.com";

	InitCommandLine(Params);

	if (outputMode == OutputMode::none || usage)
	{
		UE_LOG(LOG_DOT, Error, TEXT("%s"), *HelpUsage);
		UE_LOG(LOG_DOT, Error, TEXT("Usage:"));
		for (int i=0; i<HelpParamNames.Num(); i++)
		{
			UE_LOG(LOG_DOT, Error, TEXT(" -%s : %s"),*HelpParamNames[i],*HelpParamDescriptions[i]);
		}

		//UE_LOG(LOG_DOT, Error, TEXT("Usage flags:"));
		//UE_LOG(LOG_DOT, Error, TEXT("	\"-OutputMode=verbose|debug|doxygen\" (where doxygen produces c++ for doxygen parsing)"));
		//UE_LOG(LOG_DOT, Error, TEXT("	\"-OutputDir=-|[path]\" (where - means stdout, which is the default. The directory will not be cleared first.)"));
		//UE_LOG(LOG_DOT, Error, TEXT("	\"-Stylesheet=[url]\" (defaults to 'doxygen-pixo.css')"));
		//UE_LOG(LOG_DOT, Error, TEXT("	\"-Groups=[filename.dox]\" (defaults to 'groups.dox')"));
		return 1;
	}

	PixoDocumentation pd(
		outputMode,
		outputDir,
		stylesheet,
		groups
	);

	int32 result = pd.report();

	return result;
}

/**
 * @brief UPixoDocumentationCommandlet::InitCommandLine
 * @param Params The command line parameters
 *
 * This will set the member variables passed to the PixoDocumentation
 * object, which will in turn create the blueprintReporter and materialReporter
 * or whatever things are being reported.
 */

void UPixoDocumentationCommandlet::InitCommandLine(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> SwitchParams;
	ParseCommandLine(*Params, Tokens, Switches, SwitchParams);

	if (Tokens.Contains("help") ||
		Tokens.Contains("h") ||
		Tokens.Contains("?") ||
		Tokens.Contains("usage") ||
		Switches.Contains("help") ||
		Switches.Contains("h") ||
		Switches.Contains("?") ||
		Switches.Contains("usage")	)
		usage = true;

	if (SwitchParams.Contains(TEXT("Stylesheet")))
		stylesheet = *SwitchParams[TEXT("Stylesheet")];

	if (SwitchParams.Contains(TEXT("OutputDir")))
	{	outputDir = *SwitchParams[TEXT("OutputDir")];
		outputDir.TrimStartAndEndInline();			//Sorry if you want a directory ending with a space!
		outputDir.RemoveFromEnd("/");				//remove dir slash if present
		outputDir.RemoveFromEnd("\\");				//remove dir slash if present
		//printf("output dir: %s\n", TCHAR_TO_UTF8(*outputDir));
	}

	if (SwitchParams.Contains(TEXT("Groups")))
		groups = *SwitchParams[TEXT("Groups")];

	if (SwitchParams.Contains(TEXT("OutputMode")))
	{
		outputMode = OutputMode::none;		//reset

		FString m = SwitchParams[TEXT("OutputMode")];
		OutputMode e = OutputMode_e.FindOrAdd(m,OutputMode::none);

		switch (e)
		{
		case OutputMode::debug:
			outputMode |= OutputMode::debug;
			break;

		case OutputMode::doxygen:
			outputMode |= OutputMode::doxygen;
			break;

		case OutputMode::verbose:
			outputMode |= OutputMode::verbose;
			outputMode |= OutputMode::debug;
			break;

		default:
			UE_LOG(LOG_DOT, Warning, TEXT("Unknown OutputMode: '%s'..."), *m);
			break;
		}

		//printf("output mode %x\n", outputMode);
	}
	else
		UE_LOG(LOG_DOT, Error, TEXT("No OutputMode specified."));
}







