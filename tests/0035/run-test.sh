#! /bin/sh

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 2 > testfeature1
floatstring 0 1 >> testfeature1
intstring 2 > testfeature3
floatstring 1 0 >> testfeature3
floatstring 0 1 >> testfeature3
floatstring 1 0 >> testfeature3

${AUDIODB} -d testdb -I -f testfeature1
${AUDIODB} -d testdb -I -f testfeature3

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

echo "query point (0 1, 1 0)"
intstring 2 > testquery
floatstring 0 1 >> testquery
floatstring 1 0 >> testquery

${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -n 1 > testoutput
wc -l testoutput | grep "1 testoutput"
grep "^testfeature3 .* 0 1$" testoutput

exit 104
