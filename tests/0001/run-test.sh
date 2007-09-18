#! /bin/sh

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

# creation
${AUDIODB} -N -d testdb

stat testdb

# should fail (testdb exists)
${AUDIODB} -N -d testdb && exit 1

# should fail (no db given)
${AUDIODB} -N && exit 1

exit 104
