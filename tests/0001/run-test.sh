#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

# creation
${AUDIODB} -N -d testdb

stat testdb

# should fail (testdb exists)
expect_clean_error_exit ${AUDIODB} -N -d testdb

# should fail (no db given)
expect_clean_error_exit ${AUDIODB} -N

exit 104
