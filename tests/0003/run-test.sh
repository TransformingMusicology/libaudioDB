#! /bin/sh

trap "exit 1" ERR

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

# FIXME: endianness!
printf "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0\x3f" > testfeature

${AUDIODB} -d testdb -I -f testfeature

${AUDIODB} -d testdb -Q point -f testfeature > query-output

echo testfeature 1 0 0 > test-query-output

cmp query-output test-query-output

# failure cases
${AUDIODB} -d testdb -I && exit 1
${AUDIODB} -d testdb -f testfeature && exit 1
${AUDIODB} -I -f testfeature && exit 1
${AUDIODB} -d testdb -Q notpoint -f testfeature && exit 1
${AUDIODB} -Q point -f testfeature && exit 1

exit 104
