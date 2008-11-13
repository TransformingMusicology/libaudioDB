#! /bin/sh

for file in [0-9][0-9][0-9][0-9]*; do
  if [ -d ${file} ]; then
    echo Cleaning ${file}
    rm -f ${file}/test*
    (cd ${file} && make -f ../libtest.mk clean >/dev/null 2>&1)
    if [ -f ${file}/clean.sh ]; then
      (cd ${file} && sh ./clean.sh)
    fi
  fi
done
