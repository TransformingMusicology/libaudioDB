#! /bin/sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

# FIXME: endianness!
printf "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f" > testfeature

${AUDIODB} -d testdb -I -f testfeature

${AUDIODB} -d testdb -Q point -f /tmp/foo > query-output

echo testfeature 1 0 0 > test-query-output

cmp query-output test-query-output

exit 104
