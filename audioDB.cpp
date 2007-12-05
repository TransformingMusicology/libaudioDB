#include "audioDB.h"

#if defined(O2_DEBUG)
void sigterm_action(int signal, siginfo_t *info, void *context) {
  exit(128+signal);
}

void sighup_action(int signal, siginfo_t *info, void *context) {
  // FIXME: reread any configuration files
}
#endif

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
    cerr << a << ": " << b << endl;
    if (sysFunc) {
      perror(sysFunc);
    }
    exit(1);
  }
}

audioDB::audioDB(const unsigned argc, char* const argv[]): O2_AUDIODB_INITIALIZERS
{
  if(processArgs(argc, argv)<0){
    printf("No command found.\n");
    cmdline_parser_print_version ();
    if (strlen(gengetopt_args_info_purpose) > 0)
      printf("%s\n", gengetopt_args_info_purpose);
    printf("%s\n", gengetopt_args_info_usage);
    printf("%s\n", gengetopt_args_info_help[1]);
    printf("%s\n", gengetopt_args_info_help[2]);
    printf("%s\n", gengetopt_args_info_help[0]);
    error("No command found");
  }

  if(O2_ACTION(COM_SERVER))
    startServer();

  else  if(O2_ACTION(COM_CREATE))
    create(dbName);

  else if(O2_ACTION(COM_INSERT))
    insert(dbName, inFile);

  else if(O2_ACTION(COM_BATCHINSERT))
    batchinsert(dbName, inFile);

  else if(O2_ACTION(COM_QUERY))
    if(isClient)
      ws_query(dbName, inFile, (char*)hostport);
    else
      query(dbName, inFile);

  else if(O2_ACTION(COM_STATUS))
    if(isClient)
      ws_status(dbName,(char*)hostport);
    else
      status(dbName);
  
  else if(O2_ACTION(COM_L2NORM))
    l2norm(dbName);
  
  else if(O2_ACTION(COM_POWER))
    power_flag(dbName);

  else if(O2_ACTION(COM_DUMP))
    dump(dbName);
  
  else
    error("Unrecognized command",command);
}

audioDB::audioDB(const unsigned argc, char* const argv[], adb__queryResponse *adbQueryResponse): O2_AUDIODB_INITIALIZERS
{
  try {
    isServer = 1; // FIXME: Hack
    processArgs(argc, argv);
    assert(O2_ACTION(COM_QUERY));
    query(dbName, inFile, adbQueryResponse);
  } catch(char *err) {
    cleanup();
    throw(err);
  }
}

audioDB::audioDB(const unsigned argc, char* const argv[], adb__statusResponse *adbStatusResponse): O2_AUDIODB_INITIALIZERS
{
  try {
    isServer = 1; // FIXME: Hack
    processArgs(argc, argv);
    assert(O2_ACTION(COM_STATUS));
    status(dbName, adbStatusResponse);
  } catch(char *err) {
    cleanup();
    throw(err);
  }
}

void audioDB::cleanup() {
  cmdline_parser_free(&args_info);
  if(indata)
    munmap(indata,statbuf.st_size);
  if(db)
    munmap(db,getpagesize());
  if(fileTable)
    munmap(fileTable, fileTableLength);
  if(trackTable)
    munmap(trackTable, trackTableLength);
  if(dataBuf)
    munmap(dataBuf, dataBufLength);
  if(timesTable)
    munmap(timesTable, timesTableLength);
  if(l2normTable)
    munmap(l2normTable, l2normTableLength);

  if(dbfid>0)
    close(dbfid);
  if(infid>0)
    close(infid);
  if(dbH)
    delete dbH;
}

audioDB::~audioDB(){
  cleanup();
}

int audioDB::processArgs(const unsigned argc, char* const argv[]){

  if(argc<2){
    cmdline_parser_print_version ();
    if (strlen(gengetopt_args_info_purpose) > 0)
      printf("%s\n", gengetopt_args_info_purpose);
    printf("%s\n", gengetopt_args_info_usage);
    printf("%s\n", gengetopt_args_info_help[1]);
    printf("%s\n", gengetopt_args_info_help[2]);
    printf("%s\n", gengetopt_args_info_help[0]);
    exit(0);
  }

  if (cmdline_parser (argc, argv, &args_info) != 0)
    error("Error parsing command line");

  if(args_info.help_given){
    cmdline_parser_print_help();
    exit(0);
  }

  if(args_info.verbosity_given){
    verbosity=args_info.verbosity_arg;
    if(verbosity<0 || verbosity>10){
      cerr << "Warning: verbosity out of range, setting to 1" << endl;
      verbosity=1;
    }
  }

  if(args_info.size_given) {
    if (args_info.size_arg < 50 || args_info.size_arg > 32000) {
      error("Size out of range", "");
    }
    size = (off_t) args_info.size_arg * 1000000;
  }

  if(args_info.radius_given){
    radius=args_info.radius_arg;
    if(radius<=0 || radius>1000000000){
      error("radius out of range");
    }
    else 
      if(verbosity>3) {
	cerr << "Setting radius to " << radius << endl;
      }
  }
  
  if(args_info.SERVER_given){
    command=COM_SERVER;
    port=args_info.SERVER_arg;
    if(port<100 || port > 100000)
      error("port out of range");
    isServer = 1;
#if defined(O2_DEBUG)
    struct sigaction sa;
    sa.sa_sigaction = sigterm_action;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_sigaction = sighup_action;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    sigaction(SIGHUP, &sa, NULL);
#endif
    return 0;
  }

  // No return on client command, find database command
  if(args_info.client_given){
    command=COM_CLIENT;
    hostport=args_info.client_arg;
    isClient=1;
  }

  if(args_info.NEW_given){
    command=COM_CREATE;
    dbName=args_info.database_arg;
    return 0;
  }

  if(args_info.STATUS_given){
    command=COM_STATUS;
    dbName=args_info.database_arg;
    return 0;
  }

  if(args_info.DUMP_given){
    command=COM_DUMP;
    dbName=args_info.database_arg;
    output = args_info.output_arg;
    return 0;
  }

  if(args_info.L2NORM_given){
    command=COM_L2NORM;
    dbName=args_info.database_arg;
    return 0;
  }
       
  if(args_info.POWER_given){
    command=COM_POWER;
    dbName=args_info.database_arg;
    return 0;
  }
       
  if(args_info.INSERT_given){
    command=COM_INSERT;
    dbName=args_info.database_arg;
    inFile=args_info.features_arg;
    if(args_info.key_given)
      key=args_info.key_arg;
    if(args_info.times_given){
      timesFileName=args_info.times_arg;
      if(strlen(timesFileName)>0){
        if(!(timesFile = new ifstream(timesFileName,ios::in)))
          error("Could not open times file for reading", timesFileName);
        usingTimes=1;
      }
    }
    if (args_info.power_given) {
      powerFileName = args_info.power_arg;
      if (strlen(powerFileName) > 0) {
        if (!(powerfd = open(powerFileName, O_RDONLY))) {
          error("Could not open power file for reading", powerFileName, "open");
        }
        usingPower = 1;
      }
    }    
    return 0;
  }
  
  if(args_info.BATCHINSERT_given){
    command=COM_BATCHINSERT;
    dbName=args_info.database_arg;
    inFile=args_info.featureList_arg;
    if(args_info.keyList_given)
      key=args_info.keyList_arg; // INCONSISTENT NO CHECK

    /* TO DO: REPLACE WITH
      if(args_info.keyList_given){
      trackFileName=args_info.keyList_arg;
      if(strlen(trackFileName)>0 && !(trackFile = new ifstream(trackFileName,ios::in)))
      error("Could not open keyList file for reading",trackFileName);
      }
      AND UPDATE BATCHINSERT()
    */
    
    if(args_info.timesList_given){
      timesFileName=args_info.timesList_arg;
      if(strlen(timesFileName)>0){
        if(!(timesFile = new ifstream(timesFileName,ios::in)))
          error("Could not open timesList file for reading", timesFileName);
        usingTimes=1;
      }
    }
    if(args_info.powerList_given){
      powerFileName=args_info.powerList_arg;
      if(strlen(powerFileName)>0){
        if(!(powerFile = new ifstream(powerFileName,ios::in)))
          error("Could not open powerList file for reading", powerFileName);
        usingPower=1;
      }
    }
    return 0;
  }
  
  // Query command and arguments
  if(args_info.QUERY_given){
    command=COM_QUERY;
    dbName=args_info.database_arg;
    inFile=args_info.features_arg;
    
    if(args_info.keyList_given){
      trackFileName=args_info.keyList_arg;
      if(strlen(trackFileName)>0 && !(trackFile = new ifstream(trackFileName,ios::in)))
        error("Could not open keyList file for reading",trackFileName);
    }
    
    if(args_info.times_given){
      timesFileName=args_info.times_arg;
      if(strlen(timesFileName)>0){
        if(!(timesFile = new ifstream(timesFileName,ios::in)))
          error("Could not open times file for reading", timesFileName);
        usingTimes=1;
      }
    }

    if(args_info.power_given){
      powerFileName=args_info.power_arg;
      if(strlen(powerFileName)>0){
        if (!(powerfd = open(powerFileName, O_RDONLY))) {
          error("Could not open power file for reading", powerFileName, "open");
        }
        usingPower = 1;
      }
    }
    
    // query type
    if(strncmp(args_info.QUERY_arg, "track", MAXSTR)==0)
      queryType=O2_TRACK_QUERY;
    else if(strncmp(args_info.QUERY_arg, "point", MAXSTR)==0)
      queryType=O2_POINT_QUERY;
    else if(strncmp(args_info.QUERY_arg, "sequence", MAXSTR)==0)
      queryType=O2_SEQUENCE_QUERY;
    else
      error("unsupported query type",args_info.QUERY_arg);
    
    if(!args_info.exhaustive_flag){
      queryPoint = args_info.qpoint_arg;
      usingQueryPoint=1;
      if(queryPoint<0 || queryPoint >10000)
        error("queryPoint out of range: 0 <= queryPoint <= 10000");
    }
    
    pointNN = args_info.pointnn_arg;
    if(pointNN < 1 || pointNN > 1000) {
      error("pointNN out of range: 1 <= pointNN <= 1000");
    }
    trackNN = args_info.resultlength_arg;
    if(trackNN < 1 || trackNN > 1000) {
      error("resultlength out of range: 1 <= resultlength <= 1000");
    }
    sequenceLength = args_info.sequencelength_arg;
    if(sequenceLength < 1 || sequenceLength > 1000) {
      error("seqlen out of range: 1 <= seqlen <= 1000");
    }
    sequenceHop = args_info.sequencehop_arg;
    if(sequenceHop < 1 || sequenceHop > 1000) {
      error("seqhop out of range: 1 <= seqhop <= 1000");
    }

    if (args_info.absolute_threshold_given) {
      if (args_info.absolute_threshold_arg >= 0) {
	error("absolute threshold out of range: should be negative");
      }
      use_absolute_threshold = true;
      absolute_threshold = args_info.absolute_threshold_arg;
    }
    if (args_info.relative_threshold_given) {
      use_relative_threshold = true;
      relative_threshold = args_info.relative_threshold_arg;
    }
    return 0;
  }
  return -1; // no command found
}

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

  if(verbosity) {
    cerr << "header size:" << O2_HEADERSIZE << endl;
  }
  
  dbH = new dbTableHeaderT();
  assert(dbH);

  unsigned int maxfiles = (unsigned int) rint((double) O2_MAXFILES * (double) size / (double) O2_DEFAULTDBSIZE);

  // Initialize header
  dbH->magic = O2_MAGIC;
  dbH->version = O2_FORMAT_VERSION;
  dbH->numFiles = 0;
  dbH->dim = 0;
  dbH->flags = 0;
  dbH->headerSize = O2_HEADERSIZE;
  dbH->length = 0;
  dbH->fileTableOffset = ALIGN_PAGE_UP(O2_HEADERSIZE);
  dbH->trackTableOffset = ALIGN_PAGE_UP(dbH->fileTableOffset + O2_FILETABLESIZE*maxfiles);
  dbH->dataOffset = ALIGN_PAGE_UP(dbH->trackTableOffset + O2_TRACKTABLESIZE*maxfiles);
  dbH->l2normTableOffset = ALIGN_PAGE_DOWN(size - maxfiles*O2_MEANNUMVECTORS*sizeof(double));
  dbH->powerTableOffset = ALIGN_PAGE_DOWN(dbH->l2normTableOffset - maxfiles*O2_MEANNUMVECTORS*sizeof(double));
  dbH->timesTableOffset = ALIGN_PAGE_DOWN(dbH->powerTableOffset - 2*maxfiles*O2_MEANNUMVECTORS*sizeof(double));
  dbH->dbSize = size;

  write(dbfid, dbH, O2_HEADERSIZE);

  // go to the location corresponding to the last byte
  if (lseek (dbfid, size - 1, SEEK_SET) == -1)
    error("lseek error in db file", "", "lseek");

  // write a dummy byte at the last location
  if (write (dbfid, "", 1) != 1)
    error("write error", "", "write");
  
  if(verbosity) {
    cerr << COM_CREATE << " " << dbName << endl;
  }
}

