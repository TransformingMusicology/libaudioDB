#ifndef AUDIODB_API_H
#define AUDIODB_API_H

#include <stdbool.h>
#include <stdint.h>

/* for API questions contact 
 * Christophe Rhodes c.rhodes@gold.ac.uk
 * Ian Knopke mas01ik@gold.ac.uk, ian.knopke@gmail.com */

/* Temporary workarounds */
int acquire_lock(int, bool);
int divest_lock(int);


/*******************************************************************/
/* Data types for API */

/* FIXME: it might be that "adb_" isn't such a good prefix to use, and
   that we should prefer "audiodb_".  Or else maybe we should be
   calling ourselves libadb? */
typedef struct adb adb_t;

typedef struct adb_datum {
  uint32_t nvectors;
  uint32_t dim;
  const char *key;
  double *data;
  double *power;
  double *times;
} adb_datum_t;

typedef struct adb_reference {
  const char *features;
  const char *power;
  const char *key;
  const char *times;
} adb_reference_t, adb_insert_t;

/* struct for returning status results */
typedef struct adb_status {
    unsigned int numFiles;  
    unsigned int dim;
    unsigned int dudCount;
    unsigned int nullCount;
    unsigned int flags;
    uint64_t length;
    uint64_t data_region_size;
} adb_status_t;

typedef struct adb_result {
  const char *qkey;
  const char *ikey;
  uint32_t qpos;
  uint32_t ipos;
  double dist;
} adb_result_t;

#define ADB_REFINE_INCLUDE_KEYLIST 1
#define ADB_REFINE_EXCLUDE_KEYLIST 2
#define ADB_REFINE_RADIUS 4
#define ADB_REFINE_ABSOLUTE_THRESHOLD 8
#define ADB_REFINE_RELATIVE_THRESHOLD 16
#define ADB_REFINE_DURATION_RATIO 32
#define ADB_REFINE_HOP_SIZE 64

typedef struct adb_keylist {
  uint32_t nkeys;
  const char **keys;
} adb_keylist_t;

typedef struct adb_query_refine {
  uint32_t flags;
  adb_keylist_t include;
  adb_keylist_t exclude;
  double radius;
  double absolute_threshold;
  double relative_threshold;
  double duration_ratio; /* expandfactor */
  uint32_t qhopsize;
  uint32_t ihopsize;
} adb_query_refine_t;

#define ADB_ACCUMULATION_DB 1
#define ADB_ACCUMULATION_PER_TRACK 2
#define ADB_ACCUMULATION_ONE_TO_ONE 3

#define ADB_DISTANCE_DOT_PRODUCT 1
#define ADB_DISTANCE_EUCLIDEAN_NORMED 2
#define ADB_DISTANCE_EUCLIDEAN 3
#define ADB_DISTANCE_KULLBACK_LEIBLER_DIVERGENCE 4

typedef struct adb_query_parameters {
  uint32_t accumulation;
  uint32_t distance;
  uint32_t npoints;
  uint32_t ntracks;
} adb_query_parameters_t;

typedef struct adb_query_results {
  uint32_t nresults;
  adb_result_t *results;
} adb_query_results_t;

#define ADB_QID_FLAG_EXHAUSTIVE 1
#define ADB_QID_FLAG_ALLOW_FALSE_POSITIVES 2

typedef struct adb_queryid {
  adb_datum_t *datum;
  uint32_t sequence_length;
  uint32_t flags;
  uint32_t sequence_start;
} adb_query_id_t;

typedef struct adb_query_spec {
  adb_query_id_t qid;
  adb_query_parameters_t params;
  adb_query_refine_t refine;
} adb_query_spec_t;

typedef struct adb_track_entry {
  uint32_t nvectors;
  const char *key;
} adb_track_entry_t;

typedef struct adb_liszt_results {
  uint32_t nresults;
  adb_track_entry_t *entries;
} adb_liszt_results_t;

/*******************************************************************/
/* Function prototypes for API */

/* open an existing database */
/* returns a struct or NULL on failure */
adb_t *audiodb_open(const char *path, int flags);

/* create a new database */
/* returns a struct or NULL on failure */
adb_t *audiodb_create(const char *path, unsigned datasize, unsigned ntracks, unsigned datadim);

/* close a database */
void audiodb_close(adb_t *adb);

/* You'll need to turn both of these on to do anything useful */
int audiodb_l2norm(adb_t *adb);
int audiodb_power(adb_t *adb);

/* insert functions */
int audiodb_insert_datum(adb_t *, const adb_datum_t *);
int audiodb_insert_reference(adb_t *, const adb_reference_t *);

/* query functions */
adb_query_results_t *audiodb_query_spec(adb_t *, const adb_query_spec_t *);
adb_query_results_t *audiodb_query_spec_given_sofar(adb_t *, const adb_query_spec_t *, const adb_query_results_t *);
int audiodb_query_free_results(adb_t *, const adb_query_spec_t *, adb_query_results_t *);

/* database status */  
int audiodb_status(adb_t *, adb_status_t *status);

/* retrieval of inserted data */
int audiodb_retrieve_datum(adb_t *, const char *, adb_datum_t *);
int audiodb_free_datum(adb_t *, adb_datum_t *);

/* various dump formats */
int audiodb_dump(adb_t *, const char *outputdir);

/* liszt */
adb_liszt_results_t *audiodb_liszt(adb_t *);
int audiodb_liszt_free_results(adb_t *, adb_liszt_results_t *);

/* sample */
adb_query_results_t *audiodb_sample_spec(adb_t *, const adb_query_spec_t *);

/* backwards compatibility */
int audiodb_insert(adb_t *, adb_insert_t *ins);
int audiodb_batchinsert(adb_t *, adb_insert_t *ins, unsigned int size);

#endif
