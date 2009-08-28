extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"
#include "lshlib.h"

/*
 * Routines and datastructures which are specific to indexed queries.
 */
typedef struct adb_qcallback {
  adb_t *adb;
  adb_qstate_internal_t *qstate;
} adb_qcallback_t;

// return true if indexed query performed else return false
int audiodb_index_init_query(adb_t *adb, const adb_query_spec_t *spec, adb_qstate_internal_t *qstate, bool corep) {

  uint32_t sequence_length = spec->qid.sequence_length;
  double radius = spec->refine.radius;
  if(!(audiodb_index_exists(adb->path, radius, sequence_length)))
    return false;

  char *indexName = audiodb_index_get_name(adb->path, radius, sequence_length);
  if(!indexName) {
    return false;
  }

  qstate->lsh = audiodb_index_allocate(adb, indexName, corep);

  /* FIXME: it would be nice if the LSH library didn't make me do
   * this. */
  if((!corep) && (qstate->lsh->get_lshHeader()->flags & O2_SERIAL_FILEFORMAT2)) {
    delete qstate->lsh;
    qstate->lsh = audiodb_index_allocate(adb, indexName, true);
#ifdef LSH_DUMP_CORE_TABLES
    qstate->lsh->dump_hashtables();
#endif
  }

  delete[] indexName;
  return true;
}

void audiodb_index_add_point_approximate(void *user_data, uint32_t pointID, uint32_t qpos, float dist) {
  adb_qcallback_t *data = (adb_qcallback_t *) user_data;
  adb_t *adb = data->adb;
  adb_qstate_internal_t *qstate = data->qstate;
  uint32_t trackID = audiodb_index_to_track_id(adb, pointID);
  uint32_t spos = audiodb_index_to_track_pos(adb, trackID, pointID);
  std::set<std::string>::iterator keys_end = qstate->allowed_keys->end();
  if(qstate->allowed_keys->find((*adb->keys)[trackID]) != keys_end) {
    adb_result_t r;
    r.key = (*adb->keys)[trackID].c_str();
    r.dist = dist;
    r.qpos = qpos;
    r.ipos = spos;
    if(qstate->set->find(r) == qstate->set->end()) {
      qstate->set->insert(r);
      qstate->accumulator->add_point(&r);
    }
  }
}

// Maintain a queue of points to pass to audiodb_query_queue_loop()
// for exact evaluation
void audiodb_index_add_point_exact(void *user_data, uint32_t pointID, uint32_t qpos, float dist) {
  adb_qcallback_t *data = (adb_qcallback_t *) user_data;
  adb_t *adb = data->adb;
  adb_qstate_internal_t *qstate = data->qstate;
  uint32_t trackID = audiodb_index_to_track_id(adb, pointID);
  uint32_t spos = audiodb_index_to_track_pos(adb, trackID, pointID);
  std::set<std::string>::iterator keys_end = qstate->allowed_keys->end();
  if(qstate->allowed_keys->find((*adb->keys)[trackID]) != keys_end) {
    PointPair p(trackID, qpos, spos);
    qstate->exact_evaluation_queue->push(p);
  }
}

