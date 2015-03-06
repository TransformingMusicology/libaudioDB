#include "audioDB/audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char **argv) {
  adb_t *adb;
  
  clean_remove_db(TESTDB);
  if(!(adb = audiodb_create(TESTDB, 0, 0, 0)))
    return 1;

  if(audiodb_power(adb))
    return 1;
  if(audiodb_power(adb))
    return 1;

  audiodb_close(adb);

  return 104;
}
