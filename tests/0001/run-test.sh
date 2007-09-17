# /bin/sh

trap "exit 1" ERR

if [ -f testdb ]; then rm -f testdb; fi

# creation
${AUDIODB} -N -d testdb

stat testdb

# should fail
${AUDIODB} -N -d testdb && exit 1

exit 104