// return -1 on error
// return 0: if index does not exist
// return nqv: if index exists
int audiodb_index_query_loop(adb_t *adb, const adb_query_spec_t *spec, adb_qstate_internal_t *qstate) {
  if(adb->header->flags>>28)
    cerr << "WARNING: Database created using deprecated LSH_N_POINT_BITS coding: REBUILD INDEXES..." << endl;

  double *query = 0, *query_data = 0;
  adb_qpointers_internal_t qpointers = {0};
  
  adb_qcallback_t callback_data;
  callback_data.adb = adb;
  callback_data.qstate = qstate;

  void (*add_point_func)(void *, uint32_t, uint32_t, float);

  uint32_t sequence_length = spec->qid.sequence_length;
  bool normalized = (spec->params.distance == ADB_DISTANCE_EUCLIDEAN_NORMED);
  double radius = spec->refine.radius;
  bool use_absolute_threshold = spec->refine.flags & ADB_REFINE_ABSOLUTE_THRESHOLD;
  double absolute_threshold = spec->refine.absolute_threshold;

  qstate->set = new std::set< adb_result_t, adb_result_triple_lt >;

  if(spec->qid.flags & ADB_QID_FLAG_ALLOW_FALSE_POSITIVES) {
    add_point_func = &audiodb_index_add_point_approximate;  
  } else {
    qstate->exact_evaluation_queue = new std::priority_queue<PointPair>;
    add_point_func = &audiodb_index_add_point_exact;
  }

  /* FIXME: this hardwired lsh_in_core is here to allow for a
   * transition period while the need for the argument is worked
   * through.  Hopefully it will disappear again eventually. */
  bool lsh_in_core = true;

  if(!audiodb_index_init_query(adb, spec, qstate, lsh_in_core)) {
    return 0;
  }

  char *database = audiodb_index_get_name(adb->path, radius, sequence_length);
  if(!database) {
    return -1;
  }

  if(audiodb_query_spec_qpointers(adb, spec, &query_data, &query, &qpointers)) {
    delete [] database;
    return -1;
  }

  uint32_t Nq = qpointers.nvectors - sequence_length + 1;
  std::vector<std::vector<float> > *vv = audiodb_index_initialize_shingles(Nq, adb->header->dim, sequence_length);

  // Construct shingles from query features  
  for(uint32_t pointID = 0; pointID < Nq; pointID++) {
    audiodb_index_make_shingle(vv, pointID, query, adb->header->dim, sequence_length);
  }
  
  // Normalize query vectors
  int vcount = audiodb_index_norm_shingles(vv, qpointers.l2norm, qpointers.power, adb->header->dim, sequence_length, radius, normalized, use_absolute_threshold, absolute_threshold);
  if(vcount == -1) {
    audiodb_index_delete_shingles(vv);
    delete [] database;
    return -1;
  }
  uint32_t numVecsAboveThreshold = vcount;

  // Nq contains number of inspected points in query file, 
  // numVecsAboveThreshold is number of points with power >= absolute_threshold
  double *qpp = qpointers.power; // Keep original qpPtr for possible exact evaluation
  if(!(spec->qid.flags & ADB_QID_FLAG_EXHAUSTIVE) && numVecsAboveThreshold) {
    if((qstate->lsh->get_lshHeader()->flags & O2_SERIAL_FILEFORMAT2) || lsh_in_core) {
      qstate->lsh->retrieve_point((*vv)[0], spec->qid.sequence_start, add_point_func, &callback_data);
    } else {
      qstate->lsh->serial_retrieve_point(database, (*vv)[0], spec->qid.sequence_start, add_point_func, &callback_data);
    }
  } else if(numVecsAboveThreshold) {
    for(uint32_t pointID = 0; pointID < Nq; pointID++) {
      if(!use_absolute_threshold || (use_absolute_threshold && (*qpp++ >= absolute_threshold))) {
	if((qstate->lsh->get_lshHeader()->flags & O2_SERIAL_FILEFORMAT2) || lsh_in_core) {
	  qstate->lsh->retrieve_point((*vv)[pointID], pointID, add_point_func, &callback_data);
        } else {
	  qstate->lsh->serial_retrieve_point(database, (*vv)[pointID], pointID, add_point_func, &callback_data);   
        }
      }
    }
  }
  audiodb_index_delete_shingles(vv);

  if(!(spec->qid.flags & ADB_QID_FLAG_ALLOW_FALSE_POSITIVES)) {
    audiodb_query_queue_loop(adb, spec, qstate, query, &qpointers);
  }

  delete qstate->set;

  
 // Clean up
  if(query_data)
    delete[] query_data;
  if(qpointers.l2norm_data)
    delete[] qpointers.l2norm_data;
  if(qpointers.power_data)
    delete[] qpointers.power_data;
  if(qpointers.mean_duration)
    delete[] qpointers.mean_duration;
  if(database)
    delete[] database;
  if(qstate->lsh != adb->cached_lsh)
    delete qstate->lsh;

  return Nq;
}
