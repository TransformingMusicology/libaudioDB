#include "audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char **argv) {
  adb_t *adb;
  adb_insert_t insert = {0};
  adb_status_t status = {0};
  adb_insert_t batch[4] = {{0},{0},{0},{0}};

  clean_remove_db(TESTDB);
  if(!(adb = audiodb_create(TESTDB, 0, 0, 0)))
    return 1;

  maketestfile("testfeature", 2, (double[2]) {1, 1}, 2);
  maketestfile("testfeature01", 2, (double[2]) {0, 1}, 2);
  maketestfile("testfeature10", 2, (double[2]) {1, 0}, 2);

  insert.features = "testfeature";
  if(audiodb_insert(adb, &insert))
    return 1;
  if(audiodb_status(adb, &status) || status.numFiles != 1)
    return 1;
  
  /* reinserts using audiodb_insert() should silently not fail and
   * silently not insert, to support legacy command-line behaviour. */
  if(audiodb_insert(adb, &insert))
    return 1;
  if(audiodb_status(adb, &status) || status.numFiles != 1)
    return 1;

  /* reinserts using audiodb_insert_datum() should fail. */
  adb_datum_t datum = {1, 2, "testfeature", (double[2]) {1, 1}};
  if(!audiodb_insert_datum(adb, &datum))
    return 1;

  insert.features = "testfeature01";
  if(audiodb_insert(adb, &insert))
    return 1;
  if(audiodb_status(adb, &status) || status.numFiles != 2)
    return 1;

  insert.features = "testfeature10";
  if(audiodb_insert(adb, &insert))
    return 1;
  if(audiodb_status(adb, &status) || status.numFiles != 3)
    return 1;
  
  audiodb_close(adb);

  clean_remove_db(TESTDB);
  if(!(adb = audiodb_create(TESTDB, 0, 0, 0)))
    return 1;

  batch[0].features = "testfeature";
  batch[1].features = "testfeature01";
  batch[2].features = "testfeature10";
  batch[3].features = "testfeature10";
  if(audiodb_batchinsert(adb, batch, 4))
    return 1;
  if(audiodb_status(adb, &status) || status.numFiles != 3)
    return 1;

  audiodb_close(adb);

  return 104;
}
