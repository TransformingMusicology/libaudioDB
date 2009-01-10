#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 2 > testfeature01
floatstring 0 1 >> testfeature01
intstring 2 > testfeature10
floatstring 1 0 >> testfeature10

${AUDIODB} -d testdb -I -f testfeature01
${AUDIODB} -d testdb -I -f testfeature10

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

start_server ${AUDIODB} 10020

echo "query point (0.0,0.5)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery

${AUDIODB} -c localhost:10020 -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
echo testfeature01 1 > test-expected-output1
echo testfeature10 1 >> test-expected-output1
echo testfeature10 1 > test-expected-output2
echo testfeature01 1 >> test-expected-output2
cmp testoutput test-expected-output1 || cmp testoutput test-expected-output2
${AUDIODB} -c localhost:10020 -d testdb -Q sequence -l 1 -f testquery -r 1 -R 5 > testoutput
echo testfeature01 1 > test-expected-output1
echo testfeature10 1 > test-expected-output2
cmp testoutput test-expected-output1 || cmp testoutput test-expected-output2

check_server $!

echo "query point (0.5,0.0)"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -c localhost:10020 -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
echo testfeature01 1 > test-expected-output1
echo testfeature10 1 >> test-expected-output1
echo testfeature10 1 > test-expected-output2
echo testfeature01 1 >> test-expected-output2
cmp testoutput test-expected-output1 || cmp testoutput test-expected-output2
${AUDIODB} -c localhost:10020 -d testdb -Q sequence -l 1 -f testquery -r 1 -R 5 > testoutput
echo testfeature01 1 > test-expected-output1
echo testfeature10 1 > test-expected-output2
cmp testoutput test-expected-output1 || cmp testoutput test-expected-output2

stop_server $!

exit 104
