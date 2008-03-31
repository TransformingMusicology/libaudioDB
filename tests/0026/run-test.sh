#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -N -d testdb

${AUDIODB} -P -d testdb
${AUDIODB} -d testdb -P

# should fail (no db given)
expect_clean_error_exit ${AUDIODB} -P

exit 104
