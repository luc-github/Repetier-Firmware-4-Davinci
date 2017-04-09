#!/bin/bash

function build_sketch()
{
	local sketch=$1
	local binpath=$2
     mkdir -p binpath
	# buld sketch with arudino ide
	echo -e "\n Build $sketch \n"
	echo -e "\n in $binpath \n"
	arduino --verbose --verify --pref build.path=$binpath  $sketch

	# get build result from arduino
	local re=$?

	# check result
	if [ $re -ne 0 ]; then
		echo "Failed to build $sketch"
		return $re
	fi
}
