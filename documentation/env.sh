#!/bin/bash

export PROJECT_NAME='Pixo Unreal BP Docs'				# should be short enough to fit on the documentation page
export PROJECT_VERSION=`cd ../; git tag | tail -n 1`			# the last tagged version
export PROJECT_BRIEF='Unreal blueprints and materials in doxygen.'	# a brief description of the SDK or module
export PROJECT_STATUS='active'						# project status, for publishing.  Recognized choices are 'active' (default if not present), 'inactive' (invisable and won't appear in the documentation menu), 'deprecated' (visible in menu, but marked deprecated)
export PROJECT_LOGO='docs-doxygen/doxygen-custom/defaultIcon.png'	# an icon for the submodule, or 'docs-doxygen/doxygen-custom/defaultIcon.png'.  Path is relative to the folder containing `build.sh`.
export PROJECT_REPO='https://github.com/PixoVR/pixo-unreal-documentation' # the project repo url, for cloning
export PROJECT_URL='/UnrealDocumentation'				# subfolder used for documentation: 'https://docs.pixovr.com/SomeSDK-Target', like 'ApexSDK-Unreal'
export DEV_PROJECT_URL='../../../../Unreal/pixo-unreal-documentation/documentation/html/index.html'	# a url for local development, which is used when `docs-root` is publishing on a local system via `DEV=true ./build.sh`.  This will usually just be to replace `SomeSDK-Target` with the repo name.
export PROJECT_MAIN_PAGE='../pages/mainpage.md'				# the main home markdown page for the documentation

export DOXYGEN_FILTER='scripts/unreal_filter.py'			# a script filter for interpreting the code (adds stuff like decorators or macros)
export DOXYGEN_INPUT='  "../../Source"'					# a list of input folders for documenting, which is a whitespace-separated list of (optionally) quoted paths.  The Doxyfile will already include "../pages" for you.  Paths are relative to the `docs-doxygen` folder.
export DOXYGEN_IMAGES=''						# a list of root folders to look for images
export DOXYGEN_EXCLUDE=''						# a list of paths to exclude when building.
export DOXYGEN_STRIP_FROM_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../" && pwd)"	# the full absolute path to the root folder to be removed during publish.  This is kind of cosmetic but helps reduce confusion when finding libraries.
export DOXYGEN_IGNORE_PREFIX='FPixo'					# a class/method/variable prefix to be ignored when alphabetizing.  For instance if everything is pApexSDK, mMatrix, etc, the ignore prefix may want to be "p m" where the list is a whitespace-separated list of prefixes.  The ignore order matters, where longer entries should be first.

# user variables.  Useful when writing pages that refer to other urls, but don't want to hardcode them in.
export PROJECT_BUILDER_REPO='https://github.com/PixoVR/docs-docker-ue4-base'

