#! /bin/sh

trap "exit 1" ERR

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -N -d testdb

# FIXME: at some point we will want to test that some relevant
# information is being printed
${AUDIODB} -S -d testdb
${AUDIODB} -d testdb -S

# should fail (no db given)
${AUDIODB} -S && exit 1

exit 104
