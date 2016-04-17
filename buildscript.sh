#!/bin/bash

cd $TRAVIS_BUILD_DIR
source command.sh
export PATH="$HOME/arduino_ide:$PATH"
arduino --board arduino:sam:arduino_due_x --save-prefs
arduino --get-pref sketchbook.path

if [ "$DAVINCI" = "1M0" ]; then
	echo "Davinci Model 0"
else
	echo "Unknown model: $DAVINCI"
if

build_sketch ./src/ArduinoDUE/Repetier/Repetier.ino
