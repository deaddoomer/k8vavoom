#!/bin/sh

odir=`pwd`
mdir=`dirname "$0"`

cd "$mdir"
k8vlumpy vlumpy.ls
res=$?

cd "$odir"
exit $res
