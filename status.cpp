extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"

int audiodb_status(adb_t *adb, adb_status_t *status) {
  /* FIXME: it would be nice to be able to test for "is this database
     pointer valid", but at the moment we punt that to memory
     discipline.  */

  unsigned dudCount = 0;
  unsigned nullCount = 0;

  for(unsigned k = 0; k < adb->header->numFiles; k++) {
    /* FIXME: this bare "16" here reveals a problem (or maybe two).
     * 16 here means the default value of the sequenceLength parameter
     * initializer (both in C++ and corresponding to the "-l" or
     * "--sequencelength" command-line argument).
     *
     * The problem is that the API as currently designed provides no
     * way to pass that information in to this routine; there's no
     * input parameter; nor is there in the SOAP version of this
     * query.  However, there /is/ a way to pass that information on
     * the command-line -- though that codepath is completely
     * untested.  I can see that it might be useful to provide this
     * information, but at present it's probably completely unused, so
     * the compromise for now is to hardwire the 16.
     */ 
    if((*adb->track_lengths)[k] < 16) {
      dudCount++; 
      if(!(*adb->track_lengths)[k]) {
	nullCount++; 
      } 
    }
  }

  status->numFiles = adb->header->numFiles;
  status->dim = adb->header->dim;
  status->length = adb->header->length;
  status->dudCount = dudCount;
  status->nullCount = nullCount;
  status->flags = adb->header->flags;
  status->data_region_size = adb->header->timesTableOffset - adb->header->dataOffset;

  return 0;
}

