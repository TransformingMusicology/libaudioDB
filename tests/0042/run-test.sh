#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

# Make LARGE_ADB
${AUDIODB} -d testdb -N --ntracks 50000
${AUDIODB} -d testdb -P
${AUDIODB} -d testdb -L

${AUDIODB} -d testdb -S | grep "flags:" > testoutput
echo "flags: l2norm[on] minmax[off] power[on] times[off] largeADB[on]" > test-expected-output
cmp testoutput test-expected-output

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

echo testfeature > testList.txt
echo testpower > pwrList.txt
echo key1 > keyList.txt

echo testfeature >> testList.txt
echo testpower >> pwrList.txt
echo key2 >> keyList.txt

${AUDIODB} -d testdb -B -F testList.txt -W pwrList.txt -K keyList.txt
# Cleanup
rm testList.txt pwrList.txt keyList.txt

echo "query points (0.0,0.5),(0.0,0.5),(0.5,0.0)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery
floatstring 0 0.5 >> testquery
floatstring 0.5 0 >> testquery

# LARGE_ADB requires an INDEX
${AUDIODB} -d testdb -X -R 1 -l 1

# LARGE_ADB query from key
${AUDIODB} -d testdb -Q sequence -l 1 -k key1 -R 1 --absolute-threshold -4.5 > testoutput
echo key2 1 > test-expected-output
cmp testoutput test-expected-output

# LARGE_ADB query from feature file and power file
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testpower -R 1 --absolute-threshold -4.5 > testoutput
echo key1 1 > test-expected-output
echo key2 1 >> test-expected-output
cmp testoutput test-expected-output

# WS
WSPORT=10020
start_server ${AUDIODB} ${WSPORT}

# LARGE_ADB WS query from key
${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 1 -k key1 -R 1 --absolute-threshold -4.5 -n 1 --lsh_exact > testoutput
echo key2 0 0 0 > test-expected-output
cmp testoutput test-expected-output

# LARGE_ADB WS query from feature file and power file tests
${AUDIODB} -c localhost:${WSPORT} -d testdb -Q sequence -l 1 -f testquery -w testpower -R 1 --absolute-threshold -4.5 -n 1 > testoutput
echo key1 1 > test-expected-output
echo key2 1 >> test-expected-output
cmp testoutput test-expected-output

stop_server $!

exit 104
