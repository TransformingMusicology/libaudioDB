#! /bin/bash

. ../test-utils.sh

start_server ${AUDIODB} 10013

check_server $!

stop_server $!

exit 104
