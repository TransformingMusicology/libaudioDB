#! /bin/sh

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

start_server ${AUDIODB} 10017

echo "query point (0.0,0.5)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery

# FIXME: this actually revealed a horrible failure mode of the server:
# since we were throwing exceptions from the constructor, the
# destructor wasn't getting called and so we were retaining 2Gb of
# address space, leading to immediate out of memory errors for the
# /second/ call.  We fix that by being a bit more careful about our
# exception handling and cleanup discipline, but how to test...?

expect_client_failure ${AUDIODB} -c localhost:10017 -d testdb -Q sequence -f testquery
expect_client_failure ${AUDIODB} -c localhost:10017 -d testdb -Q sequence -f testquery -n 1

check_server $!

echo "query point (0.5,0.0)"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

expect_client_failure ${AUDIODB} -c localhost:10017 -d testdb -Q sequence -f testquery
expect_client_failure ${AUDIODB} -c localhost:10017 -d testdb -Q sequence -f testquery -n 1

check_server $!

# see if the server can actually produce any output at this point
${AUDIODB} -c localhost:10017 -d testdb -Q sequence -l 1 -f testquery -n 1 > testoutput
echo testfeature 0 0 1 > test-expected-output
cmp testoutput test-expected-output

stop_server $!

exit 104
