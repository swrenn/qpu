#!/bin/bash

NAME=hello_world
vc4asm -V $NAME.qasm -o $NAME.bin
sudo qpu execute i $NAME.bin w $((12*16*4)) | iconv -f utf-32 && echo
rm $NAME.bin
