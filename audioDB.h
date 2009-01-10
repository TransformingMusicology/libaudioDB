#ifndef __AUDIODB_H_
#define __AUDIODB_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <string>
#include <math.h>
#include <sys/time.h>
#include <assert.h>
#include <float.h>
#include <signal.h>
#include <gsl/gsl_rng.h>

// includes for LSH indexing
extern "C" {
#include "audioDB_API.h"
}
#include "ReporterBase.h"
#include "accumulator.h"
#include "lshlib.h"

// includes for web services
#include "soapH.h"
#include "cmdline.h"

// should probably be rewritten
class PointPair{
 public:
  Uns32T trackID;
  Uns32T qpos;
  Uns32T spos;
  PointPair(Uns32T a, Uns32T b, Uns32T c);
};
bool operator<(const PointPair& a, const PointPair& b);

#define MAXSTR 512

// Databse PRIMARY commands
#define COM_CREATE "--NEW"
#define COM_INSERT "--INSERT"
#define COM_BATCHINSERT "--BATCHINSERT"
#define COM_QUERY "--QUERY"
#define COM_STATUS "--STATUS"
#define COM_L2NORM "--L2NORM"
#define COM_POWER "--POWER"
#define COM_DUMP "--DUMP"
#define COM_SERVER "--SERVER"
#define COM_INDEX "--INDEX"
#define COM_SAMPLE "--SAMPLE"
#define COM_LISZT "--LISZT"

// parameters
#define COM_CLIENT "--client"
#define COM_DATABASE "--database"
#define COM_QTYPE "--qtype"
#define COM_SEQLEN "--sequencelength"
#define COM_SEQHOP "--sequencehop"
#define COM_POINTNN "--pointnn"
#define COM_RADIUS "--radius"
#define COM_TRACKNN "--resultlength"
#define COM_QPOINT "--qpoint"
#define COM_FEATURES "--features"
#define COM_QUERYKEY "--key"
#define COM_KEYLIST "--keyList"
#define COM_TIMES "--times"
#define COM_QUERYPOWER "--power"
#define COM_RELATIVE_THRESH "--relative-threshold"
#define COM_ABSOLUTE_THRESH "--absolute-threshold"
#define COM_EXHAUSTIVE "--exhaustive"
#define COM_LSH_EXACT "--lsh_exact"
#define COM_NO_UNIT_NORMING "--no_unit_norming"

#define O2_OLD_MAGIC ('O'|'2'<<8|'D'<<16|'B'<<24)
#define O2_MAGIC ('o'|'2'<<8|'d'<<16|'b'<<24)
#define O2_FORMAT_VERSION (4U)

#define O2_DEFAULT_POINTNN (10U)
#define O2_DEFAULT_TRACKNN  (10U)

//#define O2_DEFAULTDBSIZE (4000000000) // 4GB table size
#define O2_DEFAULTDBSIZE (2000000000) // 2GB table size

// Bit masks for packing (trackID,pointID) into 32-bit unsigned int
// This can be controlled at compile time
#define O2_DEFAULT_LSH_N_POINT_BITS 14

// Override the default point bit width for large database support
#ifndef LSH_N_POINT_BITS
#define LSH_N_POINT_BITS O2_DEFAULT_LSH_N_POINT_BITS
#endif

// LIMIT PARAMETERS
#define O2_DEFAULT_DATASIZE (1355U) // in MB
#define O2_DEFAULT_NTRACKS (20000U)
#define O2_DEFAULT_DATADIM (9U)
#define O2_REALTYPE (double)
#define O2_MAXFILES (1000000U)
#define O2_MAXFILESTR (256U)
#define O2_FILETABLE_ENTRY_SIZE (O2_MAXFILESTR)
#define O2_TRACKTABLE_ENTRY_SIZE (sizeof(unsigned))
#define O2_HEADERSIZE (sizeof(dbTableHeaderT))
#define O2_MEANNUMVECTORS (1000U)
#define O2_MAXDIM (20000U)
#define O2_MAXNN (1000000U)
#define O2_MAXSEQLEN (8000U)            // maximum feature vectors in a sequence
#define O2_MAXTRACKS (1000000U)           // maximum number of tracks
#define O2_MAXTRACKLEN (1<<LSH_N_POINT_BITS) // maximum shingles in a track
#define O2_MAXDOTPRODUCTMEMORY (sizeof(O2_REALTYPE)*O2_MAXSEQLEN*O2_MAXSEQLEN) // 512MB
#define O2_DISTANCE_TOLERANCE (1e-6)
#define O2_SERIAL_MAX_TRACKBATCH (1000000)
#define O2_LARGE_ADB_SIZE (O2_DEFAULT_DATASIZE+1) // datasize at which features are kept externally (in Mbytes)
#define O2_LARGE_ADB_NTRACKS (O2_DEFAULT_NTRACKS+1) // ntracks at which features are kept externally
#define O2_MAX_VECTORS ( O2_MEANNUMVECTORS * O2_MAXTRACKS )

