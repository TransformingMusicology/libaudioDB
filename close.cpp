extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"

void audiodb_close(adb_t *adb) {
  free(adb->path);
  free(adb->header);
  delete adb->keys;
  delete adb->keymap;
  delete adb->track_lengths;
  delete adb->track_offsets;
  gsl_rng_free(adb->rng);
  if(adb->cached_lsh) {
    delete adb->cached_lsh;
  }
  divest_lock(adb->fd);
  close(adb->fd);
  free(adb);
}
