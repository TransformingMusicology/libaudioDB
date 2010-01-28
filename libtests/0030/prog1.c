#include "audioDB_API.h"
#include "test_utils_lib.h"

int main(int argc, char **argv) {
  adb_t *adb;

  clean_remove_db(TESTDB);
  if(!(adb = audiodb_create(TESTDB, 0, 0, 0)))
    return 1;

  adb_datum_t feature = {4, 2, "testfeature",
                         (double [8]) {0, 1, 1, 0, 1, 0, 0, 1},
                         (double [4]) {-0.5, -1, -1, -0.5}};
  audiodb_power(adb);
  if(audiodb_insert_datum(adb, &feature))
    return 1;
  if(audiodb_l2norm(adb))
    return 1;

  adb_datum_t query = {3, 2, "testquery",
                       (double [6]) {0, 0.5, 0, 0.5, 0.5, 0},
                       (double [3]) {-0.5, -1, -1}};
  adb_query_id_t qid = {0};
  qid.datum = &query;
  qid.sequence_length = 1;
  qid.sequence_start = 0;
  adb_query_parameters_t parms =
    {ADB_ACCUMULATION_PER_TRACK, ADB_DISTANCE_EUCLIDEAN_NORMED, 10, 10};
  adb_query_refine_t refine = {0};
  refine.flags = ADB_REFINE_RADIUS;
  refine.radius = 0.1;

  adb_query_spec_t spec;
  spec.qid = qid;
  spec.params = parms;
  spec.refine = refine;

  adb_query_results_t *results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature", 0, 0, 0);
  result_present_or_fail(results, "testfeature", 0, 0, 3);
  audiodb_query_free_results(adb, &spec, results);

  spec.qid.sequence_start = 1;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature", 0, 1, 0);
  result_present_or_fail(results, "testfeature", 0, 1, 3);
  audiodb_query_free_results(adb, &spec, results);

  spec.qid.sequence_start = 2;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature", 0, 2, 1);
  result_present_or_fail(results, "testfeature", 0, 2, 2);
  audiodb_query_free_results(adb, &spec, results);

  spec.qid.sequence_start = 0;
  spec.qid.sequence_length = 2;
  spec.refine.radius = 1.1;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature", 1, 0, 0);
  result_present_or_fail(results, "testfeature", 1, 0, 2);
  audiodb_query_free_results(adb, &spec, results);

  spec.refine.radius = 0.9;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 0) return 1;
  audiodb_query_free_results(adb, &spec, results);

  spec.qid.sequence_start = 1;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 1) return 1;
  result_present_or_fail(results, "testfeature", 0, 1, 0);
  audiodb_query_free_results(adb, &spec, results);

  /* tests with power from here on down */

  spec.qid.sequence_length = 2;
  spec.qid.sequence_start = 0;
  spec.refine.flags = ADB_REFINE_RADIUS|ADB_REFINE_ABSOLUTE_THRESHOLD;
  spec.refine.radius = 1.1;
  spec.refine.absolute_threshold = -1.4;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature", 1, 0, 0);
  result_present_or_fail(results, "testfeature", 1, 0, 2);
  audiodb_query_free_results(adb, &spec, results);

  spec.refine.absolute_threshold = -0.8;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature", 1, 0, 0);
  result_present_or_fail(results, "testfeature", 1, 0, 2);
  audiodb_query_free_results(adb, &spec, results);

  spec.refine.absolute_threshold = -0.7;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 0) return 1;
  audiodb_query_free_results(adb, &spec, results);

  spec.qid.sequence_start = 1;
  spec.refine.absolute_threshold = -1.4;
  spec.refine.radius = 0.9;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 1) return 1;
  result_present_or_fail(results, "testfeature", 0, 1, 0);
  audiodb_query_free_results(adb, &spec, results);

  spec.refine.absolute_threshold = -0.9;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 0) return 1;
  audiodb_query_free_results(adb, &spec, results);

  spec.qid.sequence_length = 2;
  spec.qid.sequence_start = 0;
  spec.refine.absolute_threshold = 0;
  spec.refine.flags = ADB_REFINE_RADIUS|ADB_REFINE_RELATIVE_THRESHOLD;
  spec.refine.radius = 1.1;
  spec.refine.relative_threshold = 0.1;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 2) return 1;
  result_present_or_fail(results, "testfeature", 1, 0, 0);
  result_present_or_fail(results, "testfeature", 1, 0, 2);
  audiodb_query_free_results(adb, &spec, results);

  spec.refine.radius = 0.9;
  results = audiodb_query_spec(adb, &spec);
  if(!results || results->nresults != 0) return 1;
  audiodb_query_free_results(adb, &spec, results);

  audiodb_close(adb);

  return 104;
}
