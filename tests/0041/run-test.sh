#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 2 > testfeature
floatstring 0 1 >> testfeature
floatstring 1 0 >> testfeature
floatstring 1 0 >> testfeature
floatstring 0 1 >> testfeature

intstring 1 > testpower
floatstring -0.5 >> testpower
floatstring -1 >> testpower
floatstring -1 >> testpower
floatstring -0.5 >> testpower

expect_clean_error_exit ${AUDIODB} -d testdb -I -f testfeature -w testpower
${AUDIODB} -d testdb -P
expect_clean_error_exit ${AUDIODB} -d testdb -I -f testfeature
${AUDIODB} -d testdb -I -f testfeature -w testpower

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

echo "query points (0.0,0.5),(0.0,0.5),(0.5,0.0)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery
floatstring 0 0.5 >> testquery
floatstring 0.5 0 >> testquery

# LSH Indexing tests

# Indexing requires a radius (-R)
expect_clean_error_exit ${AUDIODB} -d testdb -X -l 1

# Index with default LSH params
${AUDIODB} -d testdb -X -l 1 -R 1

WSPORT=10020
start_server ${AUDIODB} ${WSPORT}

# WS Query using the index

${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 1 -f testquery -w testpower -R 1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 1 -f testquery -w testpower -p 0 -R 1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 1 -f testquery -w testpower -p 1 -R 1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 1 -f testquery -w testpower -e -R 1 > testoutput
echo testfeature 3 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 1 -f testquery -w testpower -e -R 1 --lsh_exact > testoutput
echo testfeature 3 > test-expected-output
cmp testoutput test-expected-output

# make index, sequenceLength=2
${AUDIODB} -d testdb -X -l 2 -R 1

# query, sequenceLength=2
${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 2 -f testquery -w testpower -p 0 -R 1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 2 -f testquery -w testpower -p 1 -R 1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 2 -f testquery -w testpower -p 0 -R 1 --lsh_exact > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output

stop_server $!

exit 104