void audioDB::drop(){
  // FIXME: drop something?  Should we even allow this?
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
    cerr << "expected: " << O2_MAGIC << ", got: " << dbH->magic << endl;
    error("database file has incorrect header", dbName);
  }

  if(dbH->version != O2_FORMAT_VERSION) {
    error("database file has incorrect version", dbName);
  }

  if(dbH->headerSize != O2_HEADERSIZE) {
    error("sizeof(dbTableHeader) unexpected: platform ABI mismatch?", dbName);
  }

#define CHECKED_MMAP(type, var, start, length) \
  { void *tmp = mmap(0, length, (PROT_READ | (forWrite ? PROT_WRITE : 0)), MAP_SHARED, dbfid, (start)); \
    if(tmp == (void *) -1) { \
      error("mmap error for db table", #var, "mmap"); \
    } \
    var = (type) tmp; \
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
      fileTableLength = ALIGN_PAGE_UP(dbH->numFiles * O2_FILETABLESIZE);
      trackTableLength = ALIGN_PAGE_UP(dbH->numFiles * O2_TRACKTABLESIZE);
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
	cerr << "error: expected dimension: " << dbH->dim << ", got : " << test <<endl;
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

bool audioDB::enough_data_space_free(off_t size) {
  return(dbH->timesTableOffset > dbH->dataOffset + dbH->length + size);
}

void audioDB::insert_data_vectors(off_t offset, void *buffer, size_t size) {
  lseek(dbfid, dbH->dataOffset + offset, SEEK_SET);
  write(dbfid, buffer, size);
}

void audioDB::insert(const char* dbName, const char* inFile) {
  forWrite = true;
  initTables(dbName, inFile);

  if(!usingTimes && (dbH->flags & O2_FLAG_TIMES))
    error("Must use timestamps with timestamped database","use --times");

  if(!usingPower && (dbH->flags & O2_FLAG_POWER))
    error("Must use power with power-enabled database", dbName);

  if(!enough_data_space_free(statbuf.st_size - sizeof(int))) {
    error("Insert failed: no more room in database", inFile);
  }

  if(!key)
    key=inFile;
  // Linear scan of filenames check for pre-existing feature
  unsigned alreadyInserted=0;
  for(unsigned k=0; k<dbH->numFiles; k++)
    if(strncmp(fileTable + k*O2_FILETABLESIZE, key, strlen(key)+1)==0){
      alreadyInserted=1;
      break;
    }

  if(alreadyInserted){
    if(verbosity) {
      cerr << "Warning: key already exists in database, ignoring: " <<inFile << endl;
    }
    return;
  }
  
  // Make a track index table of features to file indexes
  unsigned numVectors = (statbuf.st_size-sizeof(int))/(sizeof(double)*dbH->dim);
  if(!numVectors){
    if(verbosity) {
      cerr << "Warning: ignoring zero-length feature vector file:" << key << endl;
    }
    // CLEAN UP
    munmap(indata,statbuf.st_size);
    munmap(db,dbH->dbSize);
    close(infid);
    return;
  }

  strncpy(fileTable + dbH->numFiles*O2_FILETABLESIZE, key, strlen(key));

  off_t insertoffset = dbH->length;// Store current state

  // Check times status and insert times from file
  unsigned indexoffset = insertoffset/(dbH->dim*sizeof(double));
  double *timesdata = timesTable + 2*indexoffset;

  if(2*(indexoffset + numVectors) > timesTableLength) {
    error("out of space for times", key);
  }
  
  if (usingTimes) {
    insertTimeStamps(numVectors, timesFile, timesdata);
  }

  double *powerdata = powerTable + indexoffset;
  insertPowerData(numVectors, powerfd, powerdata);

  // Increment file count
  dbH->numFiles++;

  // Update Header information
  dbH->length+=(statbuf.st_size-sizeof(int));

  // Update track to file index map
  memcpy(trackTable + dbH->numFiles - 1, &numVectors, sizeof(unsigned));  

  insert_data_vectors(insertoffset, indata + sizeof(int), statbuf.st_size - sizeof(int));
  
  // Norm the vectors on input if the database is already L2 normed
  if(dbH->flags & O2_FLAG_L2NORM)
    unitNormAndInsertL2((double *)(indata + sizeof(int)), dbH->dim, numVectors, 1); // append

  // Report status
  status(dbName);
  if(verbosity) {
    cerr << COM_INSERT << " " << dbName << " " << numVectors << " vectors " 
	 << (statbuf.st_size-sizeof(int)) << " bytes." << endl;
  }

  // Copy the header back to the database
  memcpy (db, dbH, sizeof(dbTableHeaderT));  

  // CLEAN UP
  munmap(indata,statbuf.st_size);
  close(infid);
}

void audioDB::insertTimeStamps(unsigned numVectors, ifstream *timesFile, double *timesdata) {
  assert(usingTimes);

  unsigned numtimes = 0;

  if(!(dbH->flags & O2_FLAG_TIMES) && !dbH->numFiles) {
    dbH->flags=dbH->flags|O2_FLAG_TIMES;
  } else if(!(dbH->flags & O2_FLAG_TIMES)) {
    error("Timestamp file used with non-timestamped database", timesFileName);
  }
   
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

void audioDB::insertPowerData(unsigned numVectors, int powerfd, double *powerdata) {
  if (usingPower) {
    if (!(dbH->flags & O2_FLAG_POWER)) {
      error("Cannot insert power data on non-power DB", dbName);
    }

    int one;
    unsigned int count;

    count = read(powerfd, &one, sizeof(unsigned int));
    if (count != sizeof(unsigned int)) {
      error("powerfd read failed", "int", "read");
    }
    if (one != 1) {
      error("dimensionality of power file not 1", powerFileName);
    }

    // FIXME: should check that the powerfile is the right size for
    // this.  -- CSR, 2007-10-30
    count = read(powerfd, powerdata, numVectors * sizeof(double));
    if (count != numVectors * sizeof(double)) {
      error("powerfd read failed", "double", "read");
    }
  }
}

void audioDB::batchinsert(const char* dbName, const char* inFile) {

  forWrite = true;
  initDBHeader(dbName);

  if(!key)
    key=inFile;
  ifstream *filesIn = 0;
  ifstream *keysIn = 0;
  ifstream* thisTimesFile = 0;
  int thispowerfd = 0;

  if(!(filesIn = new ifstream(inFile)))
    error("Could not open batch in file", inFile);
  if(key && key!=inFile)
    if(!(keysIn = new ifstream(key)))
      error("Could not open batch key file",key);
  
  if(!usingTimes && (dbH->flags & O2_FLAG_TIMES))
    error("Must use timestamps with timestamped database","use --times");

  if(!usingPower && (dbH->flags & O2_FLAG_POWER))
    error("Must use power with power-enabled database", dbName);

  unsigned totalVectors=0;
  char *thisKey = new char[MAXSTR];
  char *thisFile = new char[MAXSTR];
  char *thisTimesFileName = new char[MAXSTR];
  char *thisPowerFileName = new char[MAXSTR];
  
  do{
    filesIn->getline(thisFile,MAXSTR);
    if(key && key!=inFile)
      keysIn->getline(thisKey,MAXSTR);
    else
      thisKey = thisFile;
    if(usingTimes)
      timesFile->getline(thisTimesFileName,MAXSTR);	  
    if(usingPower)
      powerFile->getline(thisPowerFileName, MAXSTR);
    
    if(filesIn->eof())
      break;

    initInputFile(thisFile);

    if(!enough_data_space_free(statbuf.st_size - sizeof(int))) {
      error("batchinsert failed: no more room in database", thisFile);
    }
    
    // Linear scan of filenames check for pre-existing feature
    unsigned alreadyInserted=0;
  
    for(unsigned k=0; k<dbH->numFiles; k++)
      if(strncmp(fileTable + k*O2_FILETABLESIZE, thisKey, strlen(thisKey)+1)==0){
	alreadyInserted=1;
	break;
      }
  
    if(alreadyInserted){
      if(verbosity) {
	cerr << "Warning: key already exists in database:" << thisKey << endl;
      }
    }
    else{
  
      // Make a track index table of features to file indexes
      unsigned numVectors = (statbuf.st_size-sizeof(int))/(sizeof(double)*dbH->dim);
      if(!numVectors){
	if(verbosity) {
	  cerr << "Warning: ignoring zero-length feature vector file:" << thisKey << endl;
        }
      }
      else{
	if(usingTimes){
	  if(timesFile->eof()) {
	    error("not enough timestamp files in timesList", timesFileName);
	  }
	  thisTimesFile = new ifstream(thisTimesFileName,ios::in);
	  if(!thisTimesFile->is_open()) {
	    error("Cannot open timestamp file", thisTimesFileName);
	  }
	  off_t insertoffset = dbH->length;
	  unsigned indexoffset = insertoffset / (dbH->dim*sizeof(double));
	  double *timesdata = timesTable + 2*indexoffset;
          if(2*(indexoffset + numVectors) > timesTableLength) {
            error("out of space for times", key);
          }
	  insertTimeStamps(numVectors, thisTimesFile, timesdata);
	  if(thisTimesFile)
	    delete thisTimesFile;
	}
        
        if (usingPower) {
          if(powerFile->eof()) {
            error("not enough power files in powerList", powerFileName);
          }
          thispowerfd = open(thisPowerFileName, O_RDONLY);
          if (thispowerfd < 0) {
            error("failed to open power file", thisPowerFileName);
          }
          unsigned insertoffset = dbH->length;
          unsigned poweroffset = insertoffset / (dbH->dim * sizeof(double));
          double *powerdata = powerTable + poweroffset;
          insertPowerData(numVectors, thispowerfd, powerdata);
          if (0 < thispowerfd) {
            close(thispowerfd);
          }
        }
	strncpy(fileTable + dbH->numFiles*O2_FILETABLESIZE, thisKey, strlen(thisKey));
  
	off_t insertoffset = dbH->length;// Store current state

	// Increment file count
	dbH->numFiles++;  
  
	// Update Header information
	dbH->length+=(statbuf.st_size-sizeof(int));
  
	// Update track to file index map
	memcpy (trackTable+dbH->numFiles-1, &numVectors, sizeof(unsigned));  
	
	insert_data_vectors(insertoffset, indata + sizeof(int), statbuf.st_size - sizeof(int));
	
	// Norm the vectors on input if the database is already L2 normed
	if(dbH->flags & O2_FLAG_L2NORM)
	  unitNormAndInsertL2((double *)(indata + sizeof(int)), dbH->dim, numVectors, 1); // append
	
	totalVectors+=numVectors;

	// Copy the header back to the database
	memcpy (db, dbH, sizeof(dbTableHeaderT));  
      }
    }
    // CLEAN UP
    munmap(indata,statbuf.st_size);
    close(infid);
  }while(!filesIn->eof());

  if(verbosity) {
    cerr << COM_BATCHINSERT << " " << dbName << " " << totalVectors << " vectors " 
	 << totalVectors*dbH->dim*sizeof(double) << " bytes." << endl;
  }
  
  // Report status
  status(dbName);
}

// FIXME: this can't propagate the sequence length argument (used for
// dudCount).  See adb__status() definition for the other half of
// this.  -- CSR, 2007-10-01
void audioDB::ws_status(const char*dbName, char* hostport){
  struct soap soap;
  adb__statusResponse adbStatusResponse;  
  
  // Query an existing adb database
  soap_init(&soap);
  if(soap_call_adb__status(&soap,hostport,NULL,(char*)dbName,adbStatusResponse)==SOAP_OK) {
    cout << "numFiles = " << adbStatusResponse.result.numFiles << endl;
    cout << "dim = " << adbStatusResponse.result.dim << endl;
    cout << "length = " << adbStatusResponse.result.length << endl;
    cout << "dudCount = " << adbStatusResponse.result.dudCount << endl;
    cout << "nullCount = " << adbStatusResponse.result.nullCount << endl;
    cout << "flags = " << adbStatusResponse.result.flags << endl;
  } else {
    soap_print_fault(&soap,stderr);
  }
  
  soap_destroy(&soap);
  soap_end(&soap);
  soap_done(&soap);
}

void audioDB::ws_query(const char*dbName, const char *trackKey, const char* hostport){
  struct soap soap;
  adb__queryResponse adbQueryResponse;  

  soap_init(&soap);  
  if(soap_call_adb__query(&soap,hostport,NULL,
			  (char*)dbName,(char*)trackKey,(char*)trackFileName,(char*)timesFileName,
			  queryType, queryPoint, pointNN, trackNN, sequenceLength, adbQueryResponse)==SOAP_OK){
    //std::cerr << "result list length:" << adbQueryResponse.result.__sizeRlist << std::endl;
    for(int i=0; i<adbQueryResponse.result.__sizeRlist; i++)
      std::cout << adbQueryResponse.result.Rlist[i] << " " << adbQueryResponse.result.Dist[i] 
		<< " " << adbQueryResponse.result.Qpos[i] << " " << adbQueryResponse.result.Spos[i] << std::endl;
  }
  else
    soap_print_fault(&soap,stderr);
  
  soap_destroy(&soap);
  soap_end(&soap);
  soap_done(&soap);

}


void audioDB::status(const char* dbName, adb__statusResponse *adbStatusResponse){
  if(!dbH)
    initTables(dbName, 0);

  unsigned dudCount=0;
  unsigned nullCount=0;
  for(unsigned k=0; k<dbH->numFiles; k++){
    if(trackTable[k]<sequenceLength){
      dudCount++;
      if(!trackTable[k])
        nullCount++;
    }
  }
  
  if(adbStatusResponse == 0) {

    // Update Header information
    cout << "num files:" << dbH->numFiles << endl;
    cout << "data dim:" << dbH->dim <<endl;
    if(dbH->dim>0){
      cout << "total vectors:" << dbH->length/(sizeof(double)*dbH->dim)<<endl;
      cout << "vectors available:" << (dbH->timesTableOffset-(dbH->dataOffset+dbH->length))/(sizeof(double)*dbH->dim) << endl;
    }
    cout << "total bytes:" << dbH->length << " (" << (100.0*dbH->length)/(dbH->timesTableOffset-dbH->dataOffset) << "%)" << endl;
    cout << "bytes available:" << dbH->timesTableOffset-(dbH->dataOffset+dbH->length) << " (" <<
      (100.0*(dbH->timesTableOffset-(dbH->dataOffset+dbH->length)))/(dbH->timesTableOffset-dbH->dataOffset) << "%)" << endl;
    cout << "flags:" << dbH->flags << endl;
    
    cout << "null count: " << nullCount << " small sequence count " << dudCount-nullCount << endl;    
  } else {
    adbStatusResponse->result.numFiles = dbH->numFiles;
    adbStatusResponse->result.dim = dbH->dim;
    adbStatusResponse->result.length = dbH->length;
    adbStatusResponse->result.dudCount = dudCount;
    adbStatusResponse->result.nullCount = nullCount;
    adbStatusResponse->result.flags = dbH->flags;
  }
}

void audioDB::dump(const char* dbName){
  if(!dbH) {
    initTables(dbName, 0);
  }

  if((mkdir(output, S_IRWXU|S_IRWXG|S_IRWXO)) < 0) {
    error("error making output directory", output, "mkdir");
  }

  char *cwd = new char[PATH_MAX];

  if ((getcwd(cwd, PATH_MAX)) == 0) {
    error("error getting working directory", "", "getcwd");
  }

  if((chdir(output)) < 0) {
    error("error changing working directory", output, "chdir");
  }

  int fLfd, tLfd = 0, pLfd = 0, kLfd;
  FILE *fLFile, *tLFile = 0, *pLFile = 0, *kLFile;

  if ((fLfd = open("featureList.txt", O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    error("error creating featureList file", "featureList.txt", "open");
  }

  int times = dbH->flags & O2_FLAG_TIMES;
  if (times) {
    if ((tLfd = open("timesList.txt", O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
      error("error creating timesList file", "timesList.txt", "open");
    }
  }

  int power = dbH->flags & O2_FLAG_POWER;
  if (power) {
    if ((pLfd = open("powerList.txt", O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
      error("error creating powerList file", "powerList.txt", "open");
    }
  }

  if ((kLfd = open("keyList.txt", O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    error("error creating keyList file", "keyList.txt", "open");
  }
  
  /* can these fail?  I sincerely hope not. */
  fLFile = fdopen(fLfd, "w");
  if (times) {
    tLFile = fdopen(tLfd, "w");
  }
  if (power) {
    pLFile = fdopen(pLfd, "w");
  }
  kLFile = fdopen(kLfd, "w");

  char *fName = new char[256];
  int ffd, pfd;
  FILE *tFile;
  unsigned pos = 0;
  lseek(dbfid, dbH->dataOffset, SEEK_SET);
  double *data_buffer;
  size_t data_buffer_size;
  for(unsigned k = 0; k < dbH->numFiles; k++) {
    fprintf(kLFile, "%s\n", fileTable + k*O2_FILETABLESIZE);
    snprintf(fName, 256, "%05d.features", k);
    if ((ffd = open(fName, O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
      error("error creating feature file", fName, "open");
    }
    if ((write(ffd, &dbH->dim, sizeof(uint32_t))) < 0) {
      error("error writing dimensions", fName, "write");
    }

    /* FIXME: this repeated malloc()/free() of data buffers is
       inefficient. */
    data_buffer_size = trackTable[k] * dbH->dim * sizeof(double);

    {
      void *tmp = malloc(data_buffer_size);
      if (tmp == NULL) {
	error("error allocating data buffer");
      }
      data_buffer = (double *) tmp;
    }

    if ((read(dbfid, data_buffer, data_buffer_size)) != (ssize_t) data_buffer_size) {
      error("error reading data", fName, "read");
    }

    if ((write(ffd, data_buffer, data_buffer_size)) < 0) {
      error("error writing data", fName, "write");
    }

    free(data_buffer);

    fprintf(fLFile, "%s\n", fName);
    close(ffd);

    if (times) {
      snprintf(fName, 256, "%05d.times", k);
      tFile = fopen(fName, "w");
      for(unsigned i = 0; i < trackTable[k]; i++) {
        // KLUDGE: specifying 16 digits of precision after the decimal
        // point is (but check this!) sufficient to uniquely identify
        // doubles; however, that will cause ugliness, as that's
        // vastly too many for most values of interest.  Moving to %a
        // here and scanf() in the timesFile reading might fix this.
        // -- CSR, 2007-10-19
        fprintf(tFile, "%.16e\n", *(timesTable + 2*pos + 2*i));
      }
      fprintf(tFile, "%.16e\n", *(timesTable + 2*pos + 2*trackTable[k]-1));

      fprintf(tLFile, "%s\n", fName);
    }

    if (power) {
      uint32_t one = 1;
      snprintf(fName, 256, "%05d.power", k);
      if ((pfd = open(fName, O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
	error("error creating power file", fName, "open");
      }
      if ((write(pfd, &one, sizeof(uint32_t))) < 0) {
	error("error writing one", fName, "write");
      }
      if ((write(pfd, powerTable + pos, trackTable[k] * sizeof(double))) < 0) {
	error("error writing data", fName, "write");
      }
      fprintf(pLFile, "%s\n", fName);
      close(pfd);
    } 

    pos += trackTable[k];
    cout << fileTable+k*O2_FILETABLESIZE << " " << trackTable[k] << endl;
  }

  FILE *scriptFile;
  scriptFile = fopen("restore.sh", "w");
  fprintf(scriptFile, "\
#! /bin/sh\n\
#\n\
# usage: AUDIODB=/path/to/audioDB sh ./restore.sh <newdb>\n\
\n\
if [ -z \"${AUDIODB}\" ]; then echo set AUDIODB variable; exit 1; fi\n\
if [ -z \"$1\" ]; then echo usage: $0 newdb; exit 1; fi\n\n\
\"${AUDIODB}\" -d \"$1\" -N --size=%d\n", (int) (dbH->dbSize / 1000000));
  if(dbH->flags & O2_FLAG_L2NORM) {
    fprintf(scriptFile, "\"${AUDIODB}\" -d \"$1\" -L\n");
  }
  if(power) {
    fprintf(scriptFile, "\"${AUDIODB}\" -d \"$1\" -P\n");
  }
  fprintf(scriptFile, "\"${AUDIODB}\" -d \"$1\" -B -F featureList.txt -K keyList.txt");
  if(times) {
    fprintf(scriptFile, " -T timesList.txt");
  }
  if(power) {
    fprintf(scriptFile, " -W powerList.txt");
  }
  fprintf(scriptFile, "\n");
  fclose(scriptFile);

  if((chdir(cwd)) < 0) {
    error("error changing working directory", cwd, "chdir");
  }

  fclose(fLFile);
  if(times) {
    fclose(tLFile);
  }
  if(power) {
    fclose(pLFile);
  }
  fclose(kLFile);
  delete[] fName;
    
  status(dbName);
}

void audioDB::l2norm(const char* dbName) {
  forWrite = true;
  initTables(dbName, 0);
  if(dbH->length>0){
    /* FIXME: should probably be uint64_t */
    unsigned numVectors = dbH->length/(sizeof(double)*dbH->dim);
    CHECKED_MMAP(double *, dataBuf, dbH->dataOffset, dataBufLength);
    unitNormAndInsertL2(dataBuf, dbH->dim, numVectors, 0); // No append
  }
  // Update database flags
  dbH->flags = dbH->flags|O2_FLAG_L2NORM;
  memcpy (db, dbH, O2_HEADERSIZE);
}

void audioDB::power_flag(const char *dbName) {
  forWrite = true;
  initTables(dbName, 0);
  if (dbH->length > 0) {
    error("cannot turn on power storage for non-empty database", dbName);
  }
  dbH->flags |= O2_FLAG_POWER;
  memcpy(db, dbH, O2_HEADERSIZE);
}

bool audioDB::powers_acceptable(double p1, double p2) {
  if (use_absolute_threshold) {
    if ((p1 < absolute_threshold) || (p2 < absolute_threshold)) {
      return false;
    }
  }
  if (use_relative_threshold) {
    if (fabs(p1-p2) > fabs(relative_threshold)) {
      return false;
    }
  }
  return true;
}

void audioDB::query(const char* dbName, const char* inFile, adb__queryResponse *adbQueryResponse){  
  switch(queryType){
  case O2_POINT_QUERY:
    pointQuery(dbName, inFile, adbQueryResponse);
    break;
  case O2_SEQUENCE_QUERY:
    if(radius==0)
      trackSequenceQueryNN(dbName, inFile, adbQueryResponse);
    else
      trackSequenceQueryRad(dbName, inFile, adbQueryResponse);
    break;
  case O2_TRACK_QUERY:
    trackPointQuery(dbName, inFile, adbQueryResponse);
    break;
  default:
    error("unrecognized queryType in query()");
    
  }  
}

//return ordinal position of key in keyTable
unsigned audioDB::getKeyPos(char* key){  
  for(unsigned k=0; k<dbH->numFiles; k++)
    if(strncmp(fileTable + k*O2_FILETABLESIZE, key, strlen(key))==0)
      return k;
  error("Key not found",key);
  return O2_ERR_KEYNOTFOUND;
}

// Basic point query engine
void audioDB::pointQuery(const char* dbName, const char* inFile, adb__queryResponse *adbQueryResponse) {
  
  initTables(dbName, inFile);
  
  // For each input vector, find the closest pointNN matching output vectors and report
  // we use stdout in this stub version
  unsigned numVectors = (statbuf.st_size-sizeof(int))/(sizeof(double)*dbH->dim);

  double* query = (double*)(indata+sizeof(int));
  CHECKED_MMAP(double *, dataBuf, dbH->dataOffset, dataBufLength);
  double* data = dataBuf;
  double* queryCopy = 0;

  if( dbH->flags & O2_FLAG_L2NORM ){
    // Make a copy of the query
    queryCopy = new double[numVectors*dbH->dim];
    qNorm = new double[numVectors];
    assert(queryCopy&&qNorm);
    memcpy(queryCopy, query, numVectors*dbH->dim*sizeof(double));
    unitNorm(queryCopy, dbH->dim, numVectors, qNorm);
    query = queryCopy;
  }

  // Make temporary dynamic memory for results
  assert(pointNN>0 && pointNN<=O2_MAXNN);
  double distances[pointNN];
  unsigned qIndexes[pointNN];
  unsigned sIndexes[pointNN];
  for(unsigned k=0; k<pointNN; k++){
    distances[k]=-DBL_MAX;
    qIndexes[k]=~0;
    sIndexes[k]=~0;    
  }

  unsigned j=numVectors; 
  unsigned k,l,n;
  double thisDist;

  unsigned totalVecs=dbH->length/(dbH->dim*sizeof(double));
  double meanQdur = 0;
  double *timesdata = 0;
  double *querydurs = 0;
  double *dbdurs = 0;

  if(usingTimes && !(dbH->flags & O2_FLAG_TIMES)){
    cerr << "warning: ignoring query timestamps for non-timestamped database" << endl;
    usingTimes=0;
  }

  else if(!usingTimes && (dbH->flags & O2_FLAG_TIMES))
    cerr << "warning: no timestamps given for query. Ignoring database timestamps." << endl;
  
  else if(usingTimes && (dbH->flags & O2_FLAG_TIMES)){
    timesdata = new double[2*numVectors];
    querydurs = new double[numVectors];
    insertTimeStamps(numVectors, timesFile, timesdata);
    // Calculate durations of points
    for(k=0; k<numVectors-1; k++){
      querydurs[k]=timesdata[2*k+1]-timesdata[2*k];
      meanQdur+=querydurs[k];
    }
    meanQdur/=k;
    // Individual exhaustive timepoint durations
    dbdurs = new double[totalVecs];
    for(k=0; k<totalVecs-1; k++) {
      dbdurs[k]=timesTable[2*k+1]-timesTable[2*k];
    }
  }

  if(usingQueryPoint)
    if(queryPoint>numVectors-1)
      error("queryPoint > numVectors in query");
    else{
      if(verbosity>1) {
	cerr << "query point: " << queryPoint << endl; cerr.flush();
      }
      query=query+queryPoint*dbH->dim;
      numVectors=queryPoint+1;
      j=1;
    }

  gettimeofday(&tv1, NULL);   
  while(j--){ // query
    data=dataBuf;
    k=totalVecs; // number of database vectors
    while(k--){  // database
      thisDist=0;
      l=dbH->dim;
      double* q=query;
      while(l--)
	thisDist+=*q++**data++;
      if(!usingTimes || 
	 (usingTimes 
	  && fabs(dbdurs[totalVecs-k-1]-querydurs[numVectors-j-1])<querydurs[numVectors-j-1]*timesTol)){
	n=pointNN;
	while(n--){
	  if(thisDist>=distances[n]){
	    if((n==0 || thisDist<=distances[n-1])){
	      // Copy all values above up the queue
	      for( l=pointNN-1 ; l >= n+1 ; l--){
		distances[l]=distances[l-1];
		qIndexes[l]=qIndexes[l-1];
		sIndexes[l]=sIndexes[l-1];	      
	      }
	      distances[n]=thisDist;
	      qIndexes[n]=numVectors-j-1;
	      sIndexes[n]=dbH->length/(sizeof(double)*dbH->dim)-k-1;
	      break;
	    }
	  }
	  else
	    break;
	}
      }
    }
    // Move query pointer to next query point
    query+=dbH->dim;
  }

  gettimeofday(&tv2, NULL); 
  if(verbosity>1) {
    cerr << endl << " elapsed time:" << ( tv2.tv_sec*1000 + tv2.tv_usec/1000 ) - ( tv1.tv_sec*1000+tv1.tv_usec/1000 ) << " msec" << endl;
  }

  if(adbQueryResponse==0){
    // Output answer
    // Loop over nearest neighbours    
    for(k=0; k < pointNN; k++){
      // Scan for key
      unsigned cumTrack=0;
      for(l=0 ; l<dbH->numFiles; l++){
	cumTrack+=trackTable[l];
	if(sIndexes[k]<cumTrack){
	  cout << fileTable+l*O2_FILETABLESIZE << " " << distances[k] << " " << qIndexes[k] << " " 
	       << sIndexes[k]+trackTable[l]-cumTrack << endl;
	  break;
	}
      }
    }
  }
  else{ // Process Web Services Query
    int listLen;
    for(k = 0; k < pointNN; k++) {
      if(distances[k] == -DBL_MAX)
        break;
    }
    listLen = k;

    adbQueryResponse->result.__sizeRlist=listLen;
    adbQueryResponse->result.__sizeDist=listLen;
    adbQueryResponse->result.__sizeQpos=listLen;
    adbQueryResponse->result.__sizeSpos=listLen;
    adbQueryResponse->result.Rlist= new char*[listLen];
    adbQueryResponse->result.Dist = new double[listLen];
    adbQueryResponse->result.Qpos = new unsigned int[listLen];
    adbQueryResponse->result.Spos = new unsigned int[listLen];
    for(k=0; k<(unsigned)adbQueryResponse->result.__sizeRlist; k++){
      adbQueryResponse->result.Rlist[k]=new char[O2_MAXFILESTR];
      adbQueryResponse->result.Dist[k]=distances[k];
      adbQueryResponse->result.Qpos[k]=qIndexes[k];
      unsigned cumTrack=0;
      for(l=0 ; l<dbH->numFiles; l++){
	cumTrack+=trackTable[l];
	if(sIndexes[k]<cumTrack){
	  sprintf(adbQueryResponse->result.Rlist[k], "%s", fileTable+l*O2_FILETABLESIZE);
	  break;
	}
      }
      adbQueryResponse->result.Spos[k]=sIndexes[k]+trackTable[l]-cumTrack;
    }
  }
  
  // Clean up
  if(queryCopy)
    delete queryCopy;
  if(qNorm)
    delete qNorm;
  if(timesdata)
    delete[] timesdata;
  if(querydurs)
    delete[] querydurs;
  if(dbdurs)
    delete dbdurs;
}

// trackPointQuery  
// return the trackNN closest tracks to the query track
// uses average of pointNN points per track 
void audioDB::trackPointQuery(const char* dbName, const char* inFile, adb__queryResponse *adbQueryResponse) {
  initTables(dbName, inFile);
  
  // For each input vector, find the closest pointNN matching output vectors and report
  unsigned numVectors = (statbuf.st_size-sizeof(int))/(sizeof(double)*dbH->dim);
  double* query = (double*)(indata+sizeof(int));
  double* data;
  double* queryCopy = 0;

  if( dbH->flags & O2_FLAG_L2NORM ){
    // Make a copy of the query
    queryCopy = new double[numVectors*dbH->dim];
    qNorm = new double[numVectors];
    assert(queryCopy&&qNorm);
    memcpy(queryCopy, query, numVectors*dbH->dim*sizeof(double));
    unitNorm(queryCopy, dbH->dim, numVectors, qNorm);
    query = queryCopy;
  }

  assert(pointNN>0 && pointNN<=O2_MAXNN);
  assert(trackNN>0 && trackNN<=O2_MAXNN);

  // Make temporary dynamic memory for results
  double trackDistances[trackNN];
  unsigned trackIDs[trackNN];
  unsigned trackQIndexes[trackNN];
  unsigned trackSIndexes[trackNN];

  double distances[pointNN];
  unsigned qIndexes[pointNN];
  unsigned sIndexes[pointNN];

  unsigned j=numVectors; // number of query points
  unsigned k,l,n, track, trackOffset=0, processedTracks=0;
  double thisDist;

  for(k=0; k<pointNN; k++){
    distances[k]=-DBL_MAX;
    qIndexes[k]=~0;
    sIndexes[k]=~0;    
  }

  for(k=0; k<trackNN; k++){
    trackDistances[k]=-DBL_MAX;
    trackQIndexes[k]=~0;
    trackSIndexes[k]=~0;
    trackIDs[k]=~0;
  }

  double meanQdur = 0;
  double *timesdata = 0;
  double *querydurs = 0;
  double *meanDBdur = 0;
  
  if(usingTimes && !(dbH->flags & O2_FLAG_TIMES)){
    cerr << "warning: ignoring query timestamps for non-timestamped database" << endl;
    usingTimes=0;
  }
  
  else if(!usingTimes && (dbH->flags & O2_FLAG_TIMES))
    cerr << "warning: no timestamps given for query. Ignoring database timestamps." << endl;
  
  else if(usingTimes && (dbH->flags & O2_FLAG_TIMES)){
    timesdata = new double[2*numVectors];
    querydurs = new double[numVectors];
    insertTimeStamps(numVectors, timesFile, timesdata);
    // Calculate durations of points
    for(k=0; k<numVectors-1; k++) {
      querydurs[k] = timesdata[2*k+1] - timesdata[2*k];
      meanQdur += querydurs[k];
    }
    meanQdur/=k;
    meanDBdur = new double[dbH->numFiles];
    for(k=0; k<dbH->numFiles; k++){
      meanDBdur[k]=0.0;
      for(j=0; j<trackTable[k]-1 ; j++) {
	meanDBdur[k]+=timesTable[2*j+1]-timesTable[2*j];
      }
      meanDBdur[k]/=j;
    }
  }

  if(usingQueryPoint)
    if(queryPoint>numVectors-1)
      error("queryPoint > numVectors in query");
    else{
      if(verbosity>1) {
	cerr << "query point: " << queryPoint << endl; cerr.flush();
      }
      query=query+queryPoint*dbH->dim;
      numVectors=queryPoint+1;
    }
  
  // build track offset table
  off_t *trackOffsetTable = new off_t[dbH->numFiles];
  unsigned cumTrack=0;
  off_t trackIndexOffset;
  for(k=0; k<dbH->numFiles;k++){
    trackOffsetTable[k]=cumTrack;
    cumTrack+=trackTable[k]*dbH->dim;
  }

  char nextKey[MAXSTR];

  gettimeofday(&tv1, NULL); 

  size_t data_buffer_size = 0;
  double *data_buffer = 0;
  lseek(dbfid, dbH->dataOffset, SEEK_SET);
        
  for(processedTracks=0, track=0 ; processedTracks < dbH->numFiles ; track++, processedTracks++){

    trackOffset = trackOffsetTable[track];     // numDoubles offset

    // get trackID from file if using a control file
    if(trackFile) {
      trackFile->getline(nextKey,MAXSTR);
      if(!trackFile->eof()) {
	track = getKeyPos(nextKey);
        trackOffset = trackOffsetTable[track];
        lseek(dbfid, dbH->dataOffset + trackOffset * sizeof(double), SEEK_SET);
      } else {
	break;
      }
    }

    trackIndexOffset=trackOffset/dbH->dim; // numVectors offset

    if(verbosity>7) {
      cerr << track << "." << trackOffset/(dbH->dim) << "." << trackTable[track] << " | ";cerr.flush();
    }

    if(dbH->flags & O2_FLAG_L2NORM)
      usingQueryPoint?query=queryCopy+queryPoint*dbH->dim:query=queryCopy;
    else
      usingQueryPoint?query=(double*)(indata+sizeof(int))+queryPoint*dbH->dim:query=(double*)(indata+sizeof(int));
    if(usingQueryPoint)
      j=1;
    else
      j=numVectors;

    if (trackTable[track] * sizeof(double) * dbH->dim > data_buffer_size) {
      if(data_buffer) {
        free(data_buffer);
      }
      { 
        data_buffer_size = trackTable[track] * sizeof(double) * dbH->dim;
        void *tmp = malloc(data_buffer_size);
        if (tmp == NULL) {
          error("error allocating data buffer");
        }
        data_buffer = (double *) tmp;
      }
    }

    read(dbfid, data_buffer, trackTable[track] * sizeof(double) * dbH->dim);

    while(j--){
      k=trackTable[track];  // number of vectors in track
      data=data_buffer; // data for track
      while(k--){
	thisDist=0;
	l=dbH->dim;
	double* q=query;
	while(l--)
	  thisDist+=*q++**data++;
	if(!usingTimes || 
	   (usingTimes 
	    && fabs(meanDBdur[track]-meanQdur)<meanQdur*timesTol)){
	  n=pointNN;
	  while(n--){
	    if(thisDist>=distances[n]){
	      if((n==0 || thisDist<=distances[n-1])){
		// Copy all values above up the queue
		for( l=pointNN-1 ; l > n ; l--){
		  distances[l]=distances[l-1];
		  qIndexes[l]=qIndexes[l-1];
		  sIndexes[l]=sIndexes[l-1];	      
		}
		distances[n]=thisDist;
		qIndexes[n]=numVectors-j-1;
		sIndexes[n]=trackTable[track]-k-1;
		break;
	      }
	    }
	    else
	      break;
	  }
	}
      } // track
      // Move query pointer to next query point
      query+=dbH->dim;
    } // query 
    // Take the average of this track's distance
    // Test the track distances
    thisDist=0;
    for (n = 0; n < pointNN; n++) {
      if (distances[n] == -DBL_MAX) break;
      thisDist += distances[n];
    }
    thisDist /= n;

    n=trackNN;
    while(n--){
      if(thisDist>=trackDistances[n]){
	if((n==0 || thisDist<=trackDistances[n-1])){
	  // Copy all values above up the queue
	  for( l=trackNN-1 ; l > n ; l--){
	    trackDistances[l]=trackDistances[l-1];
	    trackQIndexes[l]=trackQIndexes[l-1];
	    trackSIndexes[l]=trackSIndexes[l-1];
	    trackIDs[l]=trackIDs[l-1];
	  }
	  trackDistances[n]=thisDist;
	  trackQIndexes[n]=qIndexes[0];
	  trackSIndexes[n]=sIndexes[0];
	  trackIDs[n]=track;
	  break;
	}
      }
      else
	break;
    }
    for(unsigned k=0; k<pointNN; k++){
      distances[k]=-DBL_MAX;
      qIndexes[k]=~0;
      sIndexes[k]=~0;    
    }
  } // tracks

  free(data_buffer);

  gettimeofday(&tv2, NULL); 

  if(verbosity>1) {
    cerr << endl << "processed tracks :" << processedTracks 
	 << " elapsed time:" << ( tv2.tv_sec*1000 + tv2.tv_usec/1000 ) - ( tv1.tv_sec*1000+tv1.tv_usec/1000 ) << " msec" << endl;
  }

  if(adbQueryResponse==0){
    if(verbosity>1) {
      cerr<<endl;
    }
    // Output answer
    // Loop over nearest neighbours
    for(k=0; k < min(trackNN,processedTracks); k++)
      cout << fileTable+trackIDs[k]*O2_FILETABLESIZE 
	   << " " << trackDistances[k] << " " << trackQIndexes[k] << " " << trackSIndexes[k] << endl;
  }
  else{ // Process Web Services Query
    int listLen = min(trackNN, processedTracks);
    adbQueryResponse->result.__sizeRlist=listLen;
    adbQueryResponse->result.__sizeDist=listLen;
    adbQueryResponse->result.__sizeQpos=listLen;
    adbQueryResponse->result.__sizeSpos=listLen;
    adbQueryResponse->result.Rlist= new char*[listLen];
    adbQueryResponse->result.Dist = new double[listLen];
    adbQueryResponse->result.Qpos = new unsigned int[listLen];
    adbQueryResponse->result.Spos = new unsigned int[listLen];
    for(k=0; k<(unsigned)adbQueryResponse->result.__sizeRlist; k++){
      adbQueryResponse->result.Rlist[k]=new char[O2_MAXFILESTR];
      adbQueryResponse->result.Dist[k]=trackDistances[k];
      adbQueryResponse->result.Qpos[k]=trackQIndexes[k];
      adbQueryResponse->result.Spos[k]=trackSIndexes[k];
      sprintf(adbQueryResponse->result.Rlist[k], "%s", fileTable+trackIDs[k]*O2_FILETABLESIZE);
    }
  }
    
  // Clean up
  if(trackOffsetTable)
    delete trackOffsetTable;
  if(queryCopy)
    delete queryCopy;
  if(qNorm)
    delete qNorm;
  if(timesdata)
    delete[] timesdata;
  if(querydurs)
    delete[] querydurs;
  if(meanDBdur)
    delete meanDBdur;
}

// This is a common pattern in sequence queries: what we are doing is
// taking a window of length seqlen over a buffer of length length,
// and placing the sum of the elements in that window in the first
// element of the window: thus replacing all but the last seqlen
// elements in the buffer the corresponding windowed sum.
void audioDB::sequence_sum(double *buffer, int length, int seqlen) {
  double tmp1, tmp2, *ps;
  int j, w;

  tmp1 = *buffer;
  j = 1;
  w = seqlen - 1;
  while(w--) {
    *buffer += buffer[j++];
  }
  ps = buffer + 1;
  w = length - seqlen; // +1 - 1
  while(w--) {
    tmp2 = *ps;
    *ps = *(ps - 1) - tmp1 + *(ps + seqlen - 1);
    tmp1 = tmp2;
    ps++;
  }
}

void audioDB::sequence_sqrt(double *buffer, int length, int seqlen) {
  int w = length - seqlen + 1;
  while(w--) {
    *buffer = sqrt(*buffer);
    buffer++;
  }
}

void audioDB::sequence_average(double *buffer, int length, int seqlen) {
  int w = length - seqlen + 1;
  while(w--) {
    *buffer /= seqlen;
    buffer++;
  }
}

// k nearest-neighbor (k-NN) search between query and target tracks
// efficient implementation based on matched filter
// assumes normed shingles
// outputs distances of retrieved shingles, max retreived = pointNN shingles per per track
void audioDB::trackSequenceQueryNN(const char* dbName, const char* inFile, adb__queryResponse *adbQueryResponse){
  
  initTables(dbName, inFile);
  
  // For each input vector, find the closest pointNN matching output vectors and report
  // we use stdout in this stub version
  unsigned numVectors = (statbuf.st_size-sizeof(int))/(sizeof(double)*dbH->dim);
  double* query = (double*)(indata+sizeof(int));
  double* queryCopy = 0;

  if(!(dbH->flags & O2_FLAG_L2NORM) )
    error("Database must be L2 normed for sequence query","use -L2NORM");

  if(numVectors<sequenceLength)
    error("Query shorter than requested sequence length", "maybe use -l");
  
  if(verbosity>1) {
    cerr << "performing norms ... "; cerr.flush();
  }
  unsigned dbVectors = dbH->length/(sizeof(double)*dbH->dim);

  // Make a copy of the query
  queryCopy = new double[numVectors*dbH->dim];
  memcpy(queryCopy, query, numVectors*dbH->dim*sizeof(double));
  qNorm = new double[numVectors];
  sNorm = new double[dbVectors];
  assert(qNorm&&sNorm&&queryCopy&&sequenceLength);    
  unitNorm(queryCopy, dbH->dim, numVectors, qNorm);
  query = queryCopy;

  // Make norm measurements relative to sequenceLength
  unsigned w = sequenceLength-1;
  unsigned i,j;

  // Copy the L2 norm values to core to avoid disk random access later on
  memcpy(sNorm, l2normTable, dbVectors*sizeof(double));
  double* qnPtr = qNorm;
  double* snPtr = sNorm;

  double *sPower = 0, *qPower = 0;
  double *spPtr = 0, *qpPtr = 0;

  if (usingPower) {
    if (!(dbH->flags & O2_FLAG_POWER)) {
      error("database not power-enabled", dbName);
    }
    sPower = new double[dbVectors];
    spPtr = sPower;
    memcpy(sPower, powerTable, dbVectors * sizeof(double));
  }

  for(i=0; i<dbH->numFiles; i++){
    if(trackTable[i]>=sequenceLength) {
      sequence_sum(snPtr, trackTable[i], sequenceLength);
      sequence_sqrt(snPtr, trackTable[i], sequenceLength);

      if (usingPower) {
	sequence_sum(spPtr, trackTable[i], sequenceLength);
        sequence_average(spPtr, trackTable[i], sequenceLength);
      }
    }
    snPtr += trackTable[i];
    if (usingPower) {
      spPtr += trackTable[i];
    }
  }
  
  sequence_sum(qnPtr, numVectors, sequenceLength);
  sequence_sqrt(qnPtr, numVectors, sequenceLength);

  if (usingPower) {
    qPower = new double[numVectors];
    qpPtr = qPower;
    if (lseek(powerfd, sizeof(int), SEEK_SET) == (off_t) -1) {
      error("error seeking to data", powerFileName, "lseek");
    }
    int count = read(powerfd, qPower, numVectors * sizeof(double));
    if (count == -1) {
      error("error reading data", powerFileName, "read");
    }
    if ((unsigned) count != numVectors * sizeof(double)) {
      error("short read", powerFileName);
    }

    sequence_sum(qpPtr, numVectors, sequenceLength);
    sequence_average(qpPtr, numVectors, sequenceLength);
  }

  if(verbosity>1) {
    cerr << "done." << endl;
  }
  
  if(verbosity>1) {
    cerr << "matching tracks..." << endl;
  }
  
  assert(pointNN>0 && pointNN<=O2_MAXNN);
  assert(trackNN>0 && trackNN<=O2_MAXNN);
  
  // Make temporary dynamic memory for results
  double trackDistances[trackNN];
  unsigned trackIDs[trackNN];
  unsigned trackQIndexes[trackNN];
  unsigned trackSIndexes[trackNN];
  
  double distances[pointNN];
  unsigned qIndexes[pointNN];
  unsigned sIndexes[pointNN];
  

  unsigned k,l,m,n,track,trackOffset=0, HOP_SIZE=sequenceHop, wL=sequenceLength;
  double thisDist;
  
  for(k=0; k<pointNN; k++){
    distances[k]=1.0e6;
    qIndexes[k]=~0;
    sIndexes[k]=~0;    
  }
  
  for(k=0; k<trackNN; k++){
    trackDistances[k]=1.0e6;
    trackQIndexes[k]=~0;
    trackSIndexes[k]=~0;
    trackIDs[k]=~0;
  }

  // Timestamp and durations processing
  double meanQdur = 0;
  double *timesdata = 0;
  double *querydurs = 0;
  double *meanDBdur = 0;
  
  if(usingTimes && !(dbH->flags & O2_FLAG_TIMES)){
    cerr << "warning: ignoring query timestamps for non-timestamped database" << endl;
    usingTimes=0;
  }
  
  else if(!usingTimes && (dbH->flags & O2_FLAG_TIMES))
    cerr << "warning: no timestamps given for query. Ignoring database timestamps." << endl;
  
  else if(usingTimes && (dbH->flags & O2_FLAG_TIMES)){
    timesdata = new double[2*numVectors];
    querydurs = new double[numVectors];

    insertTimeStamps(numVectors, timesFile, timesdata);
    // Calculate durations of points
    for(k=0; k<numVectors-1; k++) {
      querydurs[k] = timesdata[2*k+1] - timesdata[2*k];
      meanQdur += querydurs[k];
    }
    meanQdur/=k;
    if(verbosity>1) {
      cerr << "mean query file duration: " << meanQdur << endl;
    }
    meanDBdur = new double[dbH->numFiles];
    assert(meanDBdur);
    for(k=0; k<dbH->numFiles; k++){
      meanDBdur[k]=0.0;
      for(j=0; j<trackTable[k]-1 ; j++) {
	meanDBdur[k]+=timesTable[2*j+1]-timesTable[2*j];
      }
      meanDBdur[k]/=j;
    }
  }

  if(usingQueryPoint)
    if(queryPoint>numVectors || queryPoint>numVectors-wL+1)
      error("queryPoint > numVectors-wL+1 in query");
    else{
      if(verbosity>1) {
	cerr << "query point: " << queryPoint << endl; cerr.flush();
      }
      query = query + queryPoint * dbH->dim;
      qnPtr = qnPtr + queryPoint;
      if (usingPower) {
        qpPtr = qpPtr + queryPoint;
      }
      numVectors=wL;
    }
  
  double ** D = 0;    // Differences query and target 
  double ** DD = 0;   // Matched filter distance

  D = new double*[numVectors];
  assert(D);
  DD = new double*[numVectors];
  assert(DD);

  gettimeofday(&tv1, NULL); 
  unsigned processedTracks = 0;
  unsigned successfulTracks=0;

  double* qp;
  double* sp;
  double* dp;

  // build track offset table
  off_t *trackOffsetTable = new off_t[dbH->numFiles];
  unsigned cumTrack=0;
  off_t trackIndexOffset;
  for(k=0; k<dbH->numFiles;k++){
    trackOffsetTable[k]=cumTrack;
    cumTrack+=trackTable[k]*dbH->dim;
  }

  char nextKey [MAXSTR];

  // chi^2 statistics
  double sampleCount = 0;
  double sampleSum = 0;
  double logSampleSum = 0;
  double minSample = 1e9;
  double maxSample = 0;

  // Track loop 
  size_t data_buffer_size = 0;
  double *data_buffer = 0;
  lseek(dbfid, dbH->dataOffset, SEEK_SET);

  for(processedTracks=0, track=0 ; processedTracks < dbH->numFiles ; track++, processedTracks++) {

    trackOffset = trackOffsetTable[track];     // numDoubles offset

    // get trackID from file if using a control file
    if(trackFile) {
      trackFile->getline(nextKey,MAXSTR);
      if(!trackFile->eof()) {
	track = getKeyPos(nextKey);
        trackOffset = trackOffsetTable[track];
        lseek(dbfid, dbH->dataOffset + trackOffset * sizeof(double), SEEK_SET);
      } else {
	break;
      }
    }

    trackIndexOffset=trackOffset/dbH->dim; // numVectors offset

    if (trackTable[track] * sizeof(double) * dbH->dim > data_buffer_size) {
      if(data_buffer) {
	free(data_buffer);
      }
      { 
	data_buffer_size = trackTable[track] * sizeof(double) * dbH->dim;
	void *tmp = malloc(data_buffer_size);
	if (tmp == NULL) {
	  error("error allocating data buffer");
	}
	data_buffer = (double *) tmp;
      }
    }
    
    read(dbfid, data_buffer, trackTable[track] * sizeof(double) * dbH->dim);

    if(sequenceLength<=trackTable[track]){  // test for short sequences
      
      if(verbosity>7) {
	cerr << track << "." << trackIndexOffset << "." << trackTable[track] << " | ";cerr.flush();
      }
		
      // Sum products matrix
      for(j=0; j<numVectors;j++){
	D[j]=new double[trackTable[track]]; 
	assert(D[j]);

      }

      // Matched filter matrix
      for(j=0; j<numVectors;j++){
	DD[j]=new double[trackTable[track]];
	assert(DD[j]);
      }

      // Dot product
      for(j=0; j<numVectors; j++)
	for(k=0; k<trackTable[track]; k++){
	  qp=query+j*dbH->dim;
	  sp=data_buffer+k*dbH->dim;
	  DD[j][k]=0.0; // Initialize matched filter array
	  dp=&D[j][k];  // point to correlation cell j,k
	  *dp=0.0;      // initialize correlation cell
	  l=dbH->dim;         // size of vectors
	  while(l--)
	    *dp+=*qp++**sp++;
	}
  
      // Matched Filter
      // HOP SIZE == 1
      double* spd;
      if(HOP_SIZE==1){ // HOP_SIZE = shingleHop
	for(w=0; w<wL; w++)
	  for(j=0; j<numVectors-w; j++){ 
	    sp=DD[j];
	    spd=D[j+w]+w;
	    k=trackTable[track]-w;
	    while(k--)
	      *sp+++=*spd++;
	  }
      }

      else{ // HOP_SIZE != 1
	for(w=0; w<wL; w++)
	  for(j=0; j<numVectors-w; j+=HOP_SIZE){
	    sp=DD[j];
	    spd=D[j+w]+w;
	    for(k=0; k<trackTable[track]-w; k+=HOP_SIZE){
	      *sp+=*spd;
	      sp+=HOP_SIZE;
	      spd+=HOP_SIZE;
	    }
	  }
      }
      
      if(verbosity>3 && usingTimes) {
	cerr << "meanQdur=" << meanQdur << " meanDBdur=" << meanDBdur[track] << endl;
	cerr.flush();
      }

      if(!usingTimes || 
	 (usingTimes 
	  && fabs(meanDBdur[track]-meanQdur)<meanQdur*timesTol)){

	if(verbosity>3 && usingTimes) {
	  cerr << "within duration tolerance." << endl;
	  cerr.flush();
	}

	// Search for minimum distance by shingles (concatenated vectors)
	for(j=0;j<=numVectors-wL;j+=HOP_SIZE)
	  for(k=0;k<=trackTable[track]-wL;k+=HOP_SIZE){
	    thisDist=2-(2/(qnPtr[j]*sNorm[trackIndexOffset+k]))*DD[j][k];
	    if(verbosity>9) {
	      cerr << thisDist << " " << qnPtr[j] << " " << sNorm[trackIndexOffset+k] << endl;
            }
	    // Gather chi^2 statistics
	    if(thisDist<minSample)
	      minSample=thisDist;
	    else if(thisDist>maxSample)
	      maxSample=thisDist;
	    if(thisDist>1e-9){
	      sampleCount++;
	      sampleSum+=thisDist;
	      logSampleSum+=log(thisDist);
	    }

	    // diffL2 = fabs(qnPtr[j] - sNorm[trackIndexOffset+k]);
	    // Power test
	    if (usingPower) {
	      if (!(powers_acceptable(qpPtr[j], sPower[trackIndexOffset + k]))) {
		thisDist = 1000000.0;
	      }
	    }

	    // k-NN match algorithm
	    m=pointNN;
	    while(m--){
	      if(thisDist<=distances[m])
		if(m==0 || thisDist>=distances[m-1]){
		// Shuffle distances up the list
		for(l=pointNN-1; l>m; l--){
		  distances[l]=distances[l-1];
		  qIndexes[l]=qIndexes[l-1];
		  sIndexes[l]=sIndexes[l-1];
		}
		distances[m]=thisDist;
		if(usingQueryPoint)
		  qIndexes[m]=queryPoint;
		else
		  qIndexes[m]=j;
		sIndexes[m]=k;
		break;
		}
	    }
	  }
	// Calculate the mean of the N-Best matches
	thisDist=0.0;
	for(m=0; m<pointNN; m++) {
          if (distances[m] == 1000000.0) break;
	  thisDist+=distances[m];
        }
	thisDist/=m;
	
	// Let's see the distances then...
	if(verbosity>3) {
	  cerr << fileTable+track*O2_FILETABLESIZE << " " << thisDist << endl;
        }


	// All the track stuff goes here
	n=trackNN;
	while(n--){
	  if(thisDist<=trackDistances[n]){
	    if((n==0 || thisDist>=trackDistances[n-1])){
	      // Copy all values above up the queue
	      for( l=trackNN-1 ; l > n ; l--){
		trackDistances[l]=trackDistances[l-1];
		trackQIndexes[l]=trackQIndexes[l-1];
		trackSIndexes[l]=trackSIndexes[l-1];
		trackIDs[l]=trackIDs[l-1];
	      }
	      trackDistances[n]=thisDist;
	      trackQIndexes[n]=qIndexes[0];
	      trackSIndexes[n]=sIndexes[0];
	      successfulTracks++;
	      trackIDs[n]=track;
	      break;
	    }
	  }
	  else
	    break;
	}
      } // Duration match
            
      // Clean up current track
      if(D!=NULL){
	for(j=0; j<numVectors; j++)
	  delete[] D[j];
      }

      if(DD!=NULL){
	for(j=0; j<numVectors; j++)
	  delete[] DD[j];
      }
    }
    // per-track reset array values
    for(unsigned k=0; k<pointNN; k++){
      distances[k]=1.0e6;
      qIndexes[k]=~0;
      sIndexes[k]=~0;    
    }
  }

  free(data_buffer);

  gettimeofday(&tv2,NULL);
  if(verbosity>1) {
    cerr << endl << "processed tracks :" << processedTracks << " matched tracks: " << successfulTracks << " elapsed time:" 
	 << ( tv2.tv_sec*1000 + tv2.tv_usec/1000 ) - ( tv1.tv_sec*1000+tv1.tv_usec/1000 ) << " msec" << endl;
    cerr << "sampleCount: " << sampleCount << " sampleSum: " << sampleSum << " logSampleSum: " << logSampleSum 
	 << " minSample: " << minSample << " maxSample: " << maxSample << endl;
  }  
  if(adbQueryResponse==0){
    if(verbosity>1) {
      cerr<<endl;
    }
    // Output answer
    // Loop over nearest neighbours
    for(k=0; k < min(trackNN,successfulTracks); k++)
      cout << fileTable+trackIDs[k]*O2_FILETABLESIZE << " " << trackDistances[k] << " " 
	   << trackQIndexes[k] << " " << trackSIndexes[k] << endl;
  }
  else{ // Process Web Services Query
    int listLen = min(trackNN, processedTracks);
    adbQueryResponse->result.__sizeRlist=listLen;
    adbQueryResponse->result.__sizeDist=listLen;
    adbQueryResponse->result.__sizeQpos=listLen;
    adbQueryResponse->result.__sizeSpos=listLen;
    adbQueryResponse->result.Rlist= new char*[listLen];
    adbQueryResponse->result.Dist = new double[listLen];
    adbQueryResponse->result.Qpos = new unsigned int[listLen];
    adbQueryResponse->result.Spos = new unsigned int[listLen];
    for(k=0; k<(unsigned)adbQueryResponse->result.__sizeRlist; k++){
      adbQueryResponse->result.Rlist[k]=new char[O2_MAXFILESTR];
      adbQueryResponse->result.Dist[k]=trackDistances[k];
      adbQueryResponse->result.Qpos[k]=trackQIndexes[k];
      adbQueryResponse->result.Spos[k]=trackSIndexes[k];
      sprintf(adbQueryResponse->result.Rlist[k], "%s", fileTable+trackIDs[k]*O2_FILETABLESIZE);
    }
  }

  // Clean up
  if(trackOffsetTable)
    delete[] trackOffsetTable;
  if(queryCopy)
    delete[] queryCopy;
  if(qNorm)
    delete[] qNorm;
  if(sNorm)
    delete[] sNorm;
  if(qPower)
    delete[] qPower;
  if(sPower) 
    delete[] sPower;
  if(D)
    delete[] D;
  if(DD)
    delete[] DD;
  if(timesdata)
    delete[] timesdata;
  if(querydurs)
    delete[] querydurs;
  if(meanDBdur)
    delete[] meanDBdur;
}

// Radius search between query and target tracks
// efficient implementation based on matched filter
// assumes normed shingles
// outputs count of retrieved shingles, max retreived = one shingle per query shingle per track
void audioDB::trackSequenceQueryRad(const char* dbName, const char* inFile, adb__queryResponse *adbQueryResponse){
  
  initTables(dbName, inFile);
  
  // For each input vector, find the closest pointNN matching output vectors and report
  // we use stdout in this stub version
  unsigned numVectors = (statbuf.st_size-sizeof(int))/(sizeof(double)*dbH->dim);
  double* query = (double*)(indata+sizeof(int));
  double* queryCopy = 0;

  if(!(dbH->flags & O2_FLAG_L2NORM) )
    error("Database must be L2 normed for sequence query","use -l2norm");
  
  if(verbosity>1) {
    cerr << "performing norms ... "; cerr.flush();
  }
  unsigned dbVectors = dbH->length/(sizeof(double)*dbH->dim);

  // Make a copy of the query
  queryCopy = new double[numVectors*dbH->dim];
  memcpy(queryCopy, query, numVectors*dbH->dim*sizeof(double));
  qNorm = new double[numVectors];
  sNorm = new double[dbVectors];
  assert(qNorm&&sNorm&&queryCopy&&sequenceLength);    
  unitNorm(queryCopy, dbH->dim, numVectors, qNorm);
  query = queryCopy;

  // Make norm measurements relative to sequenceLength
  unsigned w = sequenceLength-1;
  unsigned i,j;

  // Copy the L2 norm values to core to avoid disk random access later on
  memcpy(sNorm, l2normTable, dbVectors*sizeof(double));
  double* snPtr = sNorm;
  double* qnPtr = qNorm;

  double *sPower = 0, *qPower = 0;
  double *spPtr = 0, *qpPtr = 0;

  if (usingPower) {
    if(!(dbH->flags & O2_FLAG_POWER)) {
      error("database not power-enabled", dbName);
    }
    sPower = new double[dbVectors];
    spPtr = sPower;
    memcpy(sPower, powerTable, dbVectors * sizeof(double));
  }

  for(i=0; i<dbH->numFiles; i++){
    if(trackTable[i]>=sequenceLength) {
      sequence_sum(snPtr, trackTable[i], sequenceLength);
      sequence_sqrt(snPtr, trackTable[i], sequenceLength);
      if (usingPower) {
        sequence_sum(spPtr, trackTable[i], sequenceLength);
        sequence_average(spPtr, trackTable[i], sequenceLength);
      }
    }
    snPtr += trackTable[i];
    if (usingPower) {
      spPtr += trackTable[i];
    }
  }
  
  sequence_sum(qnPtr, numVectors, sequenceLength);
  sequence_sqrt(qnPtr, numVectors, sequenceLength);

  if (usingPower) {
    qPower = new double[numVectors];
    qpPtr = qPower;
    if (lseek(powerfd, sizeof(int), SEEK_SET) == (off_t) -1) {
      error("error seeking to data", powerFileName, "lseek");
    }
    int count = read(powerfd, qPower, numVectors * sizeof(double));
    if (count == -1) {
      error("error reading data", powerFileName, "read");
    }
    if ((unsigned) count != numVectors * sizeof(double)) {
      error("short read", powerFileName);
    }

    sequence_sum(qpPtr, numVectors, sequenceLength);
    sequence_average(qpPtr, numVectors, sequenceLength);
  }

  if(verbosity>1) {
    cerr << "done." << endl;    
  }
  
  if(verbosity>1) {
    cerr << "matching tracks..." << endl;
  }
  
  assert(pointNN>0 && pointNN<=O2_MAXNN);
  assert(trackNN>0 && trackNN<=O2_MAXNN);
  
  // Make temporary dynamic memory for results
  double trackDistances[trackNN];
  unsigned trackIDs[trackNN];
  unsigned trackQIndexes[trackNN];
  unsigned trackSIndexes[trackNN];
  
  double distances[pointNN];
  unsigned qIndexes[pointNN];
  unsigned sIndexes[pointNN];
  

  unsigned k,l,n,track,trackOffset=0, HOP_SIZE=sequenceHop, wL=sequenceLength;
  double thisDist;
  
  for(k=0; k<pointNN; k++){
    distances[k]=0.0;
    qIndexes[k]=~0;
    sIndexes[k]=~0;    
  }
  
  for(k=0; k<trackNN; k++){
    trackDistances[k]=0.0;
    trackQIndexes[k]=~0;
    trackSIndexes[k]=~0;
    trackIDs[k]=~0;
  }

  // Timestamp and durations processing
  double meanQdur = 0;
  double *timesdata = 0;
  double *querydurs = 0;
  double *meanDBdur = 0;
  
  if(usingTimes && !(dbH->flags & O2_FLAG_TIMES)){
    cerr << "warning: ignoring query timestamps for non-timestamped database" << endl;
    usingTimes=0;
  }
  
  else if(!usingTimes && (dbH->flags & O2_FLAG_TIMES))
    cerr << "warning: no timestamps given for query. Ignoring database timestamps." << endl;
  
  else if(usingTimes && (dbH->flags & O2_FLAG_TIMES)){
    timesdata = new double[2*numVectors];
    querydurs = new double[numVectors];

    insertTimeStamps(numVectors, timesFile, timesdata);
    // Calculate durations of points
    for(k=0; k<numVectors-1; k++){
      querydurs[k] = timesdata[2*k+1] - timesdata[2*k];
      meanQdur += querydurs[k];
    }
    meanQdur/=k;
    if(verbosity>1) {
      cerr << "mean query file duration: " << meanQdur << endl;
    }
    meanDBdur = new double[dbH->numFiles];
    assert(meanDBdur);
    for(k=0; k<dbH->numFiles; k++){
      meanDBdur[k]=0.0;
      for(j=0; j<trackTable[k]-1 ; j++) {
	meanDBdur[k]+=timesTable[2*j+1]-timesTable[2*j];
      }
      meanDBdur[k]/=j;
    }
  }

  if(usingQueryPoint)
    if(queryPoint>numVectors || queryPoint>numVectors-wL+1)
      error("queryPoint > numVectors-wL+1 in query");
    else{
      if(verbosity>1) {
	cerr << "query point: " << queryPoint << endl; cerr.flush();
      }
      query = query + queryPoint*dbH->dim;
      qnPtr = qnPtr + queryPoint;
      if (usingPower) {
        qpPtr = qpPtr + queryPoint;
      }
      numVectors=wL;
    }
  
  double ** D = 0;    // Differences query and target 
  double ** DD = 0;   // Matched filter distance

  D = new double*[numVectors];
  assert(D);
  DD = new double*[numVectors];
  assert(DD);

  gettimeofday(&tv1, NULL); 
  unsigned processedTracks = 0;
  unsigned successfulTracks=0;

  double* qp;
  double* sp;
  double* dp;

  // build track offset table
  off_t *trackOffsetTable = new off_t[dbH->numFiles];
  unsigned cumTrack=0;
  off_t trackIndexOffset;
  for(k=0; k<dbH->numFiles;k++){
    trackOffsetTable[k]=cumTrack;
    cumTrack+=trackTable[k]*dbH->dim;
  }

  char nextKey [MAXSTR];

  // chi^2 statistics
  double sampleCount = 0;
  double sampleSum = 0;
  double logSampleSum = 0;
  double minSample = 1e9;
  double maxSample = 0;

  // Track loop 
  size_t data_buffer_size = 0;
  double *data_buffer = 0;
  lseek(dbfid, dbH->dataOffset, SEEK_SET);

  for(processedTracks=0, track=0 ; processedTracks < dbH->numFiles ; track++, processedTracks++){

    trackOffset = trackOffsetTable[track];     // numDoubles offset

    // get trackID from file if using a control file
    if(trackFile) {
      trackFile->getline(nextKey,MAXSTR);
      if(!trackFile->eof()) {
	track = getKeyPos(nextKey);
        trackOffset = trackOffsetTable[track];
        lseek(dbfid, dbH->dataOffset + trackOffset * sizeof(double), SEEK_SET);
      } else {
	break;
      }
    }

    trackIndexOffset=trackOffset/dbH->dim; // numVectors offset

    if (trackTable[track] * sizeof(double) * dbH->dim > data_buffer_size) {
      if(data_buffer) {
	free(data_buffer);
      }
      { 
	data_buffer_size = trackTable[track] * sizeof(double) * dbH->dim;
	void *tmp = malloc(data_buffer_size);
	if (tmp == NULL) {
	  error("error allocating data buffer");
	}
	data_buffer = (double *) tmp;
      }
    }
    
    read(dbfid, data_buffer, trackTable[track] * sizeof(double) * dbH->dim);

    if(sequenceLength<=trackTable[track]){  // test for short sequences
      
      if(verbosity>7) {
	cerr << track << "." << trackIndexOffset << "." << trackTable[track] << " | ";cerr.flush();
      }

      // Sum products matrix
      for(j=0; j<numVectors;j++){
	D[j]=new double[trackTable[track]]; 
	assert(D[j]);

      }

      // Matched filter matrix
      for(j=0; j<numVectors;j++){
	DD[j]=new double[trackTable[track]];
	assert(DD[j]);
      }

      // Dot product
      for(j=0; j<numVectors; j++)
	for(k=0; k<trackTable[track]; k++){
	  qp=query+j*dbH->dim;
	  sp=data_buffer+k*dbH->dim;
	  DD[j][k]=0.0; // Initialize matched filter array
	  dp=&D[j][k];  // point to correlation cell j,k
	  *dp=0.0;      // initialize correlation cell
	  l=dbH->dim;         // size of vectors
	  while(l--)
	    *dp+=*qp++**sp++;
	}

      // Matched Filter
      // HOP SIZE == 1
      double* spd;
      if(HOP_SIZE==1){ // HOP_SIZE = shingleHop
	for(w=0; w<wL; w++)
	  for(j=0; j<numVectors-w; j++){ 
	    sp=DD[j];
	    spd=D[j+w]+w;
	    k=trackTable[track]-w;
	    while(k--)
	      *sp+++=*spd++;
	  }
      }

      else{ // HOP_SIZE != 1
	for(w=0; w<wL; w++)
	  for(j=0; j<numVectors-w; j+=HOP_SIZE){
	    sp=DD[j];
	    spd=D[j+w]+w;
	    for(k=0; k<trackTable[track]-w; k+=HOP_SIZE){
	      *sp+=*spd;
	      sp+=HOP_SIZE;
	      spd+=HOP_SIZE;
	    }
	  }
      }
      
      if(verbosity>3 && usingTimes) {
	cerr << "meanQdur=" << meanQdur << " meanDBdur=" << meanDBdur[track] << endl;
	cerr.flush();
      }

      if(!usingTimes || 
	 (usingTimes 
	  && fabs(meanDBdur[track]-meanQdur)<meanQdur*timesTol)){

	if(verbosity>3 && usingTimes) {
	  cerr << "within duration tolerance." << endl;
	  cerr.flush();
	}

	// Search for minimum distance by shingles (concatenated vectors)
	for(j=0;j<=numVectors-wL;j+=HOP_SIZE)
	  for(k=0;k<=trackTable[track]-wL;k+=HOP_SIZE){
	    thisDist=2-(2/(qnPtr[j]*sNorm[trackIndexOffset+k]))*DD[j][k];
	    if(verbosity>9) {
	      cerr << thisDist << " " << qnPtr[j] << " " << sNorm[trackIndexOffset+k] << endl;
            }
	    // Gather chi^2 statistics
	    if(thisDist<minSample)
	      minSample=thisDist;
	    else if(thisDist>maxSample)
	      maxSample=thisDist;
	    if(thisDist>1e-9){
	      sampleCount++;
	      sampleSum+=thisDist;
	      logSampleSum+=log(thisDist);
	    }

	    // diffL2 = fabs(qnPtr[j] - sNorm[trackIndexOffset+k]);
	    // Power test
            if (usingPower) {
              if (!(powers_acceptable(qpPtr[j], sPower[trackIndexOffset + k]))) {
                thisDist = 1000000.0;
              }
            }

	    if(thisDist>=0 && thisDist<=radius){
	      distances[0]++; // increment count
	      break; // only need one track point per query point
	    }
	  }
	// How many points were below threshold ?
	thisDist=distances[0];
	
	// Let's see the distances then...
	if(verbosity>3) {
	  cerr << fileTable+track*O2_FILETABLESIZE << " " << thisDist << endl;
        }

	// All the track stuff goes here
	n=trackNN;
	while(n--){
	  if(thisDist>trackDistances[n]){
	    if((n==0 || thisDist<=trackDistances[n-1])){
	      // Copy all values above up the queue
	      for( l=trackNN-1 ; l > n ; l--){
		trackDistances[l]=trackDistances[l-1];
		trackQIndexes[l]=trackQIndexes[l-1];
		trackSIndexes[l]=trackSIndexes[l-1];
		trackIDs[l]=trackIDs[l-1];
	      }
	      trackDistances[n]=thisDist;
	      trackQIndexes[n]=qIndexes[0];
	      trackSIndexes[n]=sIndexes[0];
	      successfulTracks++;
	      trackIDs[n]=track;
	      break;
	    }
	  }
	  else
	    break;
	}
      } // Duration match
            
      // Clean up current track
      if(D!=NULL){
	for(j=0; j<numVectors; j++)
	  delete[] D[j];
      }

      if(DD!=NULL){
	for(j=0; j<numVectors; j++)
	  delete[] DD[j];
      }
    }
    // per-track reset array values
    for(unsigned k=0; k<pointNN; k++){
      distances[k]=0.0;
      qIndexes[k]=~0;
      sIndexes[k]=~0;    
    }
  }

  free(data_buffer);

  gettimeofday(&tv2,NULL);
  if(verbosity>1) {
    cerr << endl << "processed tracks :" << processedTracks << " matched tracks: " << successfulTracks << " elapsed time:" 
	 << ( tv2.tv_sec*1000 + tv2.tv_usec/1000 ) - ( tv1.tv_sec*1000+tv1.tv_usec/1000 ) << " msec" << endl;
    cerr << "sampleCount: " << sampleCount << " sampleSum: " << sampleSum << " logSampleSum: " << logSampleSum 
	 << " minSample: " << minSample << " maxSample: " << maxSample << endl;
  }
  
  if(adbQueryResponse==0){
    if(verbosity>1) {
      cerr<<endl;
    }
    // Output answer
    // Loop over nearest neighbours
    for(k=0; k < min(trackNN,successfulTracks); k++)
      cout << fileTable+trackIDs[k]*O2_FILETABLESIZE << " " << trackDistances[k] << endl;
  }
  else{ // Process Web Services Query
    int listLen = min(trackNN, processedTracks);
    adbQueryResponse->result.__sizeRlist=listLen;
    adbQueryResponse->result.__sizeDist=listLen;
    adbQueryResponse->result.__sizeQpos=listLen;
    adbQueryResponse->result.__sizeSpos=listLen;
    adbQueryResponse->result.Rlist= new char*[listLen];
    adbQueryResponse->result.Dist = new double[listLen];
    adbQueryResponse->result.Qpos = new unsigned int[listLen];
    adbQueryResponse->result.Spos = new unsigned int[listLen];
    for(k=0; k<(unsigned)adbQueryResponse->result.__sizeRlist; k++){
      adbQueryResponse->result.Rlist[k]=new char[O2_MAXFILESTR];
      adbQueryResponse->result.Dist[k]=trackDistances[k];
      adbQueryResponse->result.Qpos[k]=trackQIndexes[k];
      adbQueryResponse->result.Spos[k]=trackSIndexes[k];
      sprintf(adbQueryResponse->result.Rlist[k], "%s", fileTable+trackIDs[k]*O2_FILETABLESIZE);
    }
  }

  // Clean up
  if(trackOffsetTable)
    delete[] trackOffsetTable;
  if(queryCopy)
    delete[] queryCopy;
  if(qNorm)
    delete[] qNorm;
  if(sNorm)
    delete[] sNorm;
  if(qPower)
    delete[] qPower;
  if(sPower) 
    delete[] sPower;
  if(D)
    delete[] D;
  if(DD)
    delete[] DD;
  if(timesdata)
    delete[] timesdata;
  if(querydurs)
    delete[] querydurs;
  if(meanDBdur)
    delete[] meanDBdur;
}

// Unit norm block of features
void audioDB::unitNorm(double* X, unsigned dim, unsigned n, double* qNorm){
  unsigned d;
  double L2, *p;
  if(verbosity>2) {
    cerr << "norming " << n << " vectors...";cerr.flush();
  }
  while(n--){
    p=X;
    L2=0.0;
    d=dim;
    while(d--){
      L2+=*p**p;
      p++;
    }
    /*    L2=sqrt(L2);*/
    if(qNorm)
      *qNorm++=L2;
    /*
    oneOverL2 = 1.0/L2;
    d=dim;
    while(d--){
      *X*=oneOverL2;
      X++;
    */
    X+=dim;
  }
  if(verbosity>2) {
    cerr << "done..." << endl;
  }
}

// Unit norm block of features
void audioDB::unitNormAndInsertL2(double* X, unsigned dim, unsigned n, unsigned append=0){
  unsigned d;
  double *p;
  unsigned nn = n;

  assert(l2normTable);

  if( !append && (dbH->flags & O2_FLAG_L2NORM) )
    error("Database is already L2 normed", "automatic norm on insert is enabled");

  if(verbosity>2) {
    cerr << "norming " << n << " vectors...";cerr.flush();
  }

  double* l2buf = new double[n];
  double* l2ptr = l2buf;
  assert(l2buf);
  assert(X);

  while(nn--){
    p=X;
    *l2ptr=0.0;
    d=dim;
    while(d--){
      *l2ptr+=*p**p;
      p++;
    }
    l2ptr++;
    X+=dim;
  }
  unsigned offset;
  if(append) {
    // FIXME: a hack, a very palpable hack: the vectors have already
    // been inserted, and dbH->length has already been updated.  We
    // need to subtract off again the number of vectors that we've
    // inserted this time...
    offset=(dbH->length/(dbH->dim*sizeof(double)))-n; // number of vectors
  } else {
    offset=0;
  }
  memcpy(l2normTable+offset, l2buf, n*sizeof(double));
  if(l2buf)
    delete[] l2buf;
  if(verbosity>2) {
    cerr << "done..." << endl;
  }
}


// Start an audioDB server on the host
void audioDB::startServer(){
  struct soap soap;
  int m, s; // master and slave sockets
  soap_init(&soap);
  // FIXME: largely this use of SO_REUSEADDR is to make writing (and
  // running) test cases more convenient, so that multiple test runs
  // in close succession don't fail because of a bin() error.
  // Investigate whether there are any potential drawbacks in this,
  // and also whether there's a better way to write the tests.  --
  // CSR, 2007-10-03
  soap.bind_flags |= SO_REUSEADDR;
  m = soap_bind(&soap, NULL, port, 100);
  if (m < 0)
    soap_print_fault(&soap, stderr);
  else
    {
      fprintf(stderr, "Socket connection successful: master socket = %d\n", m);
      for (int i = 1; ; i++)
	{
	  s = soap_accept(&soap);
	  if (s < 0)
	    {
	      soap_print_fault(&soap, stderr);
	      break;
	    }
	  fprintf(stderr, "%d: accepted connection from IP=%lu.%lu.%lu.%lu socket=%d\n", i,
		  (soap.ip >> 24)&0xFF, (soap.ip >> 16)&0xFF, (soap.ip >> 8)&0xFF, soap.ip&0xFF, s);
	  if (soap_serve(&soap) != SOAP_OK) // process RPC request
	    soap_print_fault(&soap, stderr); // print error
	  fprintf(stderr, "request served\n");
	  soap_destroy(&soap); // clean up class instances
	  soap_end(&soap); // clean up everything and close socket
	}
    }
  soap_done(&soap); // close master socket and detach environment
} 


// web services

// SERVER SIDE
int adb__status(struct soap* soap, xsd__string dbName, adb__statusResponse &adbStatusResponse){
  char* const argv[]={"audioDB",COM_STATUS,"-d",dbName};
  const unsigned argc = 4;
  try {
    audioDB(argc, argv, &adbStatusResponse);
    return SOAP_OK;
  } catch(char *err) {
    soap_receiver_fault(soap, err, "");
    return SOAP_FAULT;
  }
}

// Literal translation of command line to web service

int adb__query(struct soap* soap, xsd__string dbName, xsd__string qKey, xsd__string keyList, xsd__string timesFileName, xsd__int qType, xsd__int qPos, xsd__int pointNN, xsd__int trackNN, xsd__int seqLen, adb__queryResponse &adbQueryResponse){
  char queryType[256];
  for(int k=0; k<256; k++)
    queryType[k]='\0';
  if(qType == O2_POINT_QUERY)
    strncpy(queryType, "point", strlen("point"));
  else if (qType == O2_SEQUENCE_QUERY)
    strncpy(queryType, "sequence", strlen("sequence"));
  else if(qType == O2_TRACK_QUERY)
    strncpy(queryType,"track", strlen("track"));
  else
    strncpy(queryType, "", strlen(""));

  if(pointNN==0)
    pointNN=10;
  if(trackNN==0)
    trackNN=10;
  if(seqLen==0)
    seqLen=16;

  char qPosStr[256];
  sprintf(qPosStr, "%d", qPos);
  char pointNNStr[256];
  sprintf(pointNNStr,"%d",pointNN);
  char trackNNStr[256];
  sprintf(trackNNStr,"%d",trackNN);
  char seqLenStr[256];  
  sprintf(seqLenStr,"%d",seqLen);
  
  const  char* argv[] ={
    "./audioDB", 
    COM_QUERY, 
    queryType, // Need to pass a parameter
    COM_DATABASE,
    ENSURE_STRING(dbName),
    COM_FEATURES,
    ENSURE_STRING(qKey),
    COM_KEYLIST,
    ENSURE_STRING(keyList),
    COM_TIMES,
    ENSURE_STRING(timesFileName),
    COM_QPOINT, 
    qPosStr,
    COM_POINTNN,
    pointNNStr,
    COM_TRACKNN,
    trackNNStr, // Need to pass a parameter
    COM_SEQLEN,
    seqLenStr
  };

  const unsigned argc = 19;
  try {
    audioDB(argc, (char* const*)argv, &adbQueryResponse);
    return SOAP_OK;
  } catch (char *err) {
    soap_receiver_fault(soap, err, "");
    return SOAP_FAULT;
  }
}

int adb__sequenceQuery(struct soap* soap, xsd__string dbName, xsd__string qKey,
		       adb__sequenceQueryParms *parms,
		       adb__queryResponse &adbQueryResponse) {

  char qPosStr[256];
  char pointNNStr[256];
  char trackNNStr[256];
  char seqLenStr[256];
  char relative_thresholdStr[256];
  char absolute_thresholdStr[256];

  /* When the branch is merged, move this to a header and use it
     elsewhere */
#define INTSTRINGIFY(val, str) \
  snprintf(str, 256, "%d", val);
#define DOUBLESTRINGIFY(val, str) \
  snprintf(str, 256, "%f", val);

  INTSTRINGIFY(parms->qPos, qPosStr);
  INTSTRINGIFY(parms->pointNN, pointNNStr);
  INTSTRINGIFY(parms->segNN, trackNNStr);
  /* FIXME: decide which of segLen and seqLen should live */
  INTSTRINGIFY(parms->segLen, seqLenStr);

  DOUBLESTRINGIFY(parms->relative_threshold, relative_thresholdStr);
  DOUBLESTRINGIFY(parms->absolute_threshold, absolute_thresholdStr);
  
  const char *argv[] = {
    "./audioDB",
    COM_QUERY,
    "sequence",
    COM_DATABASE,
    dbName, 
    COM_FEATURES,
    qKey,
    COM_KEYLIST,
    /* FIXME: when this branch is merged, use ENSURE_STRING */
    parms->keyList==0?"":parms->keyList,
    COM_TIMES,
    parms->timesFileName==0?"":parms->timesFileName,
    COM_QUERYPOWER,
    parms->powerFileName==0?"":parms->powerFileName,
    COM_QPOINT, 
    qPosStr,
    COM_POINTNN,
    pointNNStr,
    COM_TRACKNN,
    trackNNStr,
    COM_SEQLEN,
    seqLenStr,
    COM_RELATIVE_THRESH,
    relative_thresholdStr,
    COM_ABSOLUTE_THRESH,
    absolute_thresholdStr
  };

  const unsigned argc = 25;

  try {
    audioDB(argc, (char* const*)argv, &adbQueryResponse);
    return SOAP_OK;
  } catch (char *err) {
    soap_receiver_fault(soap, err, "");
    return SOAP_FAULT;
  }
}

int main(const unsigned argc, char* const argv[]){
  audioDB(argc, argv);
}
