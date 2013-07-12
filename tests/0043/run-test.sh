#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

. clean.sh

# Directories to store database / features
DBDIR=dbdir
FDIR=fdir
mkdir ${DBDIR}
mkdir ${FDIR}

# Make LARGE_ADB
${AUDIODB} -d testdb -N --ntracks 50000 --adb_root ${DBDIR}
${AUDIODB} -d testdb -P --adb_root ${DBDIR}
${AUDIODB} -d testdb -L --adb_root ${DBDIR}

${AUDIODB} -d testdb -S --adb_root ${DBDIR} | grep "flags:" > testoutput
echo "flags: l2norm[on] minmax[off] power[on] times[off] largeADB[on]" > test-expected-output
cmp testoutput test-expected-output

intstring 2 > ${FDIR}/testfeature
floatstring 0 1 >> ${FDIR}/testfeature
floatstring 1 0 >> ${FDIR}/testfeature
floatstring 1 0 >> ${FDIR}/testfeature
floatstring 0 1 >> ${FDIR}/testfeature

intstring 1 > ${FDIR}/testpower
floatstring -0.5 >> ${FDIR}/testpower
floatstring -1 >> ${FDIR}/testpower
floatstring -1 >> ${FDIR}/testpower
floatstring -0.5 >> ${FDIR}/testpower

echo testfeature > ${FDIR}/testList.txt
echo testpower > ${FDIR}/pwrList.txt
echo key1 > ${FDIR}/keyList.txt

echo testfeature >> ${FDIR}/testList.txt
echo testpower >> ${FDIR}/pwrList.txt
echo key2 >> ${FDIR}/keyList.txt

pushd ${FDIR}
../${AUDIODB} -d testdb -B -F testList.txt -W pwrList.txt -K keyList.txt --adb_root ../${DBDIR}
rm testList.txt pwrList.txt keyList.txt
popd
# Cleanup


echo "query points (0.0,0.5),(0.0,0.5),(0.5,0.0)"
intstring 2 > testquery
floatstring 0 0.5 >> testquery
floatstring 0 0.5 >> testquery
floatstring 0.5 0 >> testquery

cp ${FDIR}/testpower .

# LARGE_ADB requires an INDEX
${AUDIODB} -d testdb -X -R 1 -l 1 --adb_root ${DBDIR} --adb_feature_root ${FDIR}

# LARGE_ADB query from key
${AUDIODB} -d testdb -Q sequence -l 1 -k key1 -R 1 --absolute-threshold -4.5 --adb_root ${DBDIR} --adb_feature_root ${FDIR} > testoutput
echo key2 1 > test-expected-output
cmp testoutput test-expected-output

# LARGE_ADB query from feature file and power file
${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testpower -R 1 --absolute-threshold -4.5 \
    --adb_root ${DBDIR} --adb_feature_root ${FDIR} > testoutput
echo key1 1 > test-expected-output
echo key2 1 >> test-expected-output
cmp testoutput test-expected-output

# WS
WSPORT=10020
${AUDIODB} -s ${WSPORT} --adb_root ${DBDIR} --adb_feature_root ${FDIR} &
# HACK: deal with race on process creation
sleep 1
trap 'kill $!; exit 1' ERR

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

# TEST LARGE_ADB WS WITH PRELOADED INDEX
WSPORT=10020
${AUDIODB} -s ${WSPORT} --load_index -d testdb -R 1 -l 1 --adb_root ${DBDIR} --adb_feature_root ${FDIR} &
# HACK: deal with race on process creation
sleep 1
trap 'kill $!; exit 1' ERR

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

# Clean up
. clean.sh

exit 104
