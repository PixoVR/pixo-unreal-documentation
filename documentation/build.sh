#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/docs-doxygen

source ../env.sh

# pre-build our config file page
echo > ../pages/generated_configs.dox
scripts/create_configs_doc.bash pages/generated_configs.dox

rm -rf ../html

doxygen Doxyfile

cd ../

