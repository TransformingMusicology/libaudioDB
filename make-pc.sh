#! /bin/bash

EXECUTABLE="$1"
LIBDIR="$2"
INCLUDEDIR="$3"
SOVERSION=$4
MINORVERSION=$5

echo "includedir=$INCLUDEDIR"
echo "libdir=$LIBDIR"
echo
echo "Name: $EXECUTABLE"
echo "Description: the $EXECUTABLE library"
echo "Version: $SOVERSION.$MINORVERSION"
echo "Requires.private: gsl gsoap++"
echo 'Cflags: -I${includedir}'
echo 'Libs: -L${libdir} -l'"$EXECUTABLE"