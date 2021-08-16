#!/bin/sh

odir=`pwd`
mdir=`dirname "$0"`
cd "$mdir"

gcc -o floatutiltest floatutiltest.c \
  grisu2dbl.c \
  ../ryu_f2s.c ../ryu_d2s.c \
  -Wall -O2 \
  -fno-strict-aliasing -fno-strict-overflow -fwrapv

res=$?

cd "$odir"
exit $res
