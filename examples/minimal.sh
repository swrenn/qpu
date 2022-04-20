#!/bin/bash

NAME=minimal
vc4asm -V $NAME.qasm -o $NAME.bin
sudo qpu execute i $NAME.bin
rm $NAME.bin
