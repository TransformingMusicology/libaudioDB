#include <set>
#include <queue>
#include <map>
#include <string>
#include <algorithm>

#include "pointpair.h"
#include "accumulator.h"
#include "lshlib.h"

/* this struct is for writing polymorphic routines as puns.  When
 * inserting, we might have a "datum" (with actual numerical data) or
 * a "reference" (with strings denoting pathnames containing numerical
 * data), but most of the operations are the same.  This struct, used
 * only internally, allows us to write the main body of the insert
 * code only once.
 */
typedef struct adb_datum_internal {
  uint32_t nvectors;
  uint32_t dim;
  const char *key;
  void *data;
  void *times;
  void *power;
} adb_datum_internal_t;

/* this struct is to collect together a bunch of information about a
 * query (or, in fact, a single database entry, or even a whole
 * database).  The _data pointers are immutable (hey, FIXME: should
 * they be constified in some way?) so that free() can work on them
 * later, while the ones without the suffix are mutable to maintain
 * the "current" position in some way.  mean_duration points to a
 * (possibly single-element) array of mean durations for each track.
 */
typedef struct adb_qpointers_internal {
  uint32_t nvectors;
  double *l2norm_data;
  double *l2norm;
  double *power_data;
  double *power;
  double *mean_duration;
} adb_qpointers_internal_t;

/* this struct is for maintaining per-query state.  We don't want to
 * store this stuff in the adb struct itself, because (a) it doesn't
 * belong there and (b) in principle people might do two queries in
 * parallel using the same adb handle.  (b) is in practice a little
 * bit academic because at the moment we're seeking all over the disk
 * using adb->fd, but changing to use pread() might win us
 * threadsafety eventually.
 */
typedef struct adb_qstate_internal {
  Accumulator *accumulator;
  std::set<std::string> *allowed_keys;
  std::priority_queue<PointPair> *exact_evaluation_queue;
  LSH *lsh;
} adb_qstate_internal_t;

/* this struct is the in-memory representation of the binary
 * information stored at the head of each adb file */
typedef struct adbheader {
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
} adb_header_t;

#define ADB_HEADER_SIZE (sizeof(struct adbheader))

#define ADB_HEADER_FLAG_L2NORM		(0x1U)
#define ADB_HEADER_FLAG_POWER		(0x4U)
#define ADB_HEADER_FLAG_TIMES		(0x20U)
#define ADB_HEADER_FLAG_REFERENCES	(0x40U)

/* the transparent version of the opaque (forward-declared) adb_t. */
struct adb {
  char *path;
  int fd;
  int flags;
  adb_header_t *header;
  std::vector<std::string> *keys;
  std::map<std::string,uint32_t> *keymap;
  std::vector<uint32_t> *track_lengths;
  std::vector<off_t> *track_offsets;
  LSH *cached_lsh;
};

typedef struct {
  bool operator() (const adb_result_t &r1, const adb_result_t &r2) {
    return strcmp(r1.key, r2.key) < 0;
  }
} adb_result_key_lt;

typedef struct {
  bool operator() (const adb_result_t &r1, const adb_result_t &r2) {
    return r1.qpos < r2.qpos;
  }
} adb_result_qpos_lt;

typedef struct {
  bool operator() (const adb_result_t &r1, const adb_result_t &r2) {
    return r1.dist < r2.dist;
  }
} adb_result_dist_lt;

typedef struct {
  bool operator() (const adb_result_t &r1, const adb_result_t &r2) {
    return r1.dist > r2.dist;
  }
} adb_result_dist_gt;

typedef struct {
  bool operator() (const adb_result_t &r1, const adb_result_t &r2) {
    return ((r1.ipos < r2.ipos) ||
            ((r1.ipos == r2.ipos) && 
             ((r1.qpos < r2.qpos) ||
              ((r1.qpos == r2.qpos) && (strcmp(r1.key, r2.key) < 0)))));
  }
} adb_result_triple_lt;

