#! /bin/bash

AUDIODB=../../${EXECUTABLE:-audioDB}
export AUDIODB

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
    if [ /bin/true ]; then
      echo -n Running test ${file}
      if [ -f ${file}/short-description ]; then
        awk '{ printf(" (%s)",$0) }' < ${file}/short-description
      fi
      echo -n :
      (cd ${file} && make -f ../libtest.mk >/dev/null 2>&1 && ./test1 > test.out 2> test.err && exit 104)
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
