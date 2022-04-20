#!/bin/bash

NAME=elem_num
vc4asm -V $NAME.qasm -o $NAME.bin
sudo qpu execute i $NAME.bin w $((16*4)) | xxd -e
rm $NAME.bin
