#!/bin/bash

NAME=input_tmu

if [ -z $1 ]; then
    echo 'Missing argument'
    exit 1
else
    echo -n $1 > $NAME.data
    truncate -s 4 $NAME.data
fi

vc4asm -V $NAME.qasm -o $NAME.bin
sudo qpu execute i $NAME.bin r $NAME.data w $((16*4)) | xxd -e
rm $NAME.bin $NAME.data
