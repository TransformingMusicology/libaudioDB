extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"

int audiodb_power(adb_t *adb) {
  if(!(adb->flags & O_RDWR)) {
    return 1;
  }
  if(adb->header->length > 0) {
    return 1;
  }

  adb->header->flags |= ADB_HEADER_FLAG_POWER;
  return audiodb_sync_header(adb);
}
