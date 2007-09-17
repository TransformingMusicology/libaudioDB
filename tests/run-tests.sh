#! /bin/sh

for file in [0-9][0-9][0-9][0-9]*; do
  if [ -d ${file} ]; then
    if [ -f ${file}/run-test.sh ]; then
      cd ${file} && sh ./run-test.sh > test.out 2> test.err
      EXIT_STATUS=$?
      if [ ${EXIT_STATUS} -ne 104 ]; then
        echo Test ${file} failed: exit status ${EXIT_STATUS}
      fi
    else
      echo Skipping test ${file}
    fi
  fi
done
