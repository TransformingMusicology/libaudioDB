#include "audioDB.h"

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

  // For backward-compatibility, Record the point-encoding parameter for LSH indexing in the adb header
  // If this value is 0 then it will be set to 14

#if O2_LSH_N_POINT_BITS > 15
#error "AudioDB Compile ERROR: consistency check of O2_LSH_POINT_BITS failed (>15)"
#endif
  
  dbH->flags |= LSH_N_POINT_BITS << 28;

  // If database will fit in a single file the vectors are copied into the AudioDB instance
  // Else all the vectors are left on the FileSystem and we use the dataOffset as storage
  // for the location of the features, powers and times files (assuming that arbitrary keys are used for the fileTable)
  if(ntracks<O2_LARGE_ADB_NTRACKS && datasize<O2_LARGE_ADB_SIZE){
    dbH->timesTableOffset = ALIGN_PAGE_UP(dbH->dataOffset + databytes);
    dbH->powerTableOffset = ALIGN_PAGE_UP(dbH->timesTableOffset + 2*auxbytes);
    dbH->l2normTableOffset = ALIGN_PAGE_UP(dbH->powerTableOffset + auxbytes);
    dbH->dbSize = ALIGN_PAGE_UP(dbH->l2normTableOffset + auxbytes);
  }
  else{ // Create LARGE_ADB, features and powers kept on filesystem 
    dbH->flags |= O2_FLAG_LARGE_ADB;
    dbH->timesTableOffset = ALIGN_PAGE_UP(dbH->dataOffset + O2_FILETABLE_ENTRY_SIZE*ntracks);
    dbH->powerTableOffset = ALIGN_PAGE_UP(dbH->timesTableOffset + O2_FILETABLE_ENTRY_SIZE*ntracks);
    dbH->l2normTableOffset = ALIGN_PAGE_UP(dbH->powerTableOffset + O2_FILETABLE_ENTRY_SIZE*ntracks);
    dbH->dbSize = dbH->l2normTableOffset;
  } 

  write(dbfid, dbH, O2_HEADERSIZE);

  // go to the location corresponding to the last byte
  if (lseek (dbfid, dbH->dbSize - 1, SEEK_SET) == -1)
    error("lseek error in db file", "", "lseek");

  // write a dummy byte at the last location
  if (write (dbfid, "", 1) != 1)
    error("write error", "", "write");

  VERB_LOG(0, "%s %s\n", COM_CREATE, dbName);
}

