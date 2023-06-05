
TEMPLATE = aux
CONFIG += c++14
CONFIG += console
CONFIG += app_bundle
CONFIG -= qt

TARGET = pixoDocumentation

unix:!macx:
{
	unrealRootPath="/staticmount/projects2/Unreal Engine/src/UnrealEngine"
	build=bash $$unrealRootPath/Engine/Build/BatchFiles/Linux/Build.sh
}

macx:
{
	unrealRootPath="/Volumes/projects2/Unreal Engine/src/UnrealEngine"
	build=bash $$unrealRootPath/Engine/Build/BatchFiles/Mac/Build.sh
}

#include(.qmake/UE5Source.pri)
#include(.qmake/UE5Header.pri)
#include(.qmake/UE5Config.pri)
#include($$unrealRootPath/.qmake/UE5Includes.pri)
#include(.qmake/UE5Defines.pri)

include(UE5Includes.pri)

INCLUDEPATH +=	Source/PixoDocumentation/Private \
		Source/PixoDocumentation/Public

HEADERS +=	Source/PixoDocumentation/Private/*.h \
		Source/PixoDocumentation/Public/*.h

SOURCES +=	Source/PixoDocumentation/Private/*.cpp \
		Source/PixoDocumentation/Public/*.cpp

OTHER_FILES +=	Source/PixoDocumentation/PixoDocumentation.Build.cs \
		PixoDocumentation.uplugin \
		README.md \
		Makefile

