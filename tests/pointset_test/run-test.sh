#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 1 > testpower
floatstring -1 >> testpower

${AUDIODB} -d testdb -P

NPOINTS=100
NDIM=10

if [ -d rad[0-9]* ]; then rm -r rad[0-9]*; fi

for j in 1 2 3 9
do
mkdir -p "rad$j"
./genpoints2 ${NPOINTS} $(( j*j )) ${NDIM}
mv testfeature* "rad$j"
done

for i in rad[0-9]/*
do
${AUDIODB} -d testdb -I -f $i -w testpower
done

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

rm -f testdb.lsh.*

LSH_K=10
LSH_M=5

INDEXING=true
if [ ${INDEXING} ]
    then
    for j in 1 2 3 9
      do
      ${AUDIODB} -d testdb -X -R $(( j*j )) -l 1 --lsh_N 1000 \
	  --lsh_b 1000 --lsh_k ${LSH_K} --lsh_m ${LSH_M} --absolute-threshold -1
    done
fi

for j in 1 2 3 9
do
${AUDIODB} \
    -d testdb -Q sequence -R $(( j*j )) \
    -l 1 -f queryfeature -w testpower --absolute-threshold -1 -e -r 400 > output
echo APPRX points retrieved at Radius $j: \
`egrep "^rad1" output | wc | awk '{print $1}'` \
`egrep "^rad2" output | wc | awk '{print $1}'` \
`egrep "^rad3" output | wc | awk '{print $1}'` \
`egrep "^rad9" output | wc | awk '{print $1}'` 
done

rm -f *.lsh*
echo
for j in 1 2 3 9
do
${AUDIODB} \
    -d testdb -Q sequence -R $(( j*j )) \
    -l 1 -f queryfeature -w testpower --absolute-threshold -1 -e -r 400 > output
echo EXACT points retrieved at Radius $j: \
`egrep "^rad1" output | wc | awk '{print $1}'` \
`egrep "^rad2" output | wc | awk '{print $1}'` \
`egrep "^rad3" output | wc | awk '{print $1}'` \
`egrep "^rad9" output | wc | awk '{print $1}'` 
done
