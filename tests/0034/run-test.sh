#! /bin/sh

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 2 > testfeature
floatstring 1 1 >> testfeature
intstring 2 > testfeature01
floatstring 0 1 >> testfeature01
intstring 2 > testfeature10
floatstring 1 0 >> testfeature10

${AUDIODB} -d testdb -I -f testfeature
${AUDIODB} -d testdb -S | grep "num files:1"
${AUDIODB} -d testdb -I -f testfeature
${AUDIODB} -d testdb -S | grep "num files:1"
${AUDIODB} -d testdb -I -f testfeature01
${AUDIODB} -d testdb -S | grep "num files:2"
${AUDIODB} -d testdb -I -f testfeature10
${AUDIODB} -d testdb -S | grep "num files:3"

rm -f testdb

${AUDIODB} -d testdb -N

${AUDIODB} -d testdb -I -f testfeature01
${AUDIODB} -d testdb -S | grep "num files:1"
${AUDIODB} -d testdb -I -f testfeature01
${AUDIODB} -d testdb -S | grep "num files:1"
${AUDIODB} -d testdb -I -f testfeature10
${AUDIODB} -d testdb -S | grep "num files:2"
${AUDIODB} -d testdb -I -f testfeature
${AUDIODB} -d testdb -S | grep "num files:3"

rm -f testdb

${AUDIODB} -d testdb -N

echo testfeature > testfeaturelist.txt
echo testfeature01 >> testfeaturelist.txt
echo testfeature10 >> testfeaturelist.txt

${AUDIODB} -B -F testfeaturelist.txt -d testdb

${AUDIODB} -d testdb -S | grep "num files:3"

rm -f testdb

${AUDIODB} -d testdb -N

echo testfeature01 > testfeaturelist.txt
echo testfeature10 >> testfeaturelist.txt
echo testfeature >> testfeaturelist.txt

${AUDIODB} -B -F testfeaturelist.txt -d testdb

${AUDIODB} -d testdb -S | grep "num files:3"

exit 104
