/* audioDB.h 

audioDB version 1.0

An efficient feature-vector database management system (FVDBMS) for 
content-based multimedia search and retrieval.

Usage: audioDB [OPTIONS]...

      --full-help              Print help, including hidden options, and exit
  -V, --version                Print version and exit
  -H, --help                   print help on audioDB usage and exit.

Database Setup:
  These commands require a database argument.
  -d, --database=filename      database name to be used with database commands
  -N, --new                    make a new database
  -S, --status                 database information
  -D, --dump                   list all segments: index key size

Database Insertion:
  The following commands process a binary input feature file and optional 
  associated key.
  -I, --insert                 add feature vectors to an existing database
  -f, --features=filename      binary series of vectors file
  -t, --times=filename         list of time points (ascii) for feature vectors
  -k, --key=identifier         unique identifier associated with features

Batch Commands:
  These batch commands require a list of feature vector filenames in a text 
  file and optional list of keys in a text file.
  -B, --batchinsert            add feature vectors named in a featureList file 
                                 (with optional keys in a keyList file) to the 
                                 named database
  -F, --featureList=filename   text file containing list of binary feature 
                                 vector files to process
  -T, --timesList=filename     text file containing list of ascii time-point 
                                 files for each feature vector file named in 
                                 featureList
  -K, --keyList=filename       text file containing list of unique identifiers 
                                 to associate with list of feature files

Database Search:
  Thse commands control the behaviour of retrieval from a named database.
  -Q, --query                  perform a content-based search on the named 
                                 database using the named feature vector file 
                                 as a query
  -q, --qtype=type             the type of search  (possible values="point", 
                                 "segment", "sequence" default=`sequence')
  -p, --qpoint=position        ordinal position of query vector (or start of 
                                 sequence) in feature vector input file  
                                 (default=`0')
  -n, --pointnn=numpoints      number of point nearest neighbours to use [per 
                                 segment in segment and sequence mode]  
                                 (default=`10')
  -r, --resultlength=length    maximum length of the result list  
                                 (default=`10')
  -l, --sequencelength=length  length of sequences for sequence search  
                                 (default=`16')
  -h, --sequencehop=hop        hop size of sequence window for sequence search  
                                 (default=`1')

Web Services:
  These commands enable the database process to establish a connection via the 
  internet and operate as separate client and server processes.
  -s, --server=port            run as standalone web service on named port  
                                 (default=`80011')
  -c, --client=hostname:port   run as a client using named host service

*/


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
#define COM_SEGNN "--resultlength"
#define COM_QPOINT "--qpoint"
#define COM_FEATURES "--features"
#define COM_QUERYKEY "--key"
#define COM_KEYLIST "--keyList"
#define COM_TIMES "--times"

#define O2_MAGIC 1111765583 // 'B'<<24|'D'<<16|'2'<<8|'O' reads O2DB in little endian order

#define O2_DEFAULT_POINTNN (10U)
#define O2_DEFAULT_SEGNN  (10U)

//#define O2_DEFAULTDBSIZE (2000000000) // 2GB table size
#define O2_DEFAULTDBSIZE (1000000000U) // 1GB table size

//#define O2_MAXFILES (1000000)
#define O2_MAXFILES (10000U)           // 10,000 files
#define O2_MAXFILESTR (256U)
#define O2_FILETABLESIZE (O2_MAXFILESTR)
#define O2_SEGTABLESIZE (sizeof(unsigned))
#define O2_HEADERSIZE (sizeof(dbTableHeaderT))
#define O2_MEANNUMVECTORS (1000U)
#define O2_MAXDIM (1000U)
#define O2_MAXNN (1000U)

// Flags
#define O2_FLAG_L2NORM (0x1U)
#define O2_FLAG_MINMAX (0x2U)
#define O2_FLAG_POINT_QUERY (0x4U)
#define O2_FLAG_SEQUENCE_QUERY (0x8U)
#define O2_FLAG_SEG_QUERY (0x10U)
#define O2_FLAG_TIMES (0x20U)

// Error Codes
#define O2_ERR_KEYNOTFOUND (0xFFFFFF00)

// Macros
#define O2_ACTION(a) (strcmp(command,a)==0)

using namespace std;

// 64 byte header
typedef struct dbTableHeader{
  unsigned magic;
  unsigned numFiles;
  unsigned dim;
  unsigned length;
  unsigned flags;
} dbTableHeaderT, *dbTableHeaderPtr;


class audioDB{
  
 private:
  gengetopt_args_info args_info;
  unsigned dim;
  const char *dbName;
  const char *inFile;
  const char *hostport;
  const char *key;
  const char* segFileName;
  ifstream *segFile;
  const char *command;
  const char *timesFileName;
  ifstream *timesFile;

  int dbfid;
  int infid;
  char* db;
  char* indata;
  struct stat statbuf;  
  dbTableHeaderPtr dbH;
  size_t fileTableOffset;
  size_t segTableOffset;
  size_t dataoffset;
  size_t l2normTableOffset;
  size_t timesTableOffset;
  
  char *fileTable;
  unsigned* segTable;
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
  unsigned segNN;   // how many seg NNs ?
  unsigned sequenceLength;
  unsigned sequenceHop;
  unsigned queryPoint;
  unsigned usingQueryPoint;
  unsigned usingTimes;
  unsigned isClient;
  unsigned isServer;
  unsigned port;
  double timesTol;

  // Timers
  struct timeval tv1;
  struct timeval tv2;
    

  

  // private methods
  void error(const char* a, const char* b = "");
  void pointQuery(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);
  void sequenceQuery(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);
  void segPointQuery(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);
  void segSequenceQuery(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);

  void initTables(const char* dbName, const char* inFile);
  void NBestMatchedFilter();
  void unitNorm(double* X, unsigned d, unsigned n, double* qNorm);
  void unitNormAndInsertL2(double* X, unsigned dim, unsigned n, unsigned append);
  void normalize(double* X, int dim, int n);
  void normalize(double* X, int dim, int n, double minval, double maxval);
  void insertTimeStamps(unsigned n, ifstream* timesFile, double* timesdata);
  unsigned getKeyPos(char* key);
 public:

  audioDB(const unsigned argc, char* const argv[], adb__queryResult *adbQueryResult=0);
  ~audioDB();
  int processArgs(const unsigned argc, char* const argv[]);
  void create(const char* dbName);
  void drop();
  void insert(const char* dbName, const char* inFile);
  void batchinsert(const char* dbName, const char* inFile);
  void query(const char* dbName, const char* inFile, adb__queryResult *adbQueryResult=0);
  void status(const char* dbName);
  void ws_status(const char*dbName, char* hostport);
  void ws_query(const char*dbName, const char *segKey, const char* hostport);
  void l2norm(const char* dbName);
  void dump(const char* dbName);
  void deleteDB(const char* dbName, const char* inFile);

  // web services
  void startServer();
  
};
