#!/bin/sh

gcc -o floatutiltest floatutiltest.c grisu2dbl.c \
  -Wall -O2 \
  -fno-strict-aliasing -fno-strict-overflow -fwrapv
