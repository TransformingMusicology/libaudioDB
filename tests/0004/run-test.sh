#! /bin/sh

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 2 > testfeature
floatstring 0 1 >> testfeature
floatstring 1 0 >> testfeature

${AUDIODB} -d testdb -I -f testfeature

echo "query point (0.0,0.5)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery

${AUDIODB} -d testdb -Q point -f testquery > testoutput
wc -l testoutput | grep 2
${AUDIODB} -d testdb -Q point -f testquery -n 1 > testoutput
wc -l testoutput | grep 1

echo "query point (0.5,0.0)"
intstring 2 > testquery
floatstring 0.5 0 >> testquery

${AUDIODB} -d testdb -Q point -f testquery > testoutput
wc -l testoutput | grep 2
${AUDIODB} -d testdb -Q point -f testquery -n 1 > testoutput
wc -l testoutput | grep 1

exit 104
