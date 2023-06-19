#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/docs-doxygen

source ../env.sh

rm -rf ../html

doxygen Doxyfile

cd ../

