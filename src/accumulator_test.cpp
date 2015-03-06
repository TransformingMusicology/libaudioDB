#include "audioDB/audioDB.h"
extern "C" {
#include "audioDB/audioDB_API.h"
}
#include "audioDB/audioDB-internals.h"

#include "audioDB/accumulators.h"

static NearestAccumulator<adb_result_dist_lt> *foo = new NearestAccumulator<adb_result_dist_lt>();

int main() {
  adb_result_t r;
  r.key = "hello";
  r.ipos = 0;
  r.qpos = 0;
  r.dist = 3;
  foo->add_point(&r);
  r.ipos = 1;
  r.dist = 2;
  foo->add_point(&r);
  r.qpos = 1;
  foo->add_point(&r);

  adb_query_results_t *rs;
  rs = foo->get_points();
  for (unsigned int k = 0; k < rs->nresults; k++) {
    r = rs->results[k];
    printf("%s: %f %u %u\n", r.key, r.dist, r.qpos, r.ipos);
  }
  free(rs->results);
  free(rs);
  delete foo;
  return 1;
}
