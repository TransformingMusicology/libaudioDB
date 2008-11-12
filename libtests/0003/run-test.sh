#! /bin/bash

#. ../test-utils.sh
#
#if [ -f testdb ]; then rm -f testdb; fi
#
## creation
#${AUDIODB} -N -d testdb
#
#stat testdb
#
## should fail (testdb exists)
#expect_clean_error_exit ${AUDIODB} -N -d testdb
#
## should fail (no db given)
#expect_clean_error_exit ${AUDIODB} -N
#
#exit 104


LD_LIBRARY_PATH=../..
export LD_LIBRARY_PATH

. ../test-utils.sh

make clean
make all

./test1 

exit_code=$?

if [ $exit_code -eq 0 ]; then
    exit 104
else
    exit -1
fi
