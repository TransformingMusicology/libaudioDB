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

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
echo testfeature01 1 > test-expected-output
echo testfeature10 1 >> test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K /dev/null -R 5 > testoutput
cat /dev/null > test-expected-output
cmp testoutput test-expected-output

echo testfeature01 > testkl.txt
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K testkl.txt -R 5 > testoutput
echo testfeature01 1 > test-expected-output
cmp testoutput test-expected-output
echo testfeature10 > testkl.txt
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K testkl.txt -R 5 > testoutput
echo testfeature10 1 > test-expected-output
cmp testoutput test-expected-output

echo testfeature10 > testkl.txt
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K testkl.txt -r 1 -R 5 > testoutput
echo testfeature10 1 > test-expected-output
cmp testoutput test-expected-output

# NB: one might be tempted to insert a test here for having both keys
# in the keylist, but in non-database order, and then checking that
# the result list is also in that non-database order.  I think that
# would be misguided, as the efficient way of dealing with such a
# keylist is to advance as-sequentially-as-possible through the
# database; it just so happens that our current implementation is not
# so smart.

echo "query point (0.5,0.0)"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
echo testfeature01 1 > test-expected-output
echo testfeature10 1 >> test-expected-output
cmp testoutput test-expected-output

echo testfeature10 > testkl.txt
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K testkl.txt -r 1 -R 5 > testoutput
echo testfeature10 1 > test-expected-output
cmp testoutput test-expected-output

exit 104
