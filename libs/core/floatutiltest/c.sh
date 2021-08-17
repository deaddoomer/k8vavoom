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
if [ $res -ne 0 ]; then
  cd "$odir"
  exit $res
fi

gcc -o floatparsetest floatparsetest.c \
  grisu2dbl.c \
  ../ryu_f2s.c ../strtod_plan9.c \
  -Wall -O2 \
  -fno-strict-aliasing -fno-strict-overflow -fwrapv \
  -fopenmp
res=$?
if [ $res -ne 0 ]; then
  cd "$odir"
  exit $res
fi

gcc -o floatparse floatparse.c \
  grisu2dbl.c \
  ../ryu_f2s.c ../strtod_plan9.c \
  -Wall -O2 \
  -fno-strict-aliasing -fno-strict-overflow -fwrapv
res=$?

cd "$odir"
exit $res
