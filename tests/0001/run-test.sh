# /bin/sh

trap "exit 1" ERR

if [ -f testdb ]; then rm -f testdb; fi

# creation
../../audioDB -N -d testdb

stat testdb

# should fail
../../audioDB -N -d testdb && exit 1

exit 104