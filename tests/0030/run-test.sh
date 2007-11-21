#! /bin/sh

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

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 0.1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -p 0 -R 0.1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -p 1 -R 0.1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -p 0 -R 1.1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -p 0 -R 0.9 > testoutput
cat /dev/null > test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -p 1 -R 0.9 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output

echo "query points (0.0,0.5)p=-0.5,(0.0,0.5)p=-1,(0.5,0.0)p=-1"

intstring 1 > testquerypower
floatstring -0.5 -1 -1 >> testquerypower

${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-1.4 -p 0 -R 1.1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-0.8 -p 0 -R 1.1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-0.7 -p 0 -R 1.1 > testoutput
cat /dev/null > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-1.4 -p 1 -R 0.9 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-0.9 -p 1 -R 0.9 > testoutput
cat /dev/null > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --relative-threshold=0.1 -p 0 -R 1.1 > testoutput
echo testfeature 1 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --relative-threshold=0.1 -p 0 -R 0.9 > testoutput
cat /dev/null > test-expected-output
cmp testoutput test-expected-output

exit 104
