#! /bin/bash

AUDIODB=../../${EXECUTABLE:-audioDB}
export AUDIODB

LD_LIBRARY_PATH=../
export LD_LIBRARY_PATH

if [ -x ${AUDIODB#../} ]; then 
  :
else 
  echo Cannot execute audioDB: ${AUDIODB#../}
  exit 1
fi

if [ "$1" = "--full" ]; then
  pattern="[0-9][0-9][0-9][0-9]*"
else
  pattern="[0-8][0-9][0-9][0-9]*"
fi

for file in ${pattern}; do
  if [ -d ${file} ]; then
    if [ -f ${file}/run-test.sh ]; then
      echo -n Running test ${file}
      if [ -f ${file}/short-description ]; then
        awk '{ printf(" (%s)",$0) }' < ${file}/short-description
      fi
      echo -n :
      (cd ${file} && /bin/bash ./run-test.sh > test.out 2> test.err)
      EXIT_STATUS=$?
      if [ ${EXIT_STATUS} -eq 14 ]; then
        echo " n/a."
      elif [ ${EXIT_STATUS} -ne 104 ]; then
        echo " failed (exit status ${EXIT_STATUS})."
        FAILED=true
      else
        echo " success."
      fi
    else
      echo Skipping test ${file}
    fi
  fi
done

if [ -z "${FAILED}" ]; then
  exit 0
else
  exit 1
fi
