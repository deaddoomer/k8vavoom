#!/bin/sh

if [ "z$2" = "z" ]; then
  exit 1
fi

n0="$2"
n1=`basename "$n0"`
n2=`basename "$n1" .fossil`
#echo "n0=$n0"
#echo "n1=$n1"
#echo "n2=$n2"
if [ "z$n1" != "z$n2" ]; then
  echo "FATAL: CANNOT OVERWRITE FOSSIL REPO!"
  exit 1
fi

echo "OUT: $n0"
#exit 1

if [ "z$1" = "z" ]; then
  echo '#define FOSSIL_COMMIT_HASH  "unknown"' >"$n0"
else
  hh=`fossil time -R "$1" -n 1 -F "%H" -t ci | head -c 42`
  res=$?
  if [ $res -ne 0 ]; then
    echo "FOSSIL: ALAS!"
    exit $res
  fi
  echo "#define FOSSIL_COMMIT_HASH  \"${hh}\"" >"$n0"
fi
