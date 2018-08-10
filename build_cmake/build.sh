#! /bin/bash -e
SCRIPT_DIR=`dirname $0`
cd $SCRIPT_DIR

cmake .. 

core_count=`getconf _NPROCESSORS_ONLN`
make -j `expr $core_count + 1`
