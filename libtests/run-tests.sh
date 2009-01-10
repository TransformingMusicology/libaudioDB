#! /bin/bash

# FIXME: work out how to do proper getopt in bash
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
      if [ "$1" = "--valgrind" ]; then
        echo -n \ under valgrind
      fi
      echo -n :
      if [ "$1" = "--valgrind" ]; then
        (cd ${file} && make -f ../libtest.mk >/dev/null 2>&1 && valgrind --leak-check=full --show-reachable=yes --error-exitcode=1 --tool=memcheck ./test1 > test.out 2> test.err)
      else
        (cd ${file} && make -f ../libtest.mk >/dev/null 2>&1 && ./test1 > test.out 2> test.err)
      fi
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
