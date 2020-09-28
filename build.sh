#!/bin/bash

SOURCEVER=1
TARGETVER=1.5.2

echo "const int SOURCEVER = $SOURCEVER;" > src/sourcever.h

for PLATFORM in photon p1 electron argon boron bsom b5som
do
    particle compile $PLATFORM . --target $TARGETVER --saveTo release/$PLATFORM.bin
done 

# Tracker is currently a special case that requires 1.5.4-rc.1
particle compile tracker . --target 1.5.4-rc.1 --saveTo release/tracker.bin
