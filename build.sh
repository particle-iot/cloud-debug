#!/bin/bash

mkdir release || set status 0

for PLATFORM in photon p1 electron 
do
    particle compile $PLATFORM . --target 3.3.1 --saveTo release/$PLATFORM.bin
done 

for PLATFORM in argon boron bsom b5som tracker esomx p2 msom
do
    particle compile $PLATFORM . --target 6.3.4 --saveTo release/$PLATFORM.bin
done 
