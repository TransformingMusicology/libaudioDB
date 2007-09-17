#! /bin/sh

floatstring() {
  case $1 in
    0)
      printf "\x00\x00\x00\x00\x00\x00\x00\x00";;
    0.5)
      printf "\x00\x00\x00\x00\x00\x00\xe0\x3f";;
    1)
      printf "\x00\x00\x00\x00\x00\x00\xf0\x3f";;
    *)
      echo "bad arg to floatstring(): $1"
      exit 1;;
  esac
}

trap "exit 1" ERR

if [ -f testdb ]; then rm -f testdb; fi

${AUDIODB} -d testdb -N

# FIXME: endianness!
printf "\x02\x00\x00\x00" > testfeature
floatstring 0 >> testfeature
floatstring 1 >> testfeature
floatstring 1 >> testfeature
floatstring 0 >> testfeature

${AUDIODB} -d testdb -I -f testfeature

echo "query point (0.0,0.5)"
printf "\x02\x00\x00\x00" > testquery
floatstring 0 >> testquery
floatstring 0.5 >> testquery

${AUDIODB} -d testdb -Q point -f testquery > testoutput
wc -l testoutput | grep 2
${AUDIODB} -d testdb -Q point -f testquery -n 1 > testoutput
wc -l testoutput | grep 1

echo "query point (0.5,0.0)"
printf "\x02\x00\x00\x00" > testquery
floatstring 0.5 >> testquery
floatstring 0 >> testquery

${AUDIODB} -d testdb -Q point -f testquery > testoutput
wc -l testoutput | grep 2
${AUDIODB} -d testdb -Q point -f testquery -n 1 > testoutput
wc -l testoutput | grep 1

exit 104
