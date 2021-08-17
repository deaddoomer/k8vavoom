#!/bin/sh

odir=`pwd`
mdir=`dirname "$0"`
cd "$mdir"

rm -rf obj 2>/dev/null
mkdir obj

#gcc -c -o zmimalloc.o ../mimalloc/static.c
#res=$?
#if [ $res ne 0 ]; then
#  exit $res
#fi

g++ -g -DVAVOOM_DISABLE_MIMALLOC -fno-strict-aliasing -pthread -Wall -Wno-ignored-attributes -o map_test -Wall -O2 map_test.cpp ../zone.cpp #zmimalloc.o

res=$?

cd "$odir"
exit $res
