#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 2 > testfeature01
floatstring 0 1 >> testfeature01
floatstring 1 0 >> testfeature01
intstring 2 > testfeature10
floatstring 1 0 >> testfeature10
floatstring 0 1 >> testfeature10

cat > testfeaturefiles <<EOF
testfeature01
testfeature10
EOF

cat > testfeaturekeys <<EOF
testkey01
testkey02
EOF

${AUDIODB} -d testdb -B -F testfeaturefiles
${AUDIODB} -d testdb -S | grep "num files:2"

expect_clean_error_exit ${AUDIODB} -d testdb --LISZT --lisztOffset -1
expect_clean_error_exit ${AUDIODB} -d testdb --LISZT --lisztOffset 3
expect_clean_error_exit ${AUDIODB} -d testdb --LISZT --lisztLength -1

${AUDIODB} -d testdb --LISZT > testoutput
echo "[0] testfeature01 (2)" > test-expected-output
echo "[1] testfeature10 (2)" >> test-expected-output
cmp testoutput test-expected-output

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N
${AUDIODB} -d testdb -B -F testfeaturefiles -K testfeaturekeys
${AUDIODB} -d testdb -S | grep "num files:2"

${AUDIODB} -d testdb --LISZT > testoutput
echo "[0] testkey01 (2)" > test-expected-output
echo "[1] testkey02 (2)" >> test-expected-output
cmp testoutput test-expected-output

WSPORT=10039
start_server ${AUDIODB} ${WSPORT}

expect_clean_error_exit ${AUDIODB} -d testdb -c localhost:${WSPORT} --LISZT --lisztOffset -1
#expect_clean_error_exit ${AUDIODB} -d testdb -c localhost:${WSPORT} --LISZT --lisztOffset 3 #NOT EXITING CLEANLY
expect_clean_error_exit ${AUDIODB} -d testdb -c localhost:${WSPORT} --LISZT --lisztLength -1

check_server $!

${AUDIODB} -c localhost:${WSPORT} -d testdb --LISZT > testoutput
cmp testoutput test-expected-output

stop_server $!

exit 104
