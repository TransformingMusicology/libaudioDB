#include "audioDB/audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char *argv[]) {
  adb_t *adb;
  adb_insert_t *batch = 0;
  adb_status_t status;

  clean_remove_db(TESTDB);
  adb = audiodb_create(TESTDB, 0, 0, 0);
  if(!adb) {
    return 1;
  }

  maketestfile("testfeature01", 2, (double[4]) {0,1,1,0}, 4);
  maketestfile("testfeature10", 2, (double[4]) {1,0,0,1}, 4);

  batch = (adb_insert_t *) calloc(6, sizeof(adb_insert_t));
  if(!batch) {
    return 1;
  }
  batch[0].features = "testfeature01";
  batch[1].features = "testfeature01";
  batch[2].features = "testfeature10";
  batch[3].features = "testfeature10";
  batch[4].features = "testfeature01";
  batch[5].features = "testfeature10";
  
  audiodb_batchinsert(adb, batch, 6);
  free(batch);

  if(audiodb_status(adb, &status) || status.numFiles != 2)
    return 1;

  if(audiodb_l2norm(adb))
    return 1;

  adb_datum_t datum = {1, 2, NULL, (double [2]){0, 0.5}, NULL, NULL};
  adb_query_id_t qid = {0};
  qid.datum = &datum;
  qid.sequence_length = 1;
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
  result_present_or_fail(results, "testfeature01", 2, 0, 1);
  result_present_or_fail(results, "testfeature10", 0, 0, 1);
  result_present_or_fail(results, "testfeature10", 2, 0, 0);
  audiodb_query_free_results(adb, &spec, results);

  spec.params.npoints = 2;
  results = audiodb_query_spec(adb, &spec);

  if(!results || results->nresults != 4) return 1;
  result_present_or_fail(results, "testfeature01", 0, 0, 0);
  result_present_or_fail(results, "testfeature01", 2, 0, 1);
  result_present_or_fail(results, "testfeature10", 0, 0, 1);
  result_present_or_fail(results, "testfeature10", 2, 0, 0);
  audiodb_query_free_results(adb, &spec, results);

  spec.params.npoints = 5;
  results = audiodb_query_spec(adb, &spec);

  if(!results || results->nresults != 4) return 1;
  result_present_or_fail(results, "testfeature01", 0, 0, 0);
  result_present_or_fail(results, "testfeature01", 2, 0, 1);
  result_present_or_fail(results, "testfeature10", 0, 0, 1);
  result_present_or_fail(results, "testfeature10", 2, 0, 0);
  audiodb_query_free_results(adb, &spec, results);

  spec.params.npoints = 1;
  results = audiodb_query_spec(adb, &spec);

  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature01", 0, 0, 0);
  result_present_or_fail(results, "testfeature10", 0, 0, 1);

  audiodb_query_free_results(adb, &spec, results);

  spec.qid.datum->data = (double [2]) {0.5, 0};
  spec.params.npoints = 10;
  results = audiodb_query_spec(adb, &spec);

  if(!results || results->nresults != 4) return 1;
  result_present_or_fail(results, "testfeature01", 0, 0, 1);
  result_present_or_fail(results, "testfeature01", 2, 0, 0);
  result_present_or_fail(results, "testfeature10", 0, 0, 0);
  result_present_or_fail(results, "testfeature10", 2, 0, 1);
  audiodb_query_free_results(adb, &spec, results);

  spec.params.npoints = 2;
  results = audiodb_query_spec(adb, &spec);

  if(!results || results->nresults != 4) return 1;
  result_present_or_fail(results, "testfeature01", 0, 0, 1);
  result_present_or_fail(results, "testfeature01", 2, 0, 0);
  result_present_or_fail(results, "testfeature10", 0, 0, 0);
  result_present_or_fail(results, "testfeature10", 2, 0, 1);
  audiodb_query_free_results(adb, &spec, results);

  spec.params.npoints = 5;
  results = audiodb_query_spec(adb, &spec);

  if(!results || results->nresults != 4) return 1;
  result_present_or_fail(results, "testfeature01", 0, 0, 1);
  result_present_or_fail(results, "testfeature01", 2, 0, 0);
  result_present_or_fail(results, "testfeature10", 0, 0, 0);
  result_present_or_fail(results, "testfeature10", 2, 0, 1);
  audiodb_query_free_results(adb, &spec, results);

  spec.params.npoints = 1;
  results = audiodb_query_spec(adb, &spec);

  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature01", 0, 0, 1);
  result_present_or_fail(results, "testfeature10", 0, 0, 0);
  audiodb_query_free_results(adb, &spec, results);

  audiodb_close(adb);

  return 104;
}
