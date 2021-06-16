#!/bin/bash

SOURCEVER=4
TARGETVER=2.1.0

echo "const int SOURCEVER = $SOURCEVER;" > src/sourcever.h

mkdir release || set status 0

for PLATFORM in photon p1 electron argon boron bsom b5som tracker
do
    particle compile $PLATFORM . --target $TARGETVER --saveTo release/$PLATFORM.bin
done 
