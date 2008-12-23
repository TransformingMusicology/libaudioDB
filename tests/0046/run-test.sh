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

${AUDIODB} -d testdb -L
${AUDIODB} -d testdb -P

# insert two instances of the same feature
${AUDIODB} -d testdb -I -f testfeature -w testpower -k testfeatureA
${AUDIODB} -d testdb -I -f testfeature -w testpower -k testfeatureB


echo "query points (0.0,0.5),(0.0,0.5),(0.5,0.0)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery
floatstring 0 0.5 >> testquery
floatstring 0.5 0 >> testquery

# LSH Indexed query with restrict list test

# Index with default LSH params
${AUDIODB} -d testdb -X -l 1 -R 1

# Query using the index

echo testfeatureB > test-restrict-list
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testpower -K test-restrict-list -R 1 > testoutput
echo testfeatureB 1 > test-expected-output

cmp testoutput test-expected-output

exit 104
