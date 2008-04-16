#include "audioDB.h"

/* Make a new database.

   The database consists of:

   * a header (see dbTableHeader struct definition);
   * keyTable: list of keys of tracks;
   * trackTable: Maps implicit feature index to a feature vector
     matrix (sizes of tracks)
   * featureTable: Lots of doubles;
   * timesTable: (start,end) time points for each feature vector;
   * powerTable: associated power for each feature vector;
   * l2normTable: squared l2norms for each feature vector.
*/

void audioDB::create(const char* dbName){
  if ((dbfid = open (dbName, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0)
    error("Can't create database file", dbName, "open");
  get_lock(dbfid, 1);

  VERB_LOG(0, "header size: %ju\n", (intmax_t) O2_HEADERSIZE);
  
  dbH = new dbTableHeaderT();
  assert(dbH);

  //unsigned int maxfiles = (unsigned int) rint((double) O2_MAXFILES * (double) size / (double) O2_DEFAULTDBSIZE);

  // Initialize header
  dbH->magic = O2_MAGIC;
  dbH->version = O2_FORMAT_VERSION;
  dbH->numFiles = 0;
  dbH->dim = 0;
  dbH->flags = 0;
  dbH->headerSize = O2_HEADERSIZE;
  dbH->length = 0;
  dbH->fileTableOffset = ALIGN_PAGE_UP(O2_HEADERSIZE);
  dbH->trackTableOffset = ALIGN_PAGE_UP(dbH->fileTableOffset + O2_FILETABLE_ENTRY_SIZE*ntracks);
  dbH->dataOffset = ALIGN_PAGE_UP(dbH->trackTableOffset + O2_TRACKTABLE_ENTRY_SIZE*ntracks);

  off_t databytes = ((off_t) datasize) * 1024 * 1024;
  off_t auxbytes = databytes / datadim;

  dbH->timesTableOffset = ALIGN_PAGE_UP(dbH->dataOffset + databytes);
  dbH->powerTableOffset = ALIGN_PAGE_UP(dbH->timesTableOffset + 2*auxbytes);
  dbH->l2normTableOffset = ALIGN_PAGE_UP(dbH->powerTableOffset + auxbytes);
  dbH->dbSize = ALIGN_PAGE_UP(dbH->l2normTableOffset + auxbytes);

  write(dbfid, dbH, O2_HEADERSIZE);

  // go to the location corresponding to the last byte
  if (lseek (dbfid, dbH->dbSize - 1, SEEK_SET) == -1)
    error("lseek error in db file", "", "lseek");

  // write a dummy byte at the last location
  if (write (dbfid, "", 1) != 1)
    error("write error", "", "write");

  VERB_LOG(0, "%s %s\n", COM_CREATE, dbName);
}

void audioDB::drop(){
  // FIXME: drop something?  Should we even allow this?
}
