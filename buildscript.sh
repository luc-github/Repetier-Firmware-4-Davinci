#!/bin/bash


if [ "$DAVINCI" = "1M0" ]; then
	echo "Davinci 1 Model 0"
	sed -i 's/#define DAVINCI 0 /#define DAVINCI 1 /g' ./src/ArduinoDUE/Repetier/Configuration.h
	sed -i 's/#define DAVINCI 2 /#define DAVINCI 1 /g' ./src/ArduinoDUE/Repetier/Configuration.h
	sed -i 's/#define DAVINCI 3 /#define DAVINCI 1 /g' ./src/ArduinoDUE/Repetier/Configuration.h
	sed -i 's/#define DAVINCI 4 /#define DAVINCI 1 /g' ./src/ArduinoDUE/Repetier/Configuration.h
	sed -i 's/#define MODEL  1/#define MODEL  0/g' ./src/ArduinoDUE/Repetier/Configuration.h
else
	if [ "$DAVINCI" = "2M0" ]; then
	echo "Davinci 2 Model 0"
	sed -i 's/#define DAVINCI 0 /#define DAVINCI 2 /g' ./src/ArduinoDUE/Repetier/Configuration.h
	sed -i 's/#define DAVINCI 1 /#define DAVINCI 2 /g' ./src/ArduinoDUE/Repetier/Configuration.h
	sed -i 's/#define DAVINCI 3 /#define DAVINCI 2 /g' ./src/ArduinoDUE/Repetier/Configuration.h
	sed -i 's/#define DAVINCI 4 /#define DAVINCI 2 /g' ./src/ArduinoDUE/Repetier/Configuration.h
	sed -i 's/#define MODEL  1/#define MODEL  0/g' ./src/ArduinoDUE/Repetier/Configuration.h
else
	echo "Unknown model: $DAVINCI"
if
if