// Flags
#define O2_FLAG_L2NORM (0x1U)
#define O2_FLAG_MINMAX (0x2U)
#define O2_FLAG_POWER (0x4U)
#define O2_FLAG_TIMES (0x20U)
#define O2_FLAG_LARGE_ADB (0x40U)
#define DISPLAY_FLAG(x) (x?"on":"off")

// Query types
#define O2_POINT_QUERY (0x4U)
#define O2_SEQUENCE_QUERY (0x8U)
#define O2_TRACK_QUERY (0x10U)
#define O2_N_SEQUENCE_QUERY (0x20U)
#define O2_ONE_TO_ONE_N_SEQUENCE_QUERY (0x40U)

// Error Codes
#define O2_ERR_KEYNOTFOUND (0xFFFFFF00)

// Macros
#define O2_ACTION(a) (strcmp(command,a)==0)

#define ALIGN_UP(x,w) (((x) + ((1<<w)-1)) & ~((1<<w)-1))
#define ALIGN_DOWN(x,w) ((x) & ~((1<<w)-1))

#define ALIGN_PAGE_UP(x) (((x) + (getpagesize()-1)) & ~(getpagesize()-1))
#define ALIGN_PAGE_DOWN(x) ((x) & ~(getpagesize()-1))

#define ENSURE_STRING(x) ((x) ? (x) : "")

#define CHECKED_MMAP(type, var, start, length) \
  { void *tmp = mmap(0, length, (PROT_READ | (forWrite ? PROT_WRITE : 0)), MAP_SHARED, dbfid, (start)); \
    if(tmp == (void *) -1) { \
      error("mmap error for db table", #var, "mmap"); \
    } \
    var = (type) tmp; \
  }

#define CHECKED_READ(fd, buf, count) \
  { size_t tmpcount = count; \
    ssize_t tmp = read(fd, buf, tmpcount); \
    if(tmp == -1) { \
      error("read error", "", "read"); \
    } else if((size_t) tmp != tmpcount) {	\
      error("short read", ""); \
    } \
  }

#define CHECKED_WRITE(fd, buf, count) \
  { size_t tmpcount = count; \
    ssize_t tmp = write(fd, buf, tmpcount); \
    if(tmp == -1) { \
      error("write error", "", "write"); \
    } else if((size_t) tmp != tmpcount) {	\
      error("short write", ""); \
    } \
  }

#define VERB_LOG(vv, ...) \
  if(verbosity > vv) { \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr); \
  }

// We will only use this in a 32-bit address space
// So map the off_t down to 32-bits first
#define INSERT_FILETABLE_STRING(TABLE, STR) \
    strncpy(TABLE + dbH->numFiles*O2_FILETABLE_ENTRY_SIZE, STR, strlen(STR));

#define SAFE_DELETE(PTR) delete PTR; PTR=0;
#define SAFE_DELETE_ARRAY(PTR) delete[] PTR; PTR=0;

extern char* SERVER_ADB_ROOT;
extern char* SERVER_ADB_FEATURE_ROOT;

