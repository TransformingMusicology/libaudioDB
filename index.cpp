// LSH indexing
//
// Construct a persistent LSH table structure
// Store at the same location as dbName
// Naming convention:
//         dbName.lsh.${radius}.${sequenceLength}
//
//
// Author: Michael Casey
//   Date: 23 June 2008
//
// 19th August 2008 - added O2_FLAG_LARGE_ADB support

#include "audioDB.h"
#include "audioDB-internals.h"

typedef struct adb_qcallback {
  adb_t *adb;
  adb_qstate_internal_t *qstate;
} adb_qcallback_t;

/************************* LSH indexing and query initialization  *****************/

/* FIXME: there are several things wrong with this: the memory
 * discipline isn't ideal, the radius printing is a bit lame, the name
 * getting will succeed or fail depending on whether the path was
 * relative or absolute -- but most importantly encoding all that
 * information in a filename is going to lose: it's impossible to
 * maintain backwards-compatibility.  Instead we should probably store
 * the index metadata inside the audiodb instance. */
char *audiodb_index_get_name(const char *dbName, double radius, Uns32T sequenceLength) {
  char *indexName;
  if(strlen(dbName) > (MAXSTR - 32)) {
    return NULL;
  }
  indexName = new char[MAXSTR];  
  strncpy(indexName, dbName, MAXSTR);
  sprintf(indexName+strlen(dbName), ".lsh.%019.9f.%d", radius, sequenceLength);
  return indexName;
}

bool audiodb_index_exists(const char *dbName, double radius, Uns32T sequenceLength) {
  char *indexName = audiodb_index_get_name(dbName, radius, sequenceLength);
  if(!indexName) {
    return false;
  }
  struct stat st;
  if(stat(indexName, &st)) {
    delete [] indexName;
    return false;
  }
  /* FIXME: other stat checks here? */
  /* FIXME: is there any better way to check whether we can open a
   * file for reading than by opening a file for reading? */
  int fd = open(indexName, O_RDONLY);
  delete [] indexName;
  if(fd < 0) {
    return false;
  } else {
    close(fd);
    return true;
  }
}

/* FIXME: the indexName arg should be "const char *", but the LSH
 * library doesn't like that.
 */
LSH *audiodb_index_allocate(adb_t *adb, char *indexName, bool load_tables) {
  LSH *lsh;
  if(adb->cached_lsh) {
    if(!strncmp(adb->cached_lsh->get_indexName(), indexName, MAXSTR)) {
      return adb->cached_lsh;
    } else {
      delete adb->cached_lsh;
    }
  }
  lsh = new LSH(indexName, load_tables);
  if(load_tables) {
    adb->cached_lsh = lsh;
  } 
  return lsh;
}

vector<vector<float> > *audiodb_index_initialize_shingles(Uns32T sz, Uns32T dim, Uns32T seqLen) {
  std::vector<std::vector<float> > *vv = new vector<vector<float> >(sz);
  for(Uns32T i=0 ; i < sz ; i++) {
    (*vv)[i]=vector<float>(dim * seqLen);
  }
  return vv;
}

void audiodb_index_delete_shingles(vector<vector<float> > *vv) {
  delete vv;
}

/********************  LSH indexing audioDB database access forall s \in {S} ***********************/

// Prepare the AudioDB database for read access and allocate auxillary memory
void audioDB::index_initialize(double **snp, double **vsnp, double **spp, double **vspp, Uns32T *dvp) {
  if (!(dbH->flags & O2_FLAG_POWER)) {
    error("INDEXed database must be power-enabled", dbName);
  }

  double *snpp = 0, *sppp = 0;

  *dvp = dbH->length / (dbH->dim * sizeof(double)); // number of database vectors
  *snp = new double[*dvp];  // songs norm pointer: L2 norm table for each vector
  snpp = *snp;
  *spp = new double[*dvp]; // song powertable pointer
  sppp = *spp;

  memcpy(*snp, l2normTable, *dvp * sizeof(double));
  memcpy(*spp, powerTable, *dvp * sizeof(double));
  
  
  for(Uns32T i = 0; i < dbH->numFiles; i++){
    if(trackTable[i] >= sequenceLength) {
      audiodb_sequence_sum(snpp, trackTable[i], sequenceLength);
      audiodb_sequence_sqrt(snpp, trackTable[i], sequenceLength);
      
      audiodb_sequence_sum(sppp, trackTable[i], sequenceLength);
      audiodb_sequence_average(sppp, trackTable[i], sequenceLength);
    }
    snpp += trackTable[i];
    sppp += trackTable[i];
  }
  
  *vsnp = *snp;
  *vspp = *spp;
  
  // Move the feature vector read pointer to start of fetures in database
  lseek(dbfid, dbH->dataOffset, SEEK_SET);
}


