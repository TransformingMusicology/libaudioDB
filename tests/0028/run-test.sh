#! /bin/sh

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 2 > testfeature
floatstring 0 1 >> testfeature
floatstring 1 0 >> testfeature

intstring 1 > testpower
floatstring -0.5 >> testpower
floatstring -1 >> testpower

echo testfeature > testFeatureList.txt
echo testpower > testPowerList.txt

expect_clean_error_exit ${AUDIODB} -d testdb -B -F testFeatureList.txt -W testPowerList.txt

${AUDIODB} -d testdb -P

expect_clean_error_exit ${AUDIODB} -d testdb -B -F testFeatureList.txt

${AUDIODB} -d testdb -B -F testFeatureList.txt -W testPowerList.txt

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

# queries without power files should run as before
echo "query point (0.0,0.5)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
echo testfeature 1 0 0 > test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -n 1 > testoutput
echo testfeature 0 0 0 > test-expected-output
cmp testoutput test-expected-output

echo "query point (0.5,0.0)"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
echo testfeature 1 0 1 > test-expected-output
cmp testoutput test-expected-output
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -n 1 > testoutput
echo testfeature 0 0 1 > test-expected-output
cmp testoutput test-expected-output

# queries with power files might do something different
echo "query point (0.0,0.5), p=-0.5"
intstring 2 > testquery
floatstring 0 0.5 >> testquery

intstring 1 > testquerypower
floatstring -0.5 >> testquerypower

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-1.4 > testoutput
echo testfeature 1 0 0 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-0.6 > testoutput
echo testfeature 0 0 0 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-0.2 > testoutput
cat /dev/null > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --relative-threshold=1 > testoutput
echo testfeature 1 0 0 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --relative-threshold=0.2 > testoutput
echo testfeature 0 0 0 > test-expected-output
cmp testoutput test-expected-output

echo "query point (0.5,0.0), p=-0.5"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-1.4 > testoutput
echo testfeature 1 0 1 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-0.6 > testoutput
echo testfeature 2 0 0 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-0.2 > testoutput
cat /dev/null > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --relative-threshold=1 > testoutput
echo testfeature 1 0 1 > test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --relative-threshold=0.2 > testoutput
echo testfeature 2 0 0 > test-expected-output
cmp testoutput test-expected-output

exit 104
