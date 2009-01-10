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

echo "query point (0.0,0.5)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery

# because we have a tie, we treat both possible answers as correct.
# This is the only way to preserve my sanity right now.  -- CSR,
# 2008-12-15.

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
echo testfeature01 1 > test-expected-output1
echo testfeature10 1 >> test-expected-output1
echo testfeature10 1 > test-expected-output2
echo testfeature01 1 >> test-expected-output2
cmp testoutput test-expected-output1 || cmp testoutput test-expected-output2
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 -R 5 > testoutput
echo testfeature01 1 > test-expected-output1
echo testfeature10 1 > test-expected-output2
cmp testoutput test-expected-output1 || cmp testoutput test-expected-output2

echo "query point (0.5,0.0)"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
echo testfeature01 1 > test-expected-output1
echo testfeature10 1 >> test-expected-output1
echo testfeature10 1 > test-expected-output2
echo testfeature01 1 >> test-expected-output2
cmp testoutput test-expected-output1 || cmp testoutput test-expected-output2
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 -R 5 > testoutput
echo testfeature01 1 > test-expected-output1
echo testfeature10 1 > test-expected-output2
cmp testoutput test-expected-output1 || cmp testoutput test-expected-output2

exit 104