/********************* LSH shingle construction ***************************/

// Construct shingles out of a feature matrix
// inputs:
// idx is vector index in feature matrix
// fvp is base feature matrix pointer double* [numVecs x dbH->dim]
//
// pre-conditions: 
// dbH->dim 
// sequenceLength
// idx < numVectors - sequenceLength + 1
//
// post-conditions:
// (*vv)[idx] contains a shingle with dbH->dim*sequenceLength float values

static void audiodb_index_make_shingle(vector<vector<float> >* vv, Uns32T idx, double* fvp, Uns32T dim, Uns32T seqLen){
  assert(idx<(*vv).size());
  vector<float>::iterator ve = (*vv)[idx].end();
  vector<float>::iterator vi = (*vv)[idx].begin();
  // First feature vector in shingle
  if(idx == 0) {
    while(vi!=ve) {
      *vi++ = (float)(*fvp++);
    }
  } else {
    // Not first feature vector in shingle
    vector<float>::iterator ui=(*vv)[idx-1].begin() + dim;
    // Previous seqLen-1 dim-vectors
    while(vi!=ve-dim) {
      *vi++ = *ui++;
    }
    // Move data pointer to next feature vector
    fvp += ( seqLen + idx - 1 ) * dim ;
    // New d-vector
    while(vi!=ve) {
      *vi++ = (float)(*fvp++);
    }
  }
}

// norm shingles
// in-place norming, no deletions
// If using power, return number of shingles above power threshold
int audiodb_index_norm_shingles(vector<vector<float> >* vv, double* snp, double* spp, Uns32T dim, Uns32T seqLen, double radius, bool normed_vectors, bool use_pthreshold, float pthreshold) {
  int z = 0; // number of above-threshold shingles
  float l2norm;
  double power;
  float oneOverRadius = 1./(float)sqrt(radius); // Passed radius is really radius^2
  float oneOverSqrtl2NormDivRad = oneOverRadius;
  Uns32T shingleSize = seqLen * dim;

  if(!spp) {
    return -1;
  }
  for(Uns32T a=0; a<(*vv).size(); a++){
    l2norm = (float)(*snp++);
    if(normed_vectors)
      oneOverSqrtl2NormDivRad = (1./l2norm)*oneOverRadius;
    
    for(Uns32T b=0; b < shingleSize ; b++)
      (*vv)[a][b]*=oneOverSqrtl2NormDivRad;

    power = *spp++;
    if(use_pthreshold){
      if (power >= pthreshold)
	z++;
    }
    else
      z++;	
  }
  return z;
}
  