/* We could go gcc-specific here and use typeof() instead of passing
 * in an explicit type.  Answers on a postcard as to whether that's a
 * good plan or not. */
#define mmap_or_goto_error(type, var, start, length) \
  { void *tmp = mmap(0, length, PROT_READ, MAP_SHARED, adb->fd, (start)); \
    if(tmp == (void *) -1) { \
      goto error; \
    } \
    var = (type) tmp; \
  }

#define maybe_munmap(table, length) \
  { if(table) { \
      munmap(table, length); \
    } \
  }

#define maybe_delete_array(pointer) \
  { if(pointer) { \
      delete [] pointer; \
      pointer = NULL; \
    } \
  }

#define write_or_goto_error(fd, buffer, size) \
  { ssize_t tmp = size; \
    if(write(fd, buffer, size) != tmp) { \
      goto error; \
    } \
  }

#define read_or_goto_error(fd, buffer, size) \
  { ssize_t tmp = size; \
    if(read(fd, buffer, size) != tmp) { \
      goto error; \
    } \
  }

static inline int audiodb_sync_header(adb_t *adb) {
  off_t pos;
  pos = lseek(adb->fd, (off_t) 0, SEEK_CUR);
  if(pos == (off_t) -1) {
    goto error;
  }
  if(lseek(adb->fd, (off_t) 0, SEEK_SET) == (off_t) -1) {
    goto error;
  }
  if(write(adb->fd, adb->header, ADB_HEADER_SIZE) != ADB_HEADER_SIZE) {
    goto error;
  }

  /* can be fsync() if fdatasync() is racily exciting and new */
  fdatasync(adb->fd);
  if(lseek(adb->fd, pos, SEEK_SET) == (off_t) -1) {
    goto error;
  }
  return 0;

 error:
  return 1;
}

static inline double audiodb_dot_product(double *p, double *q, size_t count) {
  double result = 0;
  while(count--) {
    result += *p++ * *q++;
  }
  return result;
}

static inline void audiodb_l2norm_buffer(double *d, size_t dim, size_t nvectors, double *l) {
  while(nvectors--) {
    double *d1 = d;
    double *d2 = d;
    *l++ = audiodb_dot_product(d1, d2, dim);
    d += dim;
  }
}

// This is a common pattern in sequence queries: what we are doing is
// taking a window of length seqlen over a buffer of length length,
// and placing the sum of the elements in that window in the first
// element of the window: thus replacing all but the last seqlen
// elements in the buffer with the corresponding windowed sum.
static inline void audiodb_sequence_sum(double *buffer, int length, int seqlen) {
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
    if(isfinite(tmp1)) {
      *ps = *(ps - 1) - tmp1 + *(ps + seqlen - 1);
    } else {
      for(int i = 1; i < seqlen; i++) {
        *ps += *(ps + i);
      }
    }
    tmp1 = tmp2;
    ps++;
  }
}

// In contrast to audiodb_sequence_sum() above,
// audiodb_sequence_sqrt() and audiodb_sequence_average() below are
// simple mappers across the sequence.
static inline void audiodb_sequence_sqrt(double *buffer, int length, int seqlen) {
  int w = length - seqlen + 1;
  while(w--) {
    *buffer = sqrt(*buffer);
    buffer++;
  }
}

static inline void audiodb_sequence_average(double *buffer, int length, int seqlen) {
  int w = length - seqlen + 1;
  while(w--) {
    *buffer /= seqlen;
    buffer++;
  }
}

static inline uint32_t audiodb_key_index(adb_t *adb, const char *key) {
  std::map<std::string,uint32_t>::iterator it;
  it = adb->keymap->find(key);
  if(it == adb->keymap->end()) {
    return (uint32_t) -1;
  } else {
    return (*it).second;
  }
}

static inline const char *audiodb_index_key(adb_t *adb, uint32_t index) {
  return (*adb->keys)[index].c_str();
}

