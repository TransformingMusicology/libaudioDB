#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} > test-output
grep help test-output > /dev/null

exit 104