/************************ LSH indexing ***********************************/
void audioDB::index_index_db(const char* dbName){
  char* newIndexName;
  double *fvp = 0, *sNorm = 0, *snPtr = 0, *sPower = 0, *spPtr = 0;
  Uns32T dbVectors = 0;


  printf("INDEX: initializing header\n");
  // Check if audioDB exists, initialize header and open database for read
  forWrite = false;
  initDBHeader(dbName);

  if(dbH->flags & O2_FLAG_POWER)
    usingPower = true;
  
  if(dbH->flags & O2_FLAG_TIMES)
    usingTimes = true;

  newIndexName = audiodb_index_get_name(dbName, radius, sequenceLength);
  if(!newIndexName) {
    error("failed to get index name", dbName);
  }

  // Set unit norming flag override
  audioDB::normalizedDistance = !audioDB::no_unit_norming;

  VERB_LOG(1, "INDEX: dim %d\n", (int)dbH->dim);
  VERB_LOG(1, "INDEX: R %f\n", radius);
  VERB_LOG(1, "INDEX: seqlen %d\n", sequenceLength);
  VERB_LOG(1, "INDEX: lsh_w %f\n", lsh_param_w);
  VERB_LOG(1, "INDEX: lsh_k %d\n", lsh_param_k);
  VERB_LOG(1, "INDEX: lsh_m %d\n", lsh_param_m);
  VERB_LOG(1, "INDEX: lsh_N %d\n", lsh_param_N);
  VERB_LOG(1, "INDEX: lsh_C %d\n", lsh_param_ncols);
  VERB_LOG(1, "INDEX: lsh_b %d\n", lsh_param_b);
  VERB_LOG(1, "INDEX: normalized? %s\n", normalizedDistance?"true":"false"); 

  if((lshfid = open(newIndexName,O_RDONLY))<0){
    printf("INDEX: constructing new LSH index\n");  
    printf("INDEX: making index file %s\n", newIndexName);
    fflush(stdout);
    // Construct new LSH index
    lsh = new LSH((float)lsh_param_w, lsh_param_k,
		  lsh_param_m,
		  (Uns32T)(sequenceLength*dbH->dim),
		  lsh_param_N,
		  lsh_param_ncols,
		  (float)radius);
    assert(lsh);  

    Uns32T endTrack = lsh_param_b;
    if( endTrack > dbH->numFiles)
      endTrack = dbH->numFiles;
    // Insert up to lsh_param_b tracks
    if( ! (dbH->flags & O2_FLAG_LARGE_ADB) ){
      index_initialize(&sNorm, &snPtr, &sPower, &spPtr, &dbVectors);  
    }
    index_insert_tracks(0, endTrack, &fvp, &sNorm, &snPtr, &sPower, &spPtr);
    lsh->serialize(newIndexName, lsh_in_core?O2_SERIAL_FILEFORMAT2:O2_SERIAL_FILEFORMAT1);
    
    // Clean up
    delete lsh;
    lsh = 0;
  } else {
    close(lshfid);
  }
  
  // Attempt to open LSH file
  if((lshfid = open(newIndexName,O_RDONLY))>0){
    printf("INDEX: merging with existing LSH index\n");
    fflush(stdout);
    char* mergeIndexName = newIndexName;

    // Get the lsh header info and find how many tracks are inserted already
    lsh = new LSH(mergeIndexName, false); // lshInCore=false to avoid loading hashTables here
    assert(lsh);
    Uns32T maxs = audiodb_index_to_track_id(lsh->get_maxp(), audiodb_lsh_n_point_bits(adb))+1;
    delete lsh;
    lsh = 0;

    // Insert up to lsh_param_b tracks
    if(  !sNorm && !(dbH->flags & O2_FLAG_LARGE_ADB) ){
      index_initialize(&sNorm, &snPtr, &sPower, &spPtr, &dbVectors);  
    }
    // This allows for updating index after more tracks are inserted into audioDB
    for(Uns32T startTrack = maxs; startTrack < dbH->numFiles; startTrack+=lsh_param_b){

      Uns32T endTrack = startTrack + lsh_param_b;
      if( endTrack > dbH->numFiles)
	endTrack = dbH->numFiles;
      printf("Indexing track range: %d - %d\n", startTrack, endTrack);
      fflush(stdout);
      lsh = new LSH(mergeIndexName, false); // Initialize empty LSH tables
      assert(lsh);
      
      // Insert up to lsh_param_b database tracks
      index_insert_tracks(startTrack, endTrack, &fvp, &sNorm, &snPtr, &sPower, &spPtr);

      // Serialize to file (merging is performed here)
      lsh->serialize(mergeIndexName, lsh_in_core?O2_SERIAL_FILEFORMAT2:O2_SERIAL_FILEFORMAT1); // Serialize core LSH heap to disk
      delete lsh;
      lsh = 0;
    }
    
    close(lshfid);    
    printf("INDEX: done constructing LSH index.\n");  
    fflush(stdout);
    
  }
  else{
    error("Something's wrong with LSH index file");
    exit(1);
  }
    
  delete[] newIndexName;
  delete[] sNorm;
  delete[] sPower;
}


