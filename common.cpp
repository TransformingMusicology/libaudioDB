#include "audioDB.h"

#if defined(O2_DEBUG)
void sigterm_action(int signal, siginfo_t *info, void *context) {
  exit(128+signal);
}

void sighup_action(int signal, siginfo_t *info, void *context) {
  // FIXME: reread any configuration files
}
#endif

void audioDB::get_lock(int fd, bool exclusive) {
  if(acquire_lock(fd, exclusive)) {
    error("fcntl lock error", "", "fcntl");
  }
}

void audioDB::release_lock(int fd) {
  if (divest_lock(fd)) {
    error("fcntl unlock error", "", "fcntl");
  }
}

void audioDB::error(const char* a, const char* b, const char *sysFunc) {
 

    if(isServer) {
        /* FIXME: I think this is leaky -- we never delete err.
           actually deleting it is tricky, though; it gets placed into
           some soap-internal struct with uncertain extent... -- CSR,
           2007-10-01 */
        char *err = new char[256]; /* FIXME: overflows */
        snprintf(err, 255, "%s: %s\n%s", a, b, sysFunc ? strerror(errno) : "");
        /* FIXME: actually we could usefully do with a properly
           structured type, so that we can throw separate faultstring
           and details.  -- CSR, 2007-10-01 */
        throw(err);
    } else {
        std::cerr << a << ": " << b << std::endl;
        if (sysFunc) {
            perror(sysFunc);
        }
        exit(1);
    }
}

void audioDB::initRNG() {
  rng = gsl_rng_alloc(gsl_rng_mt19937);
  if(!rng) {
    error("could not allocate Random Number Generator");
  }
  /* FIXME: maybe we should use a real source of entropy? */
  gsl_rng_set(rng, time(NULL));
}

void audioDB::initDBHeader(const char* dbName) {
  if(!adb) {
    adb = audiodb_open(dbName, forWrite ? O_RDWR : O_RDONLY);
    if(!adb) {
      error("Failed to open database", dbName);
    }
  }
  dbfid = adb->fd;
  dbH = adb->header;

  // Make some handy tables with correct types
  if(forWrite || (dbH->length > 0)) {
    if(forWrite) {
      fileTableLength = dbH->trackTableOffset - dbH->fileTableOffset;
      trackTableLength = dbH->dataOffset - dbH->trackTableOffset;
      timesTableLength = dbH->powerTableOffset - dbH->timesTableOffset;
      powerTableLength = dbH->l2normTableOffset - dbH->powerTableOffset;
      l2normTableLength = dbH->dbSize - dbH->l2normTableOffset;
    } else {
      fileTableLength = ALIGN_PAGE_UP(dbH->numFiles * O2_FILETABLE_ENTRY_SIZE);
      trackTableLength = ALIGN_PAGE_UP(dbH->numFiles * O2_TRACKTABLE_ENTRY_SIZE);
      if( dbH->flags & O2_FLAG_LARGE_ADB ){
	timesTableLength = ALIGN_PAGE_UP(dbH->numFiles * O2_FILETABLE_ENTRY_SIZE);
	powerTableLength = ALIGN_PAGE_UP(dbH->numFiles * O2_FILETABLE_ENTRY_SIZE);
	l2normTableLength = 0;
      }
      else{
	timesTableLength = ALIGN_PAGE_UP(2*(dbH->length / dbH->dim));
	powerTableLength = ALIGN_PAGE_UP(dbH->length / dbH->dim);
	l2normTableLength = ALIGN_PAGE_UP(dbH->length / dbH->dim);
      }
    }
    CHECKED_MMAP(char *, fileTable, dbH->fileTableOffset, fileTableLength);
    CHECKED_MMAP(unsigned *, trackTable, dbH->trackTableOffset, trackTableLength);
    if( dbH->flags & O2_FLAG_LARGE_ADB ){
      CHECKED_MMAP(char *, featureFileNameTable, dbH->dataOffset, fileTableLength);
      if( dbH->flags & O2_FLAG_TIMES )
	CHECKED_MMAP(char *, timesFileNameTable, dbH->timesTableOffset, fileTableLength);
      if( dbH->flags & O2_FLAG_POWER )
	CHECKED_MMAP(char *, powerFileNameTable, dbH->powerTableOffset, fileTableLength);
    }
    else{
      CHECKED_MMAP(double *, timesTable, dbH->timesTableOffset, timesTableLength);
      CHECKED_MMAP(double *, powerTable, dbH->powerTableOffset, powerTableLength);
      CHECKED_MMAP(double *, l2normTable, dbH->l2normTableOffset, l2normTableLength);
    }
  }
}

