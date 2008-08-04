#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 2 > testfeature01
floatstring 0 1 >> testfeature01
floatstring 1 0 >> testfeature01
intstring 2 > testfeature10
floatstring 1 0 >> testfeature10
floatstring 0 1 >> testfeature10

cat > testfeaturefiles <<EOF
testfeature01
testfeature01
testfeature10
testfeature10
testfeature01
testfeature10
EOF

${AUDIODB} -d testdb -B -F testfeaturefiles

${AUDIODB} -d testdb -S | grep "num files:2"

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

echo "query point (0.0,0.5)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery

${AUDIODB} -d testdb -Q nsequence -l 1 -f testquery > testoutput
echo testfeature01 1 > test-expected-output
echo 0 0 0 >> test-expected-output
echo 2 0 1 >> test-expected-output
echo testfeature10 1 >> test-expected-output
echo 0 0 1 >> test-expected-output
echo 2 0 0 >> test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q nsequence -l 1 -f testquery -n 2 > testoutput
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q nsequence -l 1 -f testquery -n 5 > testoutput
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q nsequence -l 1 -f testquery -n 1 > testoutput
echo testfeature01 0 > test-expected-output
echo 0 0 0 >> test-expected-output
echo testfeature10 0 >> test-expected-output
echo 0 0 1 >> test-expected-output
cmp testoutput test-expected-output

echo "query point (0.5,0.0)"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -d testdb -Q nsequence -l 1 -f testquery > testoutput
echo testfeature01 1 > test-expected-output
echo 0 0 1 >> test-expected-output
echo 2 0 0 >> test-expected-output
echo testfeature10 1 >> test-expected-output
echo 0 0 0 >> test-expected-output
echo 2 0 1 >> test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q nsequence -l 1 -f testquery -n 2 > testoutput
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q nsequence -l 1 -f testquery -n 5 > testoutput
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q nsequence -l 1 -f testquery -n 1 > testoutput
echo testfeature01 0 > test-expected-output
echo 0 0 1 >> test-expected-output
echo testfeature10 0 >> test-expected-output
echo 0 0 0 >> test-expected-output
cmp testoutput test-expected-output

exit 104