static inline uint32_t audiodb_index_to_track_id(adb_t *adb, uint32_t lshid) {
  off_t offset = lshid * adb->header->dim * sizeof(double);
  std::vector<off_t>::iterator b = (*adb->track_offsets).begin();
  std::vector<off_t>::iterator e = (*adb->track_offsets).end();  
  std::vector<off_t>::iterator p = std::upper_bound(b, e, offset);
  return p - b - 1;
}

static inline uint32_t audiodb_index_to_track_pos(adb_t *adb, uint32_t track_id, uint32_t lshid) {
  uint32_t trackIndexOffset = (*adb->track_offsets)[track_id] / (adb->header->dim * sizeof(double));
  return lshid - trackIndexOffset;
}

static inline uint32_t audiodb_index_from_trackinfo(adb_t *adb, uint32_t track_id, uint32_t track_pos) {
  uint32_t trackIndexOffset = (*adb->track_offsets)[track_id] / (adb->header->dim * sizeof(double));
  return trackIndexOffset + track_pos;
}

int audiodb_read_data(adb_t *, int, int, double **, size_t *);
int audiodb_insert_create_datum(adb_insert_t *, adb_datum_t *);
int audiodb_track_id_datum(adb_t *, uint32_t, adb_datum_t *);
int audiodb_free_datum(adb_datum_t *);
int audiodb_datum_qpointers(adb_datum_t *, uint32_t, double **, double **, adb_qpointers_internal_t *);
int audiodb_query_spec_qpointers(adb_t *, const adb_query_spec_t *, double **, double **, adb_qpointers_internal_t *);
int audiodb_query_queue_loop(adb_t *, const adb_query_spec_t *, adb_qstate_internal_t *, double *, adb_qpointers_internal_t *);
int audiodb_query_loop(adb_t *, const adb_query_spec_t *, adb_qstate_internal_t *);
char *audiodb_index_get_name(const char *, double, uint32_t);
bool audiodb_index_exists(const char *, double, uint32_t);
int audiodb_index_query_loop(adb_t *, const adb_query_spec_t *, adb_qstate_internal_t *);
LSH *audiodb_index_allocate(adb_t *, char *, bool);
vector<vector<float> > *audiodb_index_initialize_shingles(uint32_t, uint32_t, uint32_t);
void audiodb_index_delete_shingles(vector<vector<float> > *);
void audiodb_index_make_shingle(vector<vector<float> > *, uint32_t, double *, uint32_t, uint32_t);
int audiodb_index_norm_shingles(vector<vector<float> > *, double *, double *, uint32_t, uint32_t, double, bool, bool, float);

#define ADB_MAXSTR (512U)
#define ADB_FILETABLE_ENTRY_SIZE (256U)
#define ADB_TRACKTABLE_ENTRY_SIZE (sizeof(uint32_t))
#define ADB_DISTANCE_TOLERANCE (1e-6)

#define ADB_DEFAULT_DATASIZE (1355U) /* in MB */
#define ADB_DEFAULT_NTRACKS (20000U)
#define ADB_DEFAULT_DATADIM (9U)

#define ADB_FIXME_LARGE_ADB_SIZE (ADB_DEFAULT_DATASIZE+1)
#define ADB_FIXME_LARGE_ADB_NTRACKS (ADB_DEFAULT_NTRACKS+1)

#define ADB_OLD_MAGIC ('O'|'2'<<8|'D'<<16|'B'<<24)
#define ADB_MAGIC ('o'|'2'<<8|'d'<<16|'b'<<24)
#define ADB_FORMAT_VERSION (4U)

#define align_up(x,w) (((x) + ((1<<w)-1)) & ~((1<<w)-1))
#define align_down(x,w) ((x) & ~((1<<w)-1))

#define align_page_up(x) (((x) + (getpagesize()-1)) & ~(getpagesize()-1))
#define align_page_down(x) ((x) & ~(getpagesize()-1))