typedef struct dbTableHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t numFiles;
  uint32_t dim;
  uint32_t flags;
  uint32_t headerSize;
  off_t length;
  off_t fileTableOffset;
  off_t trackTableOffset;
  off_t dataOffset;
  off_t l2normTableOffset;
  off_t timesTableOffset;
  off_t powerTableOffset;
  off_t dbSize;
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
  std::ifstream *trackFile;
  const char *command;
  const char *output;
  const char *timesFileName;
  std::ifstream *timesFile;
  const char *powerFileName;
  std::ifstream *powerFile;
  const char* adb_root;
  const char* adb_feature_root;

  int powerfd;
  int dbfid;
  int lshfid;
  bool forWrite;
  int infid;
  struct stat statbuf;  
  dbTableHeaderPtr dbH;
  struct adb *adb;

  gsl_rng *rng;
  
  char* fileTable;
  unsigned* trackTable;
  double* l2normTable;
  double* timesTable;
  double* powerTable;

  char* featureFileNameTable;
  char* timesFileNameTable;
  char* powerFileNameTable;

  size_t fileTableLength;
  size_t trackTableLength;
  size_t timesTableLength;
  size_t powerTableLength;
  size_t l2normTableLength;

  // Flags and parameters
  unsigned verbosity;   // how much do we want to know?

  unsigned nsamples;

  //off_t size; // given size (for creation)
  unsigned datasize; // size in MB
  unsigned ntracks;
  unsigned datadim;

  unsigned queryType; // point queries default
  unsigned pointNN;   // how many point NNs ?
  unsigned trackNN;   // how many track NNs ?
  unsigned sequenceLength;
  unsigned sequenceHop;
  bool normalizedDistance;
  bool no_unit_norming;
  unsigned queryPoint;
  unsigned usingQueryPoint;
  unsigned usingTimes;
  unsigned usingPower;
  unsigned isClient;
  unsigned isServer;
  unsigned port;
  double timesTol;
  double radius;
  bool query_from_key;
  Uns32T query_from_key_index;
  bool use_absolute_threshold;
  double absolute_threshold;
  bool use_relative_threshold;
  double relative_threshold;
  
  ReporterBase* reporter;  // track/point reporter
  set<Uns32T> * allowed_keys;    // search restrict list by key

  // LISZT parameters
  unsigned lisztOffset;
  unsigned lisztLength;

  // private methods
  void error(const char* a, const char* b = "", const char *sysFunc = 0);

  void insertTimeStamps(unsigned n, std::ifstream* timesFile, double* timesdata);
  void initRNG();
  void initDBHeader(const char *dbName);
  void initInputFile(const char *inFile);
  void initTables(const char* dbName, const char* inFile = 0);
  void initTablesFromKey(const char* dbName, const Uns32T queryIndex);
  void prefix_name(char** const name, const char* prefix);

 public:
  audioDB(const unsigned argc, const char *argv[]);
  audioDB(const unsigned argc, const char *argv[], adb__queryResponse *adbQueryResponse);
  audioDB(const unsigned argc, const char *argv[], adb__statusResponse *adbStatusResponse);
  audioDB(const unsigned argc, const char *argv[], adb__lisztResponse *adbLisztResponse);

  void cleanup();
  ~audioDB();
  int processArgs(const unsigned argc, const char* argv[]);
  void get_lock(int fd, bool exclusive);
  void release_lock(int fd);
  void create(const char* dbName);
  void insert(const char* dbName, const char* inFile);
  void batchinsert(const char* dbName, const char* inFile);
  void query(const char* dbName, const char* inFile, adb__queryResponse *adbQueryResponse=0);
  void status(const char* dbName, adb__statusResponse *adbStatusResponse=0);

  unsigned random_track(unsigned *propTable, unsigned total);
  void sample(const char *dbName);
  void l2norm(const char* dbName);
  void power_flag(const char *dbName);
  void dump(const char* dbName);
  void liszt(const char* dbName, unsigned offset, unsigned numLines, adb__lisztResponse* adbLisztResponse=0);

  // LSH indexing parameters and data structures
  LSH* lsh;
  bool lsh_in_core;     // load LSH tables for query into core (true) or keep on disk (false)
  bool lsh_use_u_functions;
  bool lsh_exact;      // flag to indicate use exact evaluation of points returned by LSH
  bool WS_load_index; // flag to indicate that we want to make a Web Services index memory resident
  double lsh_param_w; // Width of LSH hash-function bins
  Uns32T lsh_param_k; // Number of independent hash functions
  Uns32T lsh_param_m; // Combinatorial parameter for m(m-1)/2 hash tables
  Uns32T lsh_param_N; // Number of rows per hash table
  Uns32T lsh_param_b; // Batch size, in number of tracks, per indexing iteration
  Uns32T lsh_param_ncols; // Maximum number of collision in a hash-table row

  // LSH indexing and retrieval methods  
  void index_index_db(const char* dbName);
  void index_initialize(double**,double**,double**,double**,unsigned int*);
  void index_insert_tracks(Uns32T start_track, Uns32T end_track, double** fvpp, double** sNormpp,double** snPtrp, double** sPowerp, double** spPtrp);
  int index_insert_track(Uns32T trackID, double** fvpp, double** snpp, double** sppp);
  Uns32T index_insert_shingles(vector<vector<float> >*, Uns32T trackID, double* spp);
  void insertPowerData(unsigned n, int powerfd, double *powerdata);
  void init_track_aux_data(Uns32T trackID, double* fvp, double** sNormpp,double** snPtrp, double** sPowerp, double** spPtrp);
  void initialize_allowed_keys(std::ifstream*); // implementation of restrict lists using STL "set" class
  int is_in_allowed_keys(Uns32T trackID); // test method for allowed_keys used during search
  
  // Web Services
  void startServer();

  void ws_status(const char*dbName, char* hostport);
  void ws_query(const char*dbName, const char *featureFileName, const char* hostport);
  void ws_query_by_key(const char*dbName, const char *trackKey, const char* featureFileName, const char* hostport);
  void ws_liszt(const char* dbName, char* hostport);

};

