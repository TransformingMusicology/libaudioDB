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
  struct flock lock;
  int status;
  
  lock.l_type = exclusive ? F_WRLCK : F_RDLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0; /* "the whole file" */

 retry:
  do {
    status = fcntl(fd, F_SETLKW, &lock);
  } while (status != 0 && errno == EINTR);
  
  if (status) {
    if (errno == EAGAIN) {
      sleep(1);
      goto retry;
    } else {
      error("fcntl lock error", "", "fcntl");
    }
  }
}

void audioDB::release_lock(int fd) {
  struct flock lock;
  int status;

  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  status = fcntl(fd, F_SETLKW, &lock);

  if (status)
    error("fcntl unlock error", "", "fcntl");
}

void audioDB::error(const char* a, const char* b, const char *sysFunc) {
  if(isServer) {
    /* FIXME: I think this is leaky -- we never delete err.  actually
       deleting it is tricky, though; it gets placed into some
       soap-internal struct with uncertain extent... -- CSR,
       2007-10-01 */
    char *err = new char[256]; /* FIXME: overflows */
    snprintf(err, 255, "%s: %s\n%s", a, b, sysFunc ? strerror(errno) : "");
    /* FIXME: actually we could usefully do with a properly structured
       type, so that we can throw separate faultstring and details.
       -- CSR, 2007-10-01 */
    throw(err);
  } else {
    std::cerr << a << ": " << b << std::endl;
    if (sysFunc) {
      perror(sysFunc);
    }
    exit(1);
  }
}

void audioDB::initDBHeader(const char* dbName) {
  if ((dbfid = open(dbName, forWrite ? O_RDWR : O_RDONLY)) < 0) {
    error("Can't open database file", dbName, "open");
  }

  get_lock(dbfid, forWrite);
  // Get the database header info
  dbH = new dbTableHeaderT();
  assert(dbH);
  
  if(read(dbfid, (char *) dbH, O2_HEADERSIZE) != O2_HEADERSIZE) {
    error("error reading db header", dbName, "read");
  }

  if(dbH->magic == O2_OLD_MAGIC) {
    // FIXME: if anyone ever complains, write the program to convert
    // from the old audioDB format to the new...
    error("database file has old O2 header", dbName);
  }

  if(dbH->magic != O2_MAGIC) {
    std::cerr << "expected: " << O2_MAGIC << ", got: " << dbH->magic << std::endl;
    error("database file has incorrect header", dbName);
  }

  if(dbH->version != O2_FORMAT_VERSION) {
    error("database file has incorrect version", dbName);
  }

  if(dbH->headerSize != O2_HEADERSIZE) {
    error("sizeof(dbTableHeader) unexpected: platform ABI mismatch?", dbName);
  }

  CHECKED_MMAP(char *, db, 0, getpagesize());

  // Make some handy tables with correct types
  if(forWrite || (dbH->length > 0)) {
    if(forWrite) {
      fileTableLength = dbH->trackTableOffset - dbH->fileTableOffset;
      trackTableLength = dbH->dataOffset - dbH->trackTableOffset;
      dataBufLength = dbH->timesTableOffset - dbH->dataOffset;
      timesTableLength = dbH->powerTableOffset - dbH->timesTableOffset;
      powerTableLength = dbH->l2normTableOffset - dbH->powerTableOffset;
      l2normTableLength = dbH->dbSize - dbH->l2normTableOffset;
    } else {
      fileTableLength = ALIGN_PAGE_UP(dbH->numFiles * O2_FILETABLE_ENTRY_SIZE);
      trackTableLength = ALIGN_PAGE_UP(dbH->numFiles * O2_TRACKTABLE_ENTRY_SIZE);
      dataBufLength = ALIGN_PAGE_UP(dbH->length);
      timesTableLength = ALIGN_PAGE_UP(2*(dbH->length / dbH->dim));
      powerTableLength = ALIGN_PAGE_UP(dbH->length / dbH->dim);
      l2normTableLength = ALIGN_PAGE_UP(dbH->length / dbH->dim);
    }
    CHECKED_MMAP(char *, fileTable, dbH->fileTableOffset, fileTableLength);
    CHECKED_MMAP(unsigned *, trackTable, dbH->trackTableOffset, trackTableLength);
    /*
     * No more mmap() for dataBuf
     *
     * FIXME: Actually we do do the mmap() in the two cases where it's
     * still "needed": in pointQuery and in l2norm if dbH->length is
     * non-zero.  Removing those cases too (and deleting the dataBuf
     * variable completely) would be cool.  -- CSR, 2007-11-19
     *
     * CHECKED_MMAP(double *, dataBuf, dbH->dataOffset, dataBufLength);
     */
    CHECKED_MMAP(double *, timesTable, dbH->timesTableOffset, timesTableLength);
    CHECKED_MMAP(double *, powerTable, dbH->powerTableOffset, powerTableLength);
    CHECKED_MMAP(double *, l2normTable, dbH->l2normTableOffset, l2normTableLength);
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
    
    if ((indata = (char *) mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, infid, 0)) == (caddr_t) -1) {
      error("mmap error for input", inFile, "mmap");
    }
  }
}

void audioDB::initTables(const char* dbName, const char* inFile = 0) {
  initDBHeader(dbName);
  initInputFile(inFile);
}
