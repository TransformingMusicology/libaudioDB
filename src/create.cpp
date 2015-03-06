extern "C" {
#include "audioDB/audioDB_API.h"
}
#include "audioDB/audioDB-internals.h"

/* Make a new database.

(FIXME: this text, in particular the conditional, will not be true 
once we implement create flags rather than defaulting on format based
on the requested size arguments)

IF size(featuredata) < ADB_FIXME_LARGE_ADB_SIZE
   The database consists of:

   * a header (see adb_header_t definition);
   * keyTable: list of keys of tracks;
   * trackTable: Maps implicit feature index to a feature vector
     matrix (sizes of tracks)
   * featureTable: Lots of doubles;
   * timesTable: (start,end) time points for each feature vector;
   * powerTable: associated power for each feature vector;
   * l2normTable: squared l2norms for each feature vector.

ELSE the database consists of:

   * a header (see adb_header_t definition);
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
    datasize = ADB_DEFAULT_DATASIZE;
  }
  if(ntracks == 0) {
    ntracks = ADB_DEFAULT_NTRACKS;
  }
  if(datadim == 0) {
    datadim = ADB_DEFAULT_DATADIM;
  }

  if ((fd = open(path, O_RDWR|O_CREAT|O_EXCL, ADB_CREAT_PERMISSIONS)) < 0) {
    goto error;
  }

  header = (adb_header_t *) malloc(sizeof(adb_header_t));
  if(!header) {
    goto error;
  }

  // Initialize header
  header->magic = ADB_MAGIC;
  header->version = ADB_FORMAT_VERSION;
  header->numFiles = 0;
  header->dim = 0;
  header->flags = 0;
  header->headerSize = ADB_HEADER_SIZE;
  header->length = 0;
  header->fileTableOffset = align_page_up(ADB_HEADER_SIZE);
  header->trackTableOffset = align_page_up(header->fileTableOffset + ADB_FILETABLE_ENTRY_SIZE*ntracks); //
  header->dataOffset = align_page_up(header->trackTableOffset + ADB_TRACKTABLE_ENTRY_SIZE*ntracks);

  databytes = ((off_t) datasize) * 1024 * 1024;
  auxbytes = databytes / datadim;

  // If database will fit in a single file the vectors are copied into the AudioDB instance
  // Else all the vectors are left on the FileSystem and we use the dataOffset as storage
  // for the location of the features, powers and times files (assuming that arbitrary keys are used for the fileTable)
  if(ntracks < ADB_FIXME_LARGE_ADB_NTRACKS && datasize < ADB_FIXME_LARGE_ADB_SIZE) {
    header->timesTableOffset = align_page_up(header->dataOffset + databytes);
    header->powerTableOffset = align_page_up(header->timesTableOffset + 2*auxbytes);
    header->l2normTableOffset = align_page_up(header->powerTableOffset + auxbytes);
    header->dbSize = align_page_up(header->l2normTableOffset + auxbytes);
  } else { // Create REFERENCES ADB, features and powers kept on filesystem
    header->flags |= ADB_HEADER_FLAG_REFERENCES;
    header->timesTableOffset = align_page_up(header->dataOffset + ADB_FILETABLE_ENTRY_SIZE*ntracks);
    header->powerTableOffset = align_page_up(header->timesTableOffset + ADB_FILETABLE_ENTRY_SIZE*ntracks);
    header->l2normTableOffset = align_page_up(header->powerTableOffset + ADB_FILETABLE_ENTRY_SIZE*ntracks);
    header->dbSize = header->l2normTableOffset;
  }

  write_or_goto_error(fd, header, ADB_HEADER_SIZE);

  // go to the location corresponding to the last byte of the database
  // file, and write a byte there.
  lseek_set_or_goto_error(fd, header->dbSize - 1);
  write_or_goto_error(fd, "", 1);

  free(header);
  return audiodb_open(path, O_RDWR);

 error:
  maybe_free(header);
  return NULL;
}
