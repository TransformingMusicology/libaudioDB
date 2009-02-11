extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"

adb_liszt_results_t *audiodb_liszt(adb_t *adb) {
  uint32_t nfiles = adb->header->numFiles;
  adb_liszt_results_t *results;
  results = (adb_liszt_results_t *) calloc(sizeof(adb_liszt_results_t),1);
  results->nresults = nfiles;
  if(nfiles > 0) {
    results->entries = (adb_track_entry_t *) malloc(nfiles * sizeof(adb_track_entry_t));
  }
  for(uint32_t k = 0; k < nfiles; k++) {
    results->entries[k].nvectors = (*adb->track_lengths)[k];
    results->entries[k].key = audiodb_index_key(adb, k);
  }
  return results;
}

int audiodb_liszt_free_results(adb_t *adb, adb_liszt_results_t *results) {
  free(results->entries);
  free(results);
  return 0;
}
