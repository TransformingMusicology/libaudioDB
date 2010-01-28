#include "audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char **argv) {
  adb_t *adb;

  clean_remove_db(TESTDB);
  if(!(adb = audiodb_create(TESTDB, 0, 0, 0)))
    return 1;

  adb_datum_t datum1 = {1, 2, "testfeature1", (double[2]) {0, 1}};
  adb_datum_t datum3 = {3, 2, "testfeature3", (double[6]) {1, 0, 0, 1, 1, 0}};

  if(audiodb_insert_datum(adb, &datum1))
    return 1;
  if(audiodb_insert_datum(adb, &datum3))
    return 1;
  if(audiodb_l2norm(adb))
    return 1;

  adb_datum_t query = {2, 2, "testquery", (double[4]) {0, 1, 1, 0}};
  adb_query_id_t qid = {0};
  qid.datum = &query;
  qid.sequence_length = 2;
  qid.sequence_start = 0;
  adb_query_parameters_t parms =
    {ADB_ACCUMULATION_PER_TRACK, ADB_DISTANCE_EUCLIDEAN, 1, 10};
  adb_query_refine_t refine = {0};

  adb_query_spec_t spec;
  spec.qid = qid;
  spec.params = parms;
  spec.refine = refine;

  adb_query_results_t *results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 1) return 1;
  result_present_or_fail(results, "testfeature3", 0, 0, 1);
  audiodb_query_free_results(adb, &spec, results);

  audiodb_close(adb);

  return 104;
}
