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
${AUDIODB} -d testdb -I -f testfeature -w testpower -k testfeature1

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

# Merged index
${AUDIODB} -d testdb -I -f testfeature -w testpower -k testfeature2

if [ -f testdb.lsh* ]; then
    rm testdb.lsh*
fi

${AUDIODB} -d testdb -X -l 1 -R 1 --lsh_b 1

# Add a new track
${AUDIODB} -d testdb -I -f testfeature -w testpower -k testfeature3

# index using same paramters as previous index (merge new data)
${AUDIODB} -d testdb -X -l 1 -R 1

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testpower -R 1 > testoutput
echo testfeature1 1 > test-expected-output
echo testfeature2 1 >> test-expected-output
echo testfeature3 1 >> test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testpower -p 0 -R 1 > testoutput
echo testfeature1 1 > test-expected-output
echo testfeature2 1 >> test-expected-output
echo testfeature3 1 >> test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testpower -p 1 -R 1 > testoutput
echo testfeature1 1 > test-expected-output
echo testfeature2 1 >> test-expected-output
echo testfeature3 1 >> test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testpower -e -R 1 > testoutput
echo testfeature1 3 > test-expected-output
echo testfeature2 3 >> test-expected-output
echo testfeature3 3 >> test-expected-output
cmp testoutput test-expected-output

${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testpower -e -R 1 --lsh_exact > testoutput
echo testfeature1 3 > test-expected-output
echo testfeature2 3 >> test-expected-output
echo testfeature3 3 >> test-expected-output
cmp testoutput test-expected-output


exit 104