#define O2_AUDIODB_INITIALIZERS			\
  dim(0),					\
    dbName(0),					\
    inFile(0),					\
    key(0),					\
    trackFileName(0),				\
    trackFile(0),				\
    command(0),					\
    output(0),					\
    timesFileName(0),				\
    timesFile(0),				\
    powerFileName(0),				\
    powerFile(0),				\
    adb_root(0),                                \
    adb_feature_root(0),                        \
    powerfd(0),                                 \
    dbfid(0),					\
    lshfid(0),					\
    forWrite(false),				\
    infid(0),					\
    dbH(0),					\
    adb(0),                                     \
    rng(0),                                     \
    fileTable(0),				\
    trackTable(0),				\
    l2normTable(0),				\
    timesTable(0),				\
    powerTable(0),                              \
    featureFileNameTable(0),                    \
    timesFileNameTable(0),                      \
    powerFileNameTable(0),                      \
    fileTableLength(0),				\
    trackTableLength(0),			\
    timesTableLength(0),			\
    powerTableLength(0),			\
    l2normTableLength(0),			\
    verbosity(1),				\
    nsamples(2000),                             \
    datasize(O2_DEFAULT_DATASIZE),              \
    ntracks(O2_DEFAULT_NTRACKS),                \
    datadim(O2_DEFAULT_DATADIM),                \
    queryType(O2_POINT_QUERY),			\
    pointNN(O2_DEFAULT_POINTNN),		\
    trackNN(O2_DEFAULT_TRACKNN),		\
    sequenceLength(16),				\
    sequenceHop(1),				\
    normalizedDistance(true),			\
    no_unit_norming(false),                     \
    queryPoint(0),				\
    usingQueryPoint(0),				\
    usingTimes(0),				\
    usingPower(0),				\
    isClient(0),				\
    isServer(0),				\
    port(0),					\
    timesTol(0.1),				\
    radius(0),					\
    query_from_key(false),                      \
    query_from_key_index((uint32_t) -1),        \
    use_absolute_threshold(false),		\
    absolute_threshold(0.0),			\
    use_relative_threshold(false),		\
    relative_threshold(0.0),			\
    reporter(0),                                \
    allowed_keys(0),                            \
    lisztOffset(0),                             \
    lisztLength(0),                             \
    lsh(0),					\
    lsh_in_core(false),				\
    lsh_use_u_functions(false),                 \
    lsh_exact(false),                           \
    WS_load_index(false),                       \
    lsh_param_k(0),				\
    lsh_param_m(0),				\
    lsh_param_N(0),				\
    lsh_param_b(0),				\
    lsh_param_ncols(0)
#endif
