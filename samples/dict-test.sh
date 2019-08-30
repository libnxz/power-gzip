#!/bin/bash

if [ -z "$1" ]; then
    echo "error: supply a dictionary file and a source file"
    exit
fi

if [ -z "$2" ]; then
    echo "error: supply a dictionary file and a source file"    
    exit
fi

echo "   dictionary file:" $1
echo "   source file:" $2

echo
echo "deflate with ./zpipe_dict_zlib"
./zpipe_dict_zlib $1 < $2 > junk.zlib

echo "deflate with ./zpipe_dict_nx"
./zpipe_dict_nx $1 < $2 > junk.nx

echo "   File sizes are"
ls -l junk.zlib junk.nx

echo "   zlib head"
xxd junk.zlib | head -2
echo "   zlib tail"
xxd junk.zlib | tail -2

echo "   libnxz head"
xxd junk.nx | head -2
echo "   libnxz tail"
xxd junk.nx | tail -2

echo "   inflate the junk.zlib file two ways"
echo

echo "   ./zpipe_dict_zlib"
./zpipe_dict_zlib $1 -d < junk.zlib > junk.zlib.txt.zlib

echo "   ./zpipe_dict_nx"
./zpipe_dict_nx $1 -d < junk.zlib > junk.zlib.txt.nx
echo "   checksums of inflated"
sha1sum junk.zlib.txt.zlib junk.zlib.txt.nx

echo "   inflate the junk.nx file two ways"
echo

echo "   ./zpipe_dict_zlib"
./zpipe_dict_zlib $1 -d < junk.nx > junk.nx.txt.zlib

echo "   ./zpipe_dict_nx"
./zpipe_dict_nx $1 -d < junk.nx > junk.nx.txt.nx
echo "   checksums of inflated"
sha1sum junk.nx.txt.zlib junk.nx.txt.nx