void audioDB::insertPowerData(unsigned numVectors, int powerfd, double *powerdata) {
  if(usingPower){
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

// initialize auxillary track data from filesystem
// pre-conditions:
// dbH->flags & O2_FLAG_LARGE_ADB
// feature data allocated and copied (fvp)
//
// post-conditions:
// allocated power data
// allocated l2norm data
//
void audioDB::init_track_aux_data(Uns32T trackID, double* fvp, double** sNormpp,double** snPtrp, double** sPowerp, double** spPtrp){  
  if( !(dbH->flags & O2_FLAG_LARGE_ADB) )
    error("error: init_track_large_adb required O2_FLAG_LARGE_ADB");

  // Allocate and read the power sequence
  if(trackTable[trackID]>=sequenceLength){
    
    char* prefixedString = new char[O2_MAXFILESTR];
    char* tmpStr = prefixedString;
    // Open and check dimensions of power file
    strncpy(prefixedString, powerFileNameTable+trackID*O2_FILETABLE_ENTRY_SIZE, O2_MAXFILESTR);
    prefix_name((char ** const)&prefixedString, adb_feature_root);
    if(prefixedString!=tmpStr)
      delete[] tmpStr;
    powerfd = open(prefixedString, O_RDONLY);
    if (powerfd < 0) {
      error("failed to open power file", prefixedString);
    }
    if (fstat(powerfd, &statbuf) < 0) {
      error("fstat error finding size of power file", prefixedString, "fstat");
    }
    
    if( (statbuf.st_size - sizeof(int)) / (sizeof(double)) != trackTable[trackID] )
      error("Dimension mismatch: numPowers != numVectors", prefixedString);
   
    *sPowerp = new double[trackTable[trackID]]; // Allocate memory for power values
    assert(*sPowerp);
    *spPtrp = *sPowerp;
    insertPowerData(trackTable[trackID], powerfd, *sPowerp);
    if (0 < powerfd) {
      close(powerfd);
    }
    
    audiodb_sequence_sum(*sPowerp, trackTable[trackID], sequenceLength);
    audiodb_sequence_average(*sPowerp, trackTable[trackID], sequenceLength);
    powerTable = 0;

    // Allocate and calculate the l2norm sequence
    *sNormpp = new double[trackTable[trackID]];
    assert(*sNormpp);
    *snPtrp = *sNormpp;
    audiodb_l2norm_buffer(fvp, dbH->dim, trackTable[trackID], *sNormpp);
    audiodb_sequence_sum(*sNormpp, trackTable[trackID], sequenceLength);
    audiodb_sequence_sqrt(*sNormpp, trackTable[trackID], sequenceLength);
  }
}

void audioDB::index_insert_tracks(Uns32T start_track, Uns32T end_track,
				  double** fvpp, double** sNormpp,double** snPtrp, 
				  double** sPowerp, double** spPtrp){  
  size_t nfv = 0;
  double* fvp = 0; // Keep pointer for memory allocation and free() for track data
  Uns32T trackID = 0;

  VERB_LOG(1, "indexing tracks...");

  int trackfd = dbfid;
  for(trackID = start_track ; trackID < end_track ; trackID++ ){
    if( dbH->flags & O2_FLAG_LARGE_ADB ){
      char* prefixedString = new char[O2_MAXFILESTR];
      char* tmpStr = prefixedString;
      // Open and check dimensions of feature file
      strncpy(prefixedString, featureFileNameTable+trackID*O2_FILETABLE_ENTRY_SIZE, O2_MAXFILESTR);
      prefix_name((char ** const) &prefixedString, adb_feature_root);
      if(prefixedString!=tmpStr)
	delete[] tmpStr;
      initInputFile(prefixedString);
      trackfd = infid;
    }
    if(audiodb_read_data(adb, trackfd, trackID, &fvp, &nfv))
      error("failed to read data");
    *fvpp = fvp; // Protect memory allocation and free() for track data
    
    if( dbH->flags & O2_FLAG_LARGE_ADB )
      // Load power and calculate power and l2norm sequence sums
      init_track_aux_data(trackID, fvp, sNormpp, snPtrp, sPowerp, spPtrp);
    
    if(!index_insert_track(trackID, fvpp, snPtrp, spPtrp))
      break;    
    if ( dbH->flags & O2_FLAG_LARGE_ADB ){
      close(infid);
      delete[] *sNormpp;
      delete[] *sPowerp;
      *sNormpp = *sPowerp = *snPtrp = *snPtrp = 0;
    }
  } // end for(trackID = start_track ; ... )
  std::cout << "finished inserting." << endl;
}

int audioDB::index_insert_track(Uns32T trackID, double** fvpp, double** snpp, double** sppp){
  // Loop over the current input track's vectors
  Uns32T numVecs = 0;
  if (trackTable[trackID] > O2_MAXTRACKLEN) {
    if (O2_MAXTRACKLEN < sequenceLength - 1) {
      numVecs = 0;
    } else {
      numVecs = O2_MAXTRACKLEN - sequenceLength + 1;
    }
  } else {
    if (trackTable[trackID] < sequenceLength - 1) {
      numVecs = 0;
    } else {
      numVecs = trackTable[trackID] - sequenceLength + 1;
    }
  }
  
  Uns32T numVecsAboveThreshold = 0, collisionCount = 0; 
  if(numVecs){
    std::vector<std::vector<float> > *vv = audiodb_index_initialize_shingles(numVecs, dbH->dim, sequenceLength);
    
    for( Uns32T pointID = 0 ; pointID < numVecs; pointID++ )
      audiodb_index_make_shingle(vv, pointID, *fvpp, dbH->dim, sequenceLength);
    int vcount = audiodb_index_norm_shingles(vv, *snpp, *sppp, dbH->dim, sequenceLength, radius, normalizedDistance, use_absolute_threshold, absolute_threshold);
    if(vcount == -1) {
      audiodb_index_delete_shingles(vv);
      error("failed to norm shingles");
    }
    numVecsAboveThreshold = vcount;
    collisionCount = index_insert_shingles(vv, trackID, *sppp);
    audiodb_index_delete_shingles(vv);
  }

  float meanCollisionCount = numVecsAboveThreshold?(float)collisionCount/numVecsAboveThreshold:0;

  /* audiodb_index_norm_shingles() only goes as far as the end of the
     sequence, which is right, but the space allocated is for the
     whole track.  */

  /* But numVecs will be <trackTable[track] if trackTable[track]>O2_MAXTRACKLEN
   * So let's be certain the pointers are in the correct place
   */

  if( !(dbH->flags & O2_FLAG_LARGE_ADB) ){
    *snpp += trackTable[trackID];
    *sppp += trackTable[trackID];
    *fvpp += trackTable[trackID] * dbH->dim;
  }

  std::cout << " n=" << trackTable[trackID] << " n'=" << numVecsAboveThreshold << " E[#c]=" << lsh->get_mean_collision_rate() << " E[#p]=" << meanCollisionCount << endl;
  std::cout.flush();  
  return true;
}

Uns32T audioDB::index_insert_shingles(vector<vector<float> >* vv, Uns32T trackID, double* spp){
  Uns32T collisionCount = 0;
  cout << "[" << trackID << "]" << fileTable+trackID*O2_FILETABLE_ENTRY_SIZE;
  for( Uns32T pointID=0 ; pointID < (*vv).size(); pointID+=sequenceHop){
    if(!use_absolute_threshold || (use_absolute_threshold && (*spp >= absolute_threshold)))
      collisionCount += lsh->insert_point((*vv)[pointID], audiodb_index_from_trackinfo(trackID, pointID, audiodb_lsh_n_point_bits(adb)));
    spp+=sequenceHop;
    }
  return collisionCount;
}

/*********************** LSH retrieval ****************************/


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
  }

  delete[] indexName;
  return true;
}

