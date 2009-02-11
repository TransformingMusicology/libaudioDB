#include "audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char *argv[]) {
  adb_t *adb;
  adb_liszt_results_t *liszt;

  clean_remove_db(TESTDB);
  if(!(adb = audiodb_create(TESTDB, 0, 0, 0)))
    return 1;
  adb_datum_t datum1 = {2, 2, "testfeature01", (double[4]) {0, 1, 1, 0}};
  adb_datum_t datum2 = {2, 2, "testfeature10", (double[4]) {1, 0, 0, 1}};
  if(audiodb_insert_datum(adb, &datum1))
    return 1;
  if(audiodb_insert_datum(adb, &datum2))
    return 1;

  liszt = audiodb_liszt(adb);
  if(!liszt || liszt->nresults != 2)
    return 1;
  entry_present_or_fail(liszt, "testfeature01", 2);
  entry_present_or_fail(liszt, "testfeature10", 2);
  audiodb_liszt_free_results(adb, liszt);

  audiodb_close(adb);

  return 104;
}
