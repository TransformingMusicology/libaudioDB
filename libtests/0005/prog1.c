#include "audioDB/audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char **argv) {
  adb_t *adb;

  clean_remove_db(TESTDB);
  if(!(adb = audiodb_create(TESTDB, 0, 0, 0)))
    return 1;

  adb_datum_t datum = {2, 2, "testfeature", (double[4]) {0, 1, 1, 0}};
  if(audiodb_insert_datum(adb,&datum))
    return 1;

  if(audiodb_l2norm(adb))
    return 1;

  audiodb_close(adb);

  return 104;
}

