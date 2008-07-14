#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 1 > testpower
floatstring -1 >> testpower

${AUDIODB} -d testdb -P


for i in rad[0-9][0-9]/*
do
${AUDIODB} -d testdb -I -f $i -w testpower
done

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

rm -f testdb.lsh.*

${AUDIODB} -d testdb -X -R 1 -l 1 --lsh_N 10000 --lsh_b 10000 --lsh_k 10 --lsh_m 5 --absolute-threshold -10

${AUDIODB} -d testdb -Q sequence -R 1 -l 1 -f testfeature -w testpower --absolute-threshold -10 -e

