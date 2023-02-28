#!/bin/bash

SOURCEVER=5

echo "const int SOURCEVER = $SOURCEVER;" > src/sourcever.h

mkdir release || set status 0

for PLATFORM in photon p1 electron 
do
    particle compile $PLATFORM . --target 2.3.0 --saveTo release/$PLATFORM.bin
done 

for PLATFORM in argon boron bsom b5som tracker esomx
do
    particle compile $PLATFORM . --target 4.0.0 --saveTo release/$PLATFORM.bin
done 

for PLATFORM in p2
do
    particle compile $PLATFORM . --target 5.0.1 --saveTo release/$PLATFORM.bin
done 
