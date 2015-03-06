#include "audioDB/audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char **argv) {
  adb_t *adb;
  adb_insert_t batch[2]={{0}, {0}};

  clean_remove_db(TESTDB);
  if(!(adb = audiodb_create(TESTDB, 0, 0, 0)))
    return 1;

  maketestfile("testfeature01", 2, (double[2]) {0, 1}, 2);
  maketestfile("testfeature10", 2, (double[2]) {1, 0}, 2);

  batch[0].features="testfeature01";
  batch[1].features="testfeature10";
  if(audiodb_batchinsert(adb, batch, 2))
    return 1;
  if(audiodb_l2norm(adb))
    return 1;

  adb_datum_t query = {2, 2, "testquery", (double[4]) {0, 0.5, 0.5, 0}};
  adb_query_id_t qid = {0};
  qid.datum = &query;
  qid.sequence_length = 1;
  qid.flags = ADB_QID_FLAG_EXHAUSTIVE;
  adb_query_parameters_t parms =
    {ADB_ACCUMULATION_PER_TRACK, ADB_DISTANCE_EUCLIDEAN_NORMED, 10, 10};
  adb_query_refine_t refine = {0};

  adb_query_spec_t spec;
  spec.qid = qid;
  spec.params = parms;
  spec.refine = refine;
  
  adb_query_results_t *results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 4) return 1;
  result_present_or_fail(results, "testfeature01", 0, 0, 0);
  result_present_or_fail(results, "testfeature01", 2, 1, 0);
  result_present_or_fail(results, "testfeature10", 2, 0, 0);
  result_present_or_fail(results, "testfeature10", 0, 1, 0);
  audiodb_query_free_results(adb, &spec, results);

  spec.params.npoints = 1;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature01", 0, 0, 0);
  result_present_or_fail(results, "testfeature10", 0, 1, 0);
  audiodb_query_free_results(adb, &spec, results);

  audiodb_close(adb);

  return 104;
}
