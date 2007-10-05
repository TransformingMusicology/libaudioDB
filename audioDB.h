#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <sys/time.h>
#include <assert.h>
#include <float.h>
#include <signal.h>

// includes for web services
#include "soapH.h"
#include "adb.nsmap"
#include "cmdline.h"

#define MAXSTR 512

// Databse PRIMARY commands
#define COM_CREATE "--NEW"
#define COM_INSERT "--INSERT"
#define COM_BATCHINSERT "--BATCHINSERT"
#define COM_QUERY "--QUERY"
#define COM_STATUS "--STATUS"
#define COM_L2NORM "--L2NORM"
#define COM_DUMP "--DUMP"
#define COM_SERVER "--SERVER"

// parameters
#define COM_CLIENT "--client"
#define COM_DATABASE "--database"
#define COM_QTYPE "--qtype"
#define COM_SEQLEN "--sequencelength"
#define COM_SEQHOP "--sequencehop"
#define COM_POINTNN "--pointnn"
#define COM_TRACKNN "--resultlength"
#define COM_QPOINT "--qpoint"
#define COM_FEATURES "--features"
#define COM_QUERYKEY "--key"
#define COM_KEYLIST "--keyList"
#define COM_TIMES "--times"

#define O2_OLD_MAGIC ('O'|'2'<<8|'D'<<16|'B'<<24)
#define O2_MAGIC ('o'|'2'<<8|'d'<<16|'b'<<24)
#define O2_FORMAT_VERSION (0U)

#define O2_DEFAULT_POINTNN (10U)
#define O2_DEFAULT_TRACKNN  (10U)

#define O2_DEFAULTDBSIZE (2000000000) // 2GB table size
//#define O2_DEFAULTDBSIZE (1000000000U) // 1GB table size

//#define O2_MAXFILES (1000000)
#define O2_MAXFILES (10000U)           // 10,000 files
#define O2_MAXFILESTR (256U)
#define O2_FILETABLESIZE (O2_MAXFILESTR)
#define O2_TRACKTABLESIZE (sizeof(unsigned))
#define O2_HEADERSIZE (sizeof(dbTableHeaderT))
#define O2_MEANNUMVECTORS (1000U)
#define O2_MAXDIM (1000U)
#define O2_MAXNN (10000U)

// Flags
#define O2_FLAG_L2NORM (0x1U)
#define O2_FLAG_MINMAX (0x2U)
#define O2_FLAG_TIMES (0x20U)

// Query types
#define O2_POINT_QUERY (0x4U)
#define O2_SEQUENCE_QUERY (0x8U)
#define O2_TRACK_QUERY (0x10U)

// Error Codes
#define O2_ERR_KEYNOTFOUND (0xFFFFFF00)

// Macros
#define O2_ACTION(a) (strcmp(command,a)==0)

#define ALIGN_UP(x,w) ((x) + ((1<<w)-1) & ~((1<<w)-1))
#define ALIGN_DOWN(x,w) ((x) & ~((1<<w)-1))

using namespace std;

// 64 byte header
typedef struct dbTableHeader{
  unsigned magic;
  unsigned version;
  unsigned numFiles;
  unsigned dim;
  unsigned flags;
  size_t length;
  size_t fileTableOffset;
  size_t trackTableOffset;
  size_t dataOffset;
  size_t l2normTableOffset;
  size_t timesTableOffset;
} dbTableHeaderT, *dbTableHeaderPtr;


class audioDB{
  
 private:
  gengetopt_args_info args_info;
  unsigned dim;
  const char *dbName;
  const char *inFile;
  const char *hostport;
  const char *key;
  const char* trackFileName;
  ifstream *trackFile;
  const char *command;
  const char *timesFileName;
  ifstream *timesFile;

  int dbfid;
  int infid;
  char* db;
  char* indata;
  struct stat statbuf;  
  dbTableHeaderPtr dbH;
  
  char *fileTable;
  unsigned* trackTable;
  double* dataBuf;
  double* inBuf;
  double* l2normTable;
  double* qNorm;
  double* sNorm;
  double* timesTable;  

  // Flags and parameters
  unsigned verbosity;   // how much do we want to know?
  unsigned queryType; // point queries default
  unsigned pointNN;   // how many point NNs ?
  unsigned trackNN;   // how many track NNs ?
  unsigned sequenceLength;
  unsigned sequenceHop;
  unsigned queryPoint;
  unsigned usingQueryPoint;
  unsigned usingTimes;
  unsigned isClient;
  unsigned isServer;
  unsigned port;
  double timesTol;
  double radius;
  
  // Timers
  struct timeval tv1;
  struct timeval tv2;
    
  // private methods
  void error(const char* a, const char* b = "", const char *sysFunc = 0);
  void pointQuery(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);
  void trackPointQuery(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);
  void trackSequenceQueryNN(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);
  void trackSequenceQueryRad(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);

  void initTables(const char* dbName, bool forWrite, const char* inFile);
  void unitNorm(double* X, unsigned d, unsigned n, double* qNorm);
  void unitNormAndInsertL2(double* X, unsigned dim, unsigned n, unsigned append);
  void insertTimeStamps(unsigned n, ifstream* timesFile, double* timesdata);
  unsigned getKeyPos(char* key);
 public:

  audioDB(const unsigned argc, char* const argv[]);
  audioDB(const unsigned argc, char* const argv[], adb__queryResult *adbQueryResult);
  audioDB(const unsigned argc, char* const argv[], adb__statusResult *adbStatusResult);
  void cleanup();
  ~audioDB();
  int processArgs(const unsigned argc, char* const argv[]);
  void get_lock(int fd, bool exclusive);
  void release_lock(int fd);
  void create(const char* dbName);
  void drop();
  void insert(const char* dbName, const char* inFile);
  void batchinsert(const char* dbName, const char* inFile);
  void query(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);
  void status(const char* dbName, adb__statusResult *adbStatusResult=0);
  void ws_status(const char*dbName, char* hostport);
  void ws_query(const char*dbName, const char *trackKey, const char* hostport);
  void l2norm(const char* dbName);
  void dump(const char* dbName);

  // web services
  void startServer();
  
};

#define O2_AUDIODB_INITIALIZERS \
  dim(0), \
  dbName(0), \
  inFile(0), \
  key(0), \
  trackFileName(0), \
  trackFile(0), \
  command(0), \
  timesFileName(0), \
  timesFile(0), \
  dbfid(0), \
  infid(0), \
  db(0), \
  indata(0), \
  dbH(0), \
  fileTable(0), \
  trackTable(0), \
  dataBuf(0), \
  l2normTable(0), \
  qNorm(0), \
  timesTable(0), \
  verbosity(1), \
  queryType(O2_POINT_QUERY), \
  pointNN(O2_DEFAULT_POINTNN), \
  trackNN(O2_DEFAULT_TRACKNN), \
  sequenceLength(16), \
  sequenceHop(1), \
  queryPoint(0), \
  usingQueryPoint(0), \
  usingTimes(0), \
  isClient(0), \
  isServer(0), \
  port(0), \
  timesTol(0.1), \
  radius(0)
