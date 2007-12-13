#! /bin/sh

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

${AUDIODB} -d testdb -L

intstring 2 > testfeature01
floatstring 0 1 >> testfeature01
intstring 2 > testfeature10
floatstring 1 0 >> testfeature10

${AUDIODB} -d testdb -I -f testfeature01
${AUDIODB} -d testdb -I -f testfeature10

echo "query point (0.0,0.5)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery

${AUDIODB} -d testdb -Q track -l 1 -f testquery > testoutput
echo testfeature01 0.5 0 0 > test-expected-output
echo testfeature10 0 0 0 >> test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q track -l 1 -f testquery -r 1 > testoutput
echo testfeature01 0.5 0 0 > test-expected-output
cmp testoutput test-expected-output

echo "query point (0.5,0.0)"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -d testdb -Q track -l 1 -f testquery > testoutput
echo testfeature10 0.5 0 0 > test-expected-output
echo testfeature01 0 0 0 >> test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q track -l 1 -f testquery -r 1 > testoutput
echo testfeature10 0.5 0 0 > test-expected-output
cmp testoutput test-expected-output

exit 104
