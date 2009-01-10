#include "audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char **argv) {
  adb_t *adb;
  struct stat st;

  clean_remove_db(TESTDB);

  adb = audiodb_open(TESTDB, O_RDWR);
  if(adb)
    return 1;

  adb = audiodb_create(TESTDB, 0, 0, 0);
  if (!adb)
    return 1;

  if(stat(TESTDB, &st))
    return 1;

  audiodb_close(adb);

  adb = audiodb_create(TESTDB, 0, 0, 0);
  if(adb)
    return 1;

  adb = audiodb_open(TESTDB, O_RDONLY);
  if (!adb)
    return 1;

  audiodb_close(adb);

  return 104;
}
