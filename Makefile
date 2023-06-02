
UE_ENGINE = /mnt/c/Program Files/Epic Games/UE_5.0/Engine
WUE_ENGINE = C:\\Program\ Files\\Epic\ Games\\UE_5.0\\Engine

# run
UECMD = ./Binaries/Win64/UnrealEditor-Cmd.exe
COMMANDLET = BlueprintDoxygen
PROJECT = X:\PixoVR\Documentation_5_0\Documentation_5_0.uproject
LOGGING = -LogCmds="global none, LOG_DOT all" -NoLogTimes
OUTPUT_DIR = X:\PixoVR\documentation\docs-root\documentation\pages\blueprints
OUTPUT_MODE ?= doxygen

# build
UATCMD = Build\\BatchFiles\\RunUAT.bat
PLUGIN_ROOT = X:\\PixoVR\\Documentation_5_0\\Plugins\\pixo-unreal-documentation\\
PLUGIN = $(PLUGIN_ROOT)\\PixoBlueprintDocumentation.uplugin
PLATFORM = Win64
#PACKAGE = X:\\PixoVR\\dev\\SDKs\\temp
PACKAGE = C:\\temp\\build

all: run

build:
	@cmd.exe /k $(WUE_ENGINE)\\$(UATCMD) BuildPlugin -plugin="$(PLUGIN)" -package="$(PACKAGE)" -TargetPlatforms=$(PLATFORM) && exit || exit

run:
	@make commandlet OUTPUT_MODE=doxygen

debug:
	@make commandlet OUTPUT_MODE=debug 

verbose:
	@make commandlet OUTPUT_MODE=verbose

commandlet:
	@cd "$(UE_ENGINE)" && $(UECMD) "$(PROJECT)" -run=$(COMMANDLET) $(LOGGING) -OutputMode=$(OUTPUT_MODE) -OutputDir="$(OUTPUT_DIR)" || true

