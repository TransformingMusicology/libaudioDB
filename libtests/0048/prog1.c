#include "audioDB/audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char **argv) {
  adb_t *adb;

  clean_remove_db(TESTDB);
  if(!(adb = audiodb_create(TESTDB, 0, 0, 0)))
    return 1;

  adb_datum_t datum1 = {2, 2, "testfeature01", (double[4]) {0, 0.5, 0.5, 0}, 
			NULL, (double[4]) {0, 1, 1, 2}};
  adb_datum_t datum2 = {3, 2, "testfeature10", (double[6]) {0.5, 0, 0, 0.5, 0.5, 0},
			NULL, (double[6]) {0, 2, 2, 3, 3, 4}};
  if(audiodb_insert_datum(adb, &datum1))
    return 1;
  if(audiodb_insert_datum(adb, &datum2))
    return 1;
  if(audiodb_l2norm(adb))
    return 1;

  adb_datum_t retrieve;
  if(!(audiodb_retrieve_datum(adb, "testfeature", &retrieve)))
    return 1;

  if(audiodb_retrieve_datum(adb, "testfeature01", &retrieve))
    return 1;
  if(retrieve.nvectors != 2)
    return 1;
  if(retrieve.dim != 2)
    return 1;
  if(strcmp(retrieve.key, "testfeature01"))
    return 1;
  if(memcmp(retrieve.data, datum1.data, 4*sizeof(double)))
    return 1;
  if(retrieve.power)
    return 1;
  if(memcmp(retrieve.times, datum1.times, 4*sizeof(double)))
    return 1;
  if(audiodb_free_datum(adb, &retrieve))
    return 1;

  if(audiodb_retrieve_datum(adb, "testfeature10", &retrieve))
    return 1;
  if(retrieve.nvectors != 3)
    return 1;
  if(retrieve.dim != 2)
    return 1;
  if(strcmp(retrieve.key, "testfeature10"))
    return 1;
  if(memcmp(retrieve.data, datum2.data, 6*sizeof(double)))
    return 1;
  if(retrieve.power)
    return 1;
  if(memcmp(retrieve.times, datum2.times, 6*sizeof(double)))
    return 1;
  if(audiodb_free_datum(adb, &retrieve))
    return 1;

  audiodb_close(adb);

  return 104;
}
