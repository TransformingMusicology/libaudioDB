#! /bin/sh

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

echo "query point (0.0,0.5)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -p 0 > testoutput
echo testfeature01 0 0 0 > test-expected-output
echo testfeature10 2 0 0 >> test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 -p 0 > testoutput
echo testfeature01 0 0 0 > test-expected-output
cmp testoutput test-expected-output

echo "query point (0.5,0.0)"

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -p 1 > testoutput
echo testfeature10 0 0 0 > test-expected-output
echo testfeature01 2 0 0 >> test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 -p 1 > testoutput
echo testfeature10 0 0 0 > test-expected-output
cmp testoutput test-expected-output

exit 104
