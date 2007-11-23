# no shebang line: this file should be sourced by run-test.sh files

set -E

trap "exit 1" ERR

if [ -z "${AUDIODB}" ]; then
  AUDIODB=../../audioDB
fi

# FIXME: maybe generalize to multiple arguments?  Also, implement it
# properly, rather than just for a few floats that we know how to
# encode.  This might involve writing some C code, as Bash doesn't do
# Floating Point.  (scanf() is probably enough).

expect_clean_error_exit() {
  trap - ERR
  "$@"
  exit_code=$?
  trap "exit 1" ERR
  if [ $exit_code -eq 0 ]; then
    exit 1
  elif [ $exit_code -ge 126 ]; then
    exit 1
  fi
}

floatstring() {
  for arg in "$@"; do
    case ${arg} in
      0)
        printf "\x00\x00\x00\x00\x00\x00\x00\x00";;
      -0.5)
        printf "\x00\x00\x00\x00\x00\x00\xe0\xbf";;
      0.5)
        printf "\x00\x00\x00\x00\x00\x00\xe0\x3f";;
      -1)
        printf "\x00\x00\x00\x00\x00\x00\xf0\xbf";;
      1)
        printf "\x00\x00\x00\x00\x00\x00\xf0\x3f";;
      *)
        echo "bad arg to floatstring(): ${arg}"
        exit 1;;
    esac
  done
}

# FIXME: likewise.  And endianness issues (which are a reflection of
# the endianness of audioDB as of 2007-09-18, unfortunately).

intstring() {
  # works up to 9 for now
  if [ $1 -ge 10 ]; then echo "intstring() arg too large: ${1}"; exit 1; fi
  printf "%b\x00\x00\x00" "\\x${1}"
}

# Web services utilities
start_server() {
  $1 -s $2 &
  # HACK: deal with race on process creation
  sleep 1
  trap 'kill $!; exit 1' ERR
}

stop_server() {
  grep "${AUDIODB}" /proc/$1/cmdline > /dev/null
  kill $1
  # HACK: deal with race on process exit
  sleep 1
  expect_clean_error_exit grep ${AUDIODB} /proc/$1/cmdline
}

check_server() {
  grep "${AUDIODB}" /proc/$1/cmdline > /dev/null
}

expect_client_failure() {
  # FIXME: work out whether and how the client should report server
  # errors.  At present, the client exits with a zero exit code.
  "$@"
}
