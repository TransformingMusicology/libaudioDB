#include "audioDB/audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char **argv) {
  adb_t *adb;
  adb_status_t status={0};

  clean_remove_db(TESTDB);

  adb = audiodb_create(TESTDB, 0, 0, 0);

  if(audiodb_status(adb, &status)){
    return 1;
  }
  if(status.numFiles != 0)
    return 1;

  audiodb_close(adb);

  return 104;
}

