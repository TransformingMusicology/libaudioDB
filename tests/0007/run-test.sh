#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

# tests that the lack of -l when the query sequence is shorter doesn't
# segfault.

intstring 2 > testfeature
floatstring 0 1 >> testfeature
floatstring 1 0 >> testfeature

${AUDIODB} -d testdb -I -f testfeature

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

echo "query point (0.0,0.5)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery

expect_clean_error_exit ${AUDIODB} -d testdb -Q sequence -f testquery
expect_clean_error_exit ${AUDIODB} -d testdb -Q sequence -f testquery -n 1

echo "query point (0.5,0.0)"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

expect_clean_error_exit ${AUDIODB} -d testdb -Q sequence -f testquery
expect_clean_error_exit ${AUDIODB} -d testdb -Q sequence -f testquery -n 1

exit 104
