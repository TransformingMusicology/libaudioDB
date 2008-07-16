#! /bin/bash

. ../test-utils.sh

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

intstring 1 > testpower
floatstring -1 >> testpower

${AUDIODB} -d testdb -P

NPOINTS=100
NDIM=10

if [ -d rad.[0-9]* ]; then rm -r rad.[0-9]*; fi

for j in .01 .02 .03 .05 01 02 03 05 09 10
do
  R_SQ=`echo "scale=6; $j^2" | bc`
  mkdir -p "rad$j"
  ./genpoints2 ${NPOINTS} ${R_SQ} ${NDIM}
  mv testfeature* "rad$j"
done

for i in rad*[0-9]*/*
do
${AUDIODB} -d testdb -I -f $i -w testpower
done

# sequence queries require L2NORM
${AUDIODB} -d testdb -L

rm -f testdb.lsh.*

LSH_W=1
LSH_K=1
LSH_M=1
LSH_N=1000

INDEXING=true
if [ ${INDEXING} ]
    then
    for j in .01 .02 .03 .05 01 02 03 05 09 10
      do
      R_SQ=`echo "scale=6; $j^2" | bc`
      ${AUDIODB} -d testdb -X -R ${R_SQ} -l 1 --lsh_N ${LSH_N} \
	  --lsh_b ${LSH_N} --lsh_k ${LSH_K} --lsh_m ${LSH_M} --lsh_w ${LSH_W} \
	  --absolute-threshold -1 --no_unit_norming
    done
fi

#if [ -f cumulativeResult.txt ]; then rm -f cumulativeResult.txt;fi

for j in .01 .02 .03 .05 01 02 03 05 09 10
  do
  R_SQ=`echo "scale=6; $j^2" | bc`
  ${AUDIODB} \
      -d testdb -Q sequence -R ${R_SQ} -l 1 -e \
      -f queryfeature -w testpower --absolute-threshold -1 --no_unit_norming -r 1000 > output${j}
  echo `for k in .01 .02 .03 .05 01 02 03 05 09 10;do egrep "^rad$k" output${j} | wc | awk '{print $1}';done` >> cumulativeResult.txt
done



#Perform exact search as a sanity test
#rm -f *.lsh*
#echo
#for j in .01 .02 .03 .05 01 02 03 05 09 10
#  do
#  R_SQ=`echo "scale=6; $j^2" | bc`
#  ${AUDIODB} \
#      -d testdb -Q sequence -R ${R_SQ} -l 1 -e \
#      -f queryfeature -w testpower --absolute-threshold -1 --no_unit_norming -r 1000 > outputEUC
#  echo EUC points retrieved at Radius $j: \
#`for k in .01 .02 .03 .05 01 02 03 05 09 10; do egrep "^rad$k" outputEUC | wc | awk '{print $1}';done` 
#done

#Inspect the indexing parameters
#echo
#egrep "^INDEX:" output[1-9]
#echo

