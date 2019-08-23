#!/bin/bash

if [ -z "$1" ]; then
    echo "error: supply a dictionary file and a source file"
    exit
fi

if [ -z "$2" ]; then
    echo "error: supply a dictionary file and a source file"    
    exit
fi

echo "dictionary file:" $1
echo "source file:" $2

echo
echo "./zpipe_dict_zlib"
./zpipe_dict_zlib $2 < $1 > junk.zlib

echo
echo "./zpipe_dict_nx"
./zpipe_dict_nx $2 < $1 > junk.nx

echo
echo "zlib head"
xxd junk.zlib | head -1
echo "zlib tail"
xxd junk.zlib | tail -2

echo
echo "libnxz head"
xxd junk.nx | head -1
echo "libnxz tail"
xxd junk.nx | tail -2
