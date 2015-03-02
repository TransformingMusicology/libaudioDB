#! /bin/bash

LIBNAME="$1"
LIBDIR="$2"
INCLUDEDIR="$3"
SOVERSION=$4
MINORVERSION=$5

echo "includedir=$INCLUDEDIR"
echo "libdir=$LIBDIR"
echo
echo "Name: $LIBNAME"
echo "Description: the $LIBNAME library"
echo "Version: $SOVERSION.$MINORVERSION"
echo "Requires.private: gsl"
echo 'Cflags: -I${includedir}'
echo 'Libs: -L${libdir} -l'"$LIBNAME"
