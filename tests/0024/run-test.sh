#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 2 > testfeature01
floatstring 0 1 >> testfeature01
intstring 2 > testfeature10
floatstring 1 0 >> testfeature10

cat > testfeaturefiles <<EOF
testfeature01
testfeature10
EOF

${AUDIODB} -d testdb -B -F testfeaturefiles

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

echo "exhaustive search"
intstring 2 > testquery
floatstring 0 0.5 >> testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -e > testoutput
echo testfeature01 1 0 0 > test-expected-output
echo testfeature10 1 1 0 >> test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -n 1 -e > testoutput
echo testfeature01 0 0 0 > test-expected-output
echo testfeature10 0 1 0 >> test-expected-output
cmp testoutput test-expected-output

exit 104
