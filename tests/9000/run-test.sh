#! /bin/sh

. ../test-utils.sh

for file in `find .. -name testdb -print | sort -n`
do
  dir=`mktemp -d`
  echo dumping "${file}" into "${dir}/${file:3:4}"
  ${AUDIODB} -d ${file} -D --output="${dir}/${file:3:4}"
  echo restoring "${file}" into "${dir}"/"${file:3:4}"/restoredb
  export restoreadb=${AUDIODB}
  (export AUDIODB=`pwd`/$restoreadb && cd "${dir}"/"${file:3:4}" && sh ./restore.sh restoredb)
  echo comparing dbs for "${file:3:4}"
  cmp "${file}" "${dir}"/"${file:3:4}"/restoredb
  rm -rf "${dir}"
done

exit 104
