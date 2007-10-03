#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

start_server ${AUDIODB} 10015

${AUDIODB} -d testdb -N

${AUDIODB} -c localhost:10015 -d testdb -S > test1
${AUDIODB} -S -c localhost:10015 -d testdb > test2
${AUDIODB} -S -d testdb -c localhost:10015 > test3

cat > testoutput <<EOF
numFiles = 0
dim = 0
length = 0
dudCount = 0
nullCount = 0
flags = 0
EOF

cmp test1 test2
cmp test2 test3
cmp test3 testoutput

check_server $!

expect_server_failure ${AUDIODB} -c localhost:10015 -S -d /dev/null
expect_server_failure ${AUDIODB} -c localhost:10015 -S -d /tmp/foo-does-not-exist

check_server $!

stop_server $!

exit 104