void audioDB::initInputFile (const char *inFile) {
  if (inFile) {
    if ((infid = open(inFile, O_RDONLY)) < 0) {
      error("can't open input file for reading", inFile, "open");
    }

    if (fstat(infid, &statbuf) < 0) {
      error("fstat error finding size of input", inFile, "fstat");
    }

    if(dbH->dim == 0 && dbH->length == 0) { // empty database
      // initialize with input dimensionality
      if(read(infid, &dbH->dim, sizeof(unsigned)) != sizeof(unsigned)) {
        error("short read of input file", inFile);
      }
      if(dbH->dim == 0) {
        error("dimensionality of zero in input file", inFile);
      }
    } else {
      unsigned test;
      if(read(infid, &test, sizeof(unsigned)) != sizeof(unsigned)) {
        error("short read of input file", inFile);
      }
      if(dbH->dim == 0) {
        error("dimensionality of zero in input file", inFile);
      }
      if(dbH->dim != test) {      
	std::cerr << "error: expected dimension: " << dbH->dim << ", got : " << test <<std::endl;
	error("feature dimensions do not match database table dimensions", inFile);
      }
    }
  }
}

void audioDB::initTables(const char* dbName, const char* inFile) {
  /* FIXME: initRNG() really logically belongs in the audioDB
     contructor.  However, there are of the order of four constructors
     at the moment, and more to come from API implementation.  Given
     that duplication, I think this is the least worst place to put
     it; the assumption is that nothing which doesn't look at a
     database will need an RNG.  -- CSR, 2008-07-02 */
  initRNG();
  initDBHeader(dbName);
  if(inFile)
    initInputFile(inFile);
}

// If name is relative path, side effect name with prefix/name
// Do not free original pointer
void audioDB::prefix_name(char** const name, const char* prefix){
  // No prefix if prefix is empty
  if(!prefix)
    return;
  // Allocate new memory, keep old memory
  assert(name && *name);
  if (strlen(*name) + strlen(prefix) + 1 > O2_MAXFILESTR)
    error("error: path prefix + filename too long",prefix);
  // Do not prefix absolute path+filename
  if(**name=='/')
    return;
  // OK to prefix relative path+filename
  char* prefixedName = (char*) malloc(O2_MAXFILESTR);
  sprintf(prefixedName, "%s/%s", prefix, *name);
  *name = prefixedName; // side effect new name to old name
}

void audioDB::insertTimeStamps(unsigned numVectors, std::ifstream *timesFile, double *timesdata) {
  assert(usingTimes);

  unsigned numtimes = 0;

  if(!timesFile->is_open()) {
    error("problem opening times file on timestamped database", timesFileName);
  }

  double timepoint, next;
  *timesFile >> timepoint;
  if (timesFile->eof()) {
    error("no entries in times file", timesFileName);
  }
  numtimes++;
  do {
    *timesFile >> next;
    if (timesFile->eof()) {
      break;
    }
    numtimes++;
    timesdata[0] = timepoint;
    timepoint = (timesdata[1] = next);
    timesdata += 2;
  } while (numtimes < numVectors + 1);

  if (numtimes < numVectors + 1) {
    error("too few timepoints in times file", timesFileName);
  }

  *timesFile >> next;
  if (!timesFile->eof()) {
    error("too many timepoints in times file", timesFileName);
  }
}
