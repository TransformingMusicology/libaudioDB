extern "C" {
#include "audioDB/audioDB_API.h"
}
#include "audioDB/audioDB-internals.h"

int audiodb_retrieve_datum(adb_t *adb, const char *key, adb_datum_t *datum) {
  uint32_t index = audiodb_key_index(adb, key);
  if(index == (uint32_t) -1) {
    return 1;
  } else {
    return audiodb_track_id_datum(adb, index, datum);
  }
}

int audiodb_free_datum(adb_t *adb, adb_datum_t *datum) {
  return audiodb_really_free_datum(datum);
}