void audiodb_index_add_point_approximate(void *user_data, Uns32T pointID, Uns32T qpos, float dist) {
  adb_qcallback_t *data = (adb_qcallback_t *) user_data;
  adb_t *adb = data->adb;
  adb_qstate_internal_t *qstate = data->qstate;
  uint32_t nbits = audiodb_lsh_n_point_bits(adb);
  uint32_t trackID = audiodb_index_to_track_id(pointID, nbits);
  uint32_t spos = audiodb_index_to_track_pos(pointID, nbits);
  std::set<std::string>::iterator keys_end = qstate->allowed_keys->end();
  if(qstate->allowed_keys->find((*adb->keys)[trackID]) != keys_end) {
    adb_result_t r;
    r.key = (*adb->keys)[trackID].c_str();
    r.dist = dist;
    r.qpos = qpos;
    r.ipos = spos;
    qstate->accumulator->add_point(&r);
  }
}

// Maintain a queue of points to pass to audiodb_query_queue_loop()
// for exact evaluation
void audiodb_index_add_point_exact(void *user_data, Uns32T pointID, Uns32T qpos, float dist) {
  adb_qcallback_t *data = (adb_qcallback_t *) user_data;
  adb_t *adb = data->adb;
  adb_qstate_internal_t *qstate = data->qstate;
  uint32_t nbits = audiodb_lsh_n_point_bits(adb);
  uint32_t trackID = audiodb_index_to_track_id(pointID, nbits);
  uint32_t spos = audiodb_index_to_track_pos(pointID, nbits);
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

  uint32_t Nq = (qpointers.nvectors > O2_MAXTRACKLEN ? O2_MAXTRACKLEN : qpointers.nvectors) - sequence_length + 1;
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

