#! /bin/sh

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

# We could contemplate putting the test feature (and the expected
# query output) under svn control if we trust its binary file
# handling.

# FIXME: endianness!
intstring 1 > testfeature
floatstring 1 >> testfeature

${AUDIODB} -d testdb -I -f testfeature

${AUDIODB} -d testdb -Q point -f testfeature > test-query-output

echo testfeature 1 0 0 > test-expected-query-output

cmp test-query-output test-expected-query-output

# failure cases
expect_clean_error_exit ${AUDIODB} -d testdb -I
expect_clean_error_exit ${AUDIODB} -d testdb -f testfeature
expect_clean_error_exit ${AUDIODB} -I -f testfeature
expect_clean_error_exit ${AUDIODB} -d testdb -Q notpoint -f testfeature
expect_clean_error_exit ${AUDIODB} -Q point -f testfeature

exit 104
