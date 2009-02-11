#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TESTDB "testdb"

void clean_remove_db(char * dbname) {
  unlink(dbname);
}

void maketestfile(const char *path, int dim, double *doubles, int ndoubles) {
  FILE *file;

  file = fopen(path, "w");
  fwrite(&dim, sizeof(int), 1, file);
  fwrite(doubles, sizeof(double), ndoubles, file);
  fflush(file);
  fclose(file);
}

int close_enough(double a, double b, double epsilon) {
  return (fabs(a-b) < epsilon);
}

int result_position(adb_query_results_t *r, const char *key, float dist, uint32_t qpos, uint32_t ipos) {
  for(uint32_t k = 0; k < r->nresults; k++) {
    adb_result_t result = r->results[k];
    if(close_enough(dist, result.dist, 1e-4) && (qpos == result.qpos) &&
       (ipos == result.ipos) && !(strcmp(key, result.key))) {
      return k;
    }
  }
  return -1;
}

#define result_present_or_fail(r, k, d, q, i) \
  if(result_position(r, k, d, q, i) < 0) return 1;

int entry_position(adb_liszt_results_t *l, const char *key, uint32_t nvectors) {
  for(uint32_t k = 0; k < l->nresults; k++) {
    adb_track_entry_t entry = l->entries[k];
    if((nvectors == entry.nvectors) && !strcmp(key, entry.key)) {
      return k;
    }
  }
  return -1;
}

#define entry_present_or_fail(l, k, n) \
  if(entry_position(l, k, n) < 0) return 1;
