// (c) 2023 PixoVR

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "PixoDocumentationCommandlet.generated.h"

/**
 * Commandlet for generating dot syntax diagrams for all blueprints and materials
 * found in a .uproject.
 *
 * We want to document plugin blueprints and materials, so a plugin can be
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
 * ./UnrealEditor-Cmd.exe "X:\PixoVR\Documentation_5_0\Documentation_5_0.uproject" -nullrhi -nopause -nosplash -run=PixoDocumentation -LogCmds="global none, LOG_DOT all" -NoLogTimes -OutputMode=doxygen -OutputDir="X:\PixoVR\documentation\docs-root\documentation\pages\blueprints2"
 * ... and without the end parameters for usage message.
 *
 * ./UnrealEditor-Cmd.exe "X:\PixoVR\Documentation_5_0\Documentation_5_0.uproject" -run=PixoDocumentation -LogCmds="global none, LOG_DOT all" -NoLogTimes -OutputMode=doxygen -OutputDir="X:\PixoVR\documentation\docs-root\documentation\pages\blueprints2"
 */
UCLASS()
class UPixoDocumentationCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	UPixoDocumentationCommandlet(const FObjectInitializer& ObjectInitializer);

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface

protected:

	//main control
	virtual void InitCommandLine(const FString& Params);

private:
	bool		usage = false;
	int			outputMode;
	FString		outputDir = "-";			//use "-" for stdout
	TArray<FString>	includes;
	FString		stylesheet = "doxygen-pixo.css";
	FString		groups = "groups.dox";			// filename for groups file

};
