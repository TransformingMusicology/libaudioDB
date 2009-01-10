#include "audioDB.h"
extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"

/* Make a new database.

IF size(featuredata) < O2_LARGE_ADB_SIZE
   The database consists of:

   * a header (see dbTableHeader struct definition);
   * keyTable: list of keys of tracks;
   * trackTable: Maps implicit feature index to a feature vector
     matrix (sizes of tracks)
   * featureTable: Lots of doubles;
   * timesTable: (start,end) time points for each feature vector;
   * powerTable: associated power for each feature vector;
   * l2normTable: squared l2norms for each feature vector.

ELSE the database consists of:

   * a header (see dbTableHeader struct definition);
   * keyTable: list of keys of tracks
   * trackTable: sizes of tracks
   * featureTable: list of feature file names
   * timesTable: list of times file names
   * powerTable: list of power file names

*/

adb_t *audiodb_create(const char *path, unsigned datasize, unsigned ntracks, unsigned datadim) {
  int fd;
  adb_header_t *header = 0;
  off_t databytes, auxbytes;
  if(datasize == 0) {
    datasize = O2_DEFAULT_DATASIZE;
  }
  if(ntracks == 0) {
    ntracks = O2_DEFAULT_NTRACKS;
  }
  if(datadim == 0) {
    datadim = O2_DEFAULT_DATADIM;
  }

  if ((fd = open(path, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    goto error;
  }
  if (acquire_lock(fd, true)) {
    goto error;
  }

  header = (adb_header_t *) malloc(sizeof(adb_header_t));
  if(!header) {
    goto error;
  }

  // Initialize header
  header->magic = O2_MAGIC;
  header->version = O2_FORMAT_VERSION;
  header->numFiles = 0;
  header->dim = 0;
  header->flags = 0;
  header->headerSize = O2_HEADERSIZE;
  header->length = 0;
  header->fileTableOffset = ALIGN_PAGE_UP(O2_HEADERSIZE);
  header->trackTableOffset = ALIGN_PAGE_UP(header->fileTableOffset + O2_FILETABLE_ENTRY_SIZE*ntracks);
  header->dataOffset = ALIGN_PAGE_UP(header->trackTableOffset + O2_TRACKTABLE_ENTRY_SIZE*ntracks);

  databytes = ((off_t) datasize) * 1024 * 1024;
  auxbytes = databytes / datadim;

  /* FIXME: what's going on here?  There are two distinct preprocessor
     constants (O2_LSH_N_POINT_BITS, LSH_N_POINT_BITS); a third is
     presumably some default (O2_DEFAULT_LSH_N_POINT_BITS), and then
     there's this magic 28 bits [which needs to be the same as the 28
     in audiodb_lsh_n_point_bits()].  Should this really be part of
     the flags structure at all?  Putting it elsewhere will of course
     break backwards compatibility, unless 14 is the only value that's
     been used anywhere... */

  // For backward-compatibility, Record the point-encoding parameter for LSH indexing in the adb header
  // If this value is 0 then it will be set to 14

  #if LSH_N_POINT_BITS > 15
  #error "AudioDB Compile ERROR: consistency check of O2_LSH_POINT_BITS failed (>31)"
  #endif

  header->flags |= LSH_N_POINT_BITS << 28;

  // If database will fit in a single file the vectors are copied into the AudioDB instance
  // Else all the vectors are left on the FileSystem and we use the dataOffset as storage
  // for the location of the features, powers and times files (assuming that arbitrary keys are used for the fileTable)
  if(ntracks<O2_LARGE_ADB_NTRACKS && datasize<O2_LARGE_ADB_SIZE){
    header->timesTableOffset = ALIGN_PAGE_UP(header->dataOffset + databytes);
    header->powerTableOffset = ALIGN_PAGE_UP(header->timesTableOffset + 2*auxbytes);
    header->l2normTableOffset = ALIGN_PAGE_UP(header->powerTableOffset + auxbytes);
    header->dbSize = ALIGN_PAGE_UP(header->l2normTableOffset + auxbytes);
  } else { // Create LARGE_ADB, features and powers kept on filesystem
    header->flags |= O2_FLAG_LARGE_ADB;
    header->timesTableOffset = ALIGN_PAGE_UP(header->dataOffset + O2_FILETABLE_ENTRY_SIZE*ntracks);
    header->powerTableOffset = ALIGN_PAGE_UP(header->timesTableOffset + O2_FILETABLE_ENTRY_SIZE*ntracks);
    header->l2normTableOffset = ALIGN_PAGE_UP(header->powerTableOffset + O2_FILETABLE_ENTRY_SIZE*ntracks);
    header->dbSize = header->l2normTableOffset;
  }

  write_or_goto_error(fd, header, O2_HEADERSIZE);

  // go to the location corresponding to the last byte
  if (lseek (fd, header->dbSize - 1, SEEK_SET) == -1) {
    goto error;
  }

  // write a dummy byte at the last location
  write_or_goto_error(fd, "", 1);

  free(header);
  return audiodb_open(path, O_RDWR);

 error:
  if(header) {
    free(header);
  }
  return NULL;
}
