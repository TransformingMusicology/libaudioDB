#include "audioDB.h"
#include "reporter.h"

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

void audioDB::query(const char* dbName, const char* inFile, adb__queryResponse *adbQueryResponse) {
  // init database tables and dbH first
  if(query_from_key)
    initTables(dbName);
  else
    initTables(dbName, inFile);

  // keyKeyPos requires dbH to be initialized
  if(query_from_key && (!key || (query_from_key_index = getKeyPos((char*)key))==O2_ERR_KEYNOTFOUND))
    error("Query key not found :",key);  
  
  switch (queryType) {
  case O2_POINT_QUERY:
    sequenceLength = 1;
    normalizedDistance = false;
    reporter = new pointQueryReporter< std::greater < NNresult > >(pointNN);
    break;
  case O2_TRACK_QUERY:
    sequenceLength = 1;
    normalizedDistance = false;
    reporter = new trackAveragingReporter< std::greater< NNresult > >(pointNN, trackNN, dbH->numFiles);
    break;
  case O2_SEQUENCE_QUERY:    
    if(no_unit_norming)
      normalizedDistance = false;
    if(radius == 0) {
      reporter = new trackAveragingReporter< std::less< NNresult > >(pointNN, trackNN, dbH->numFiles);
    } else {
      if(index_exists(dbName, radius, sequenceLength)){
	char* indexName = index_get_name(dbName, radius, sequenceLength);
	lsh = index_allocate(indexName, false);
	reporter = new trackSequenceQueryRadReporter(trackNN, index_to_trackID(lsh->get_maxp(), lsh_n_point_bits)+1);
	delete[] indexName;
      }
      else
	reporter = new trackSequenceQueryRadReporter(trackNN, dbH->numFiles);
    }
    break;
  case O2_N_SEQUENCE_QUERY:
    if(no_unit_norming)
      normalizedDistance = false;
    if(radius == 0) {
      reporter = new trackSequenceQueryNNReporter< std::less < NNresult > >(pointNN, trackNN, dbH->numFiles);
    } else {
      if(index_exists(dbName, radius, sequenceLength)){
	char* indexName = index_get_name(dbName, radius, sequenceLength);
	lsh = index_allocate(indexName, false);
	reporter = new trackSequenceQueryRadNNReporter(pointNN,trackNN, index_to_trackID(lsh->get_maxp(), lsh_n_point_bits)+1);
	delete[] indexName;
      }
      else
	reporter = new trackSequenceQueryRadNNReporter(pointNN,trackNN, dbH->numFiles);
    }
    break;
  case O2_ONE_TO_ONE_N_SEQUENCE_QUERY :
    if(radius == 0) {
      error("query-type not yet supported");
    } else {
      reporter = new trackSequenceQueryRadNNReporterOneToOne(pointNN,trackNN, dbH->numFiles);
    }
    break;
  default:
    error("unrecognized queryType in query()");
  }  

  // Test for index (again) here
  if(radius && index_exists(dbName, radius, sequenceLength)){ 
    VERB_LOG(1, "Calling indexed query on database %s, radius=%f, sequenceLength=%d\n", dbName, radius, sequenceLength);
    index_query_loop(dbName, query_from_key_index);
  }
  else{
    VERB_LOG(1, "Calling brute-force query on database %s\n", dbName);
    query_loop(dbName, query_from_key_index);
  }

  reporter->report(fileTable, adbQueryResponse);
}

// return ordinal position of key in keyTable
// this should really be a STL hash map search
unsigned audioDB::getKeyPos(char* key){  
  if(!dbH)
    error("dbH not initialized","getKeyPos");
  for(unsigned k=0; k<dbH->numFiles; k++)
    if(strncmp(fileTable + k*O2_FILETABLE_ENTRY_SIZE, key, strlen(key))==0)
      return k;
  error("Key not found",key);
  return O2_ERR_KEYNOTFOUND;
}

// This is a common pattern in sequence queries: what we are doing is
// taking a window of length seqlen over a buffer of length length,
// and placing the sum of the elements in that window in the first
// element of the window: thus replacing all but the last seqlen
// elements in the buffer with the corresponding windowed sum.
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

// In contrast to sequence_sum() above, sequence_sqrt() and
// sequence_average() below are simple mappers across the sequence.
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

void audioDB::initialize_arrays(int track, unsigned int numVectors, double *query, double *data_buffer, double **D, double **DD) {
  unsigned int j, k, l, w;
  double *dp, *qp, *sp;

  const unsigned HOP_SIZE = sequenceHop;
  const unsigned wL = sequenceLength;

  for(j = 0; j < numVectors; j++) {
    // Sum products matrix
    D[j] = new double[trackTable[track]]; 
    assert(D[j]);
    // Matched filter matrix
    DD[j]=new double[trackTable[track]];
    assert(DD[j]);
  }

  // Dot product
  for(j = 0; j < numVectors; j++)
    for(k = 0; k < trackTable[track]; k++){
      qp = query + j * dbH->dim;
      sp = data_buffer + k * dbH->dim;
      DD[j][k] = 0.0; // Initialize matched filter array
      dp = &D[j][k];  // point to correlation cell j,k
      *dp = 0.0;      // initialize correlation cell
      l = dbH->dim;         // size of vectors
      while(l--)
        *dp += *qp++ * *sp++;
    }
  
  // Matched Filter
  // HOP SIZE == 1
  double* spd;
  if(HOP_SIZE == 1) { // HOP_SIZE = shingleHop
    for(w = 0; w < wL; w++) {
      for(j = 0; j < numVectors - w; j++) { 
        sp = DD[j];
        spd = D[j+w] + w;
        k = trackTable[track] - w;
	while(k--)
	  *sp++ += *spd++;
      }
    }
  } else { // HOP_SIZE != 1
    for(w = 0; w < wL; w++) {
      for(j = 0; j < numVectors - w; j += HOP_SIZE) {
        sp = DD[j];
        spd = D[j+w]+w;
        for(k = 0; k < trackTable[track] - w; k += HOP_SIZE) {
          *sp += *spd;
          sp += HOP_SIZE;
          spd += HOP_SIZE;
        }
      }
    }
  }
}

void audioDB::delete_arrays(int track, unsigned int numVectors, double **D, double **DD) {
  if(D != NULL) {
    for(unsigned int j = 0; j < numVectors; j++) {
      delete[] D[j];
    }
  }
  if(DD != NULL) {
    for(unsigned int j = 0; j < numVectors; j++) {
      delete[] DD[j];
    }
  }
}

void audioDB::read_data(int trkfid, int track, double **data_buffer_p, size_t *data_buffer_size_p) {
  if (trackTable[track] * sizeof(double) * dbH->dim > *data_buffer_size_p) {
    if(*data_buffer_p) {
      free(*data_buffer_p);
    }
    { 
      *data_buffer_size_p = trackTable[track] * sizeof(double) * dbH->dim;
      void *tmp = malloc(*data_buffer_size_p);
      if (tmp == NULL) {
        error("error allocating data buffer");
      }
      *data_buffer_p = (double *) tmp;
    }
  }

  read(trkfid, *data_buffer_p, trackTable[track] * sizeof(double) * dbH->dim);
}

// These names deserve some unpicking.  The names starting with a "q"
// are pointers to the query, norm and power vectors; the names
// starting with "v" are things that will end up pointing to the
// actual query point's information.  -- CSR, 2007-12-05
void audioDB::set_up_query(double **qp, double **vqp, double **qnp, double **vqnp, double **qpp, double **vqpp, double *mqdp, unsigned *nvp) {
  *nvp = (statbuf.st_size - sizeof(int)) / (dbH->dim * sizeof(double));

  if(!(dbH->flags & O2_FLAG_L2NORM)) {
    error("Database must be L2 normed for sequence query","use -L2NORM");
  }

  if(*nvp < sequenceLength) {
    error("Query shorter than requested sequence length", "maybe use -l");
  }
  
  VERB_LOG(1, "performing norms... ");

  *qp = new double[*nvp * dbH->dim];
  memcpy(*qp, indata+sizeof(int), *nvp * dbH->dim * sizeof(double));
  *qnp = new double[*nvp];
  unitNorm(*qp, dbH->dim, *nvp, *qnp);

  sequence_sum(*qnp, *nvp, sequenceLength);
  sequence_sqrt(*qnp, *nvp, sequenceLength);

  if (usingPower) {
    *qpp = new double[*nvp];
    if (lseek(powerfd, sizeof(int), SEEK_SET) == (off_t) -1) {
      error("error seeking to data", powerFileName, "lseek");
    }
    int count = read(powerfd, *qpp, *nvp * sizeof(double));
    if (count == -1) {
      error("error reading data", powerFileName, "read");
    }
    if ((unsigned) count != *nvp * sizeof(double)) {
      error("short read", powerFileName);
    }

    sequence_sum(*qpp, *nvp, sequenceLength);
    sequence_average(*qpp, *nvp, sequenceLength);
  }

  if (usingTimes) {
    unsigned int k;
    *mqdp = 0.0;
    double *querydurs = new double[*nvp];
    double *timesdata = new double[*nvp*2];
    insertTimeStamps(*nvp, timesFile, timesdata);
    for(k = 0; k < *nvp; k++) {
      querydurs[k] = timesdata[2*k+1] - timesdata[2*k];
      *mqdp += querydurs[k];
    }
    *mqdp /= k;

    VERB_LOG(1, "mean query file duration: %f\n", *mqdp);

    delete [] querydurs;
    delete [] timesdata;
  }

  // Defaults, for exhaustive search (!usingQueryPoint)
  *vqp = *qp;
  *vqnp = *qnp;
  *vqpp = *qpp;

  if(usingQueryPoint) {
    if( !(queryPoint < *nvp && queryPoint < *nvp - sequenceLength + 1) ) {
      error("queryPoint >= numVectors-sequenceLength+1 in query");
    } else {
      VERB_LOG(1, "query point: %u\n", queryPoint);
      *vqp = *qp + queryPoint * dbH->dim;
      *vqnp = *qnp + queryPoint;
      if (usingPower) {
        *vqpp = *qpp + queryPoint;
      }
      *nvp = sequenceLength;
    }
  }
}

// Does the same as set_up_query(...) but from database features instead of from a file
// Constructs the same outputs as set_up_query
void audioDB::set_up_query_from_key(double **qp, double **vqp, double **qnp, double **vqnp, double **qpp, double **vqpp, double *mqdp, unsigned *nvp, Uns32T queryIndex) {
  if(!trackTable)
    error("trackTable not initialized","set_up_query_from_key");

  if(!(dbH->flags & O2_FLAG_L2NORM)) {
    error("Database must be L2 normed for sequence query","use -L2NORM");
  }
  
  if(dbH->flags & O2_FLAG_POWER)
    usingPower = true;
  
  if(dbH->flags & O2_FLAG_TIMES)
    usingTimes = true;

  *nvp = trackTable[queryIndex];  
  if(*nvp < sequenceLength) {
    error("Query shorter than requested sequence length", "maybe use -l");
  }
  
  VERB_LOG(1, "performing norms... ");

  // For LARGE_ADB load query features from file
  if( dbH->flags & O2_FLAG_LARGE_ADB ){
    if(infid>0)
      close(infid);
    char* prefixedString = new char[O2_MAXFILESTR];
    char* tmpStr = prefixedString;
    strncpy(prefixedString, featureFileNameTable+queryIndex*O2_FILETABLE_ENTRY_SIZE, O2_MAXFILESTR);
    prefix_name(&prefixedString, adb_feature_root);
    if(tmpStr!=prefixedString)
      delete[] tmpStr;
    initInputFile(prefixedString, false); // nommap, file pointer at correct position
    size_t allocatedSize = 0;
    read_data(infid, queryIndex, qp, &allocatedSize); // over-writes qp and allocatedSize
    // Consistency check on allocated memory and query feature size
    if(*nvp*sizeof(double)*dbH->dim != allocatedSize)
      error("Query memory allocation failed consitency check","set_up_query_from_key");
    // Allocated and calculate auxillary sequences: l2norm and power
    init_track_aux_data(queryIndex, *qp, qnp, vqnp, qpp, vqpp);
  }
  else{ // Load from self-contained ADB database
    // Read query feature vectors from database
    *qp = NULL;
    lseek(dbfid, dbH->dataOffset + trackOffsetTable[queryIndex] * sizeof(double), SEEK_SET);
    size_t allocatedSize = 0;
    read_data(dbfid, queryIndex, qp, &allocatedSize);
    // Consistency check on allocated memory and query feature size
    if(*nvp*sizeof(double)*dbH->dim != allocatedSize)
      error("Query memory allocation failed consitency check","set_up_query_from_key");
    
    Uns32T trackIndexOffset = trackOffsetTable[queryIndex]/dbH->dim; // Convert num data elements to num vectors
    // Copy L2 norm partial-sum coefficients
    assert(*qnp = new double[*nvp]);
    memcpy(*qnp, l2normTable+trackIndexOffset, *nvp*sizeof(double));
    sequence_sum(*qnp, *nvp, sequenceLength);
    sequence_sqrt(*qnp, *nvp, sequenceLength);
    
    if( usingPower ){
      // Copy Power partial-sum coefficients
      assert(*qpp = new double[*nvp]);
      memcpy(*qpp, powerTable+trackIndexOffset, *nvp*sizeof(double));
      sequence_sum(*qpp, *nvp, sequenceLength);
      sequence_average(*qpp, *nvp, sequenceLength);
    }
    
    if (usingTimes) {
      unsigned int k;
      *mqdp = 0.0;
      double *querydurs = new double[*nvp];
      double *timesdata = new double[*nvp*2];
      assert(querydurs && timesdata);
      memcpy(timesdata, timesTable+trackIndexOffset, *nvp*sizeof(double));    
      for(k = 0; k < *nvp; k++) {
	querydurs[k] = timesdata[2*k+1] - timesdata[2*k];
	*mqdp += querydurs[k];
      }
      *mqdp /= k;
      
      VERB_LOG(1, "mean query file duration: %f\n", *mqdp);
      
      delete [] querydurs;
      delete [] timesdata;
    }
  }

  // Defaults, for exhaustive search (!usingQueryPoint)
  *vqp = *qp;
  *vqnp = *qnp;
  *vqpp = *qpp;

  if(usingQueryPoint) {
    if( !(queryPoint < *nvp && queryPoint < *nvp - sequenceLength + 1) ) {
      error("queryPoint >= numVectors-sequenceLength+1 in query");
    } else {
      VERB_LOG(1, "query point: %u\n", queryPoint);
      *vqp = *qp + queryPoint * dbH->dim;
      *vqnp = *qnp + queryPoint;
      if (usingPower) {
        *vqpp = *qpp + queryPoint;
      }
      *nvp = sequenceLength;
    }
  }
}


// FIXME: this is not the right name; we're not actually setting up
// the database, but copying various bits of it out of mmap()ed tables
// in order to reduce seeks.
void audioDB::set_up_db(double **snp, double **vsnp, double **spp, double **vspp, double **mddp, unsigned int *dvp) {
  *dvp = dbH->length / (dbH->dim * sizeof(double));
  *snp = new double[*dvp];

  double *snpp = *snp, *sppp = 0;
  memcpy(*snp, l2normTable, *dvp * sizeof(double));

  if (usingPower) {
    if (!(dbH->flags & O2_FLAG_POWER)) {
      error("database not power-enabled", dbName);
    }
    *spp = new double[*dvp];
    sppp = *spp;
    memcpy(*spp, powerTable, *dvp * sizeof(double));
  }

  for(unsigned int i = 0; i < dbH->numFiles; i++){
    if(trackTable[i] >= sequenceLength) {
      sequence_sum(snpp, trackTable[i], sequenceLength);
      sequence_sqrt(snpp, trackTable[i], sequenceLength);

      if (usingPower) {
	sequence_sum(sppp, trackTable[i], sequenceLength);
        sequence_average(sppp, trackTable[i], sequenceLength);
      }
    }
    snpp += trackTable[i];
    if (usingPower) {
      sppp += trackTable[i];
    }
  }

  if (usingTimes) {
    if(!(dbH->flags & O2_FLAG_TIMES)) {
      error("query timestamps provided for non-timed database", dbName);
    }

    *mddp = new double[dbH->numFiles];

    for(unsigned int k = 0; k < dbH->numFiles; k++) {
      unsigned int j;
      (*mddp)[k] = 0.0;
      for(j = 0; j < trackTable[k]; j++) {
	(*mddp)[k] += timesTable[2*j+1] - timesTable[2*j];
      }
      (*mddp)[k] /= j;
    }
  }

  *vsnp = *snp;
  *vspp = *spp;
}

// query_points()
//
// using PointPairs held in the exact_evaluation_queue compute squared distance for each PointPair
// and insert result into the current reporter.
//
// Preconditions:
// A query inFile has been opened with setup_query(...) and query pointers initialized
// The database contains some points
// An exact_evaluation_queue has been allocated and populated
// A reporter has been allocated
//
// Postconditions:
// reporter contains the points and distances that meet the reporter constraints 

void audioDB::query_loop_points(double* query, double* qnPtr, double* qpPtr, double meanQdur, Uns32T numVectors){ 
  unsigned int dbVectors;
  double *sNorm = 0, *snPtr, *sPower = 0, *spPtr = 0;
  double *meanDBdur = 0;

  // check pre-conditions
  assert(exact_evaluation_queue&&reporter);
  if(!exact_evaluation_queue->size()) // Exit if no points to evaluate
    return;

  // Compute database info
  // FIXME: we more than likely don't need very much of the database
  // so make a new method to build these values per-track or, even better, per-point
  if( !( dbH->flags & O2_FLAG_LARGE_ADB) )
    set_up_db(&sNorm, &snPtr, &sPower, &spPtr, &meanDBdur, &dbVectors);

  VERB_LOG(1, "matching points...");

  assert(pointNN>0 && pointNN<=O2_MAXNN);
  assert(trackNN>0 && trackNN<=O2_MAXNN);

  // We are guaranteed that the order of points is sorted by:
  // trackID, spos, qpos
  // so we can be relatively efficient in initialization of track data.
  // Here we assume that points don't overlap, so we will use exhaustive dot
  // product evaluation instead of memoization of partial sums which is used
  // for exhaustive brute-force evaluation from smaller databases: e.g. query_loop()
  double dist;
  size_t data_buffer_size = 0;
  double *data_buffer = 0;
  Uns32T trackOffset = 0;
  Uns32T trackIndexOffset = 0;
  Uns32T currentTrack = 0x80000000; // Initialize with a value outside of track index range
  Uns32T npairs = exact_evaluation_queue->size();
  while(npairs--){
    PointPair pp = exact_evaluation_queue->top();
    // Large ADB track data must be loaded here for sPower
    if(dbH->flags & O2_FLAG_LARGE_ADB){
      trackOffset=0;
      trackIndexOffset=0;
      if(currentTrack!=pp.trackID){
	char* prefixedString = new char[O2_MAXFILESTR];
	char* tmpStr = prefixedString;
	// On currentTrack change, allocate and load track data
	currentTrack=pp.trackID;
	SAFE_DELETE_ARRAY(sNorm);
	SAFE_DELETE_ARRAY(sPower);
	if(infid>0)
	  close(infid);
	// Open and check dimensions of feature file
	strncpy(prefixedString, featureFileNameTable+pp.trackID*O2_FILETABLE_ENTRY_SIZE, O2_MAXFILESTR);
	prefix_name((char ** const) &prefixedString, adb_feature_root);
	if (prefixedString!=tmpStr)
	  delete[] tmpStr;
	initInputFile(prefixedString, false); // nommap, file pointer at correct position
	// Load the feature vector data for current track into data_buffer
	read_data(infid, pp.trackID, &data_buffer, &data_buffer_size);	
	// Load power and calculate power and l2norm sequence sums
	init_track_aux_data(pp.trackID, data_buffer, &sNorm, &snPtr, &sPower, &spPtr);
      }
    }
    else{
      // These offsets are w.r.t. the entire database of feature vectors and auxillary variables
      trackOffset=trackOffsetTable[pp.trackID]; // num data elements offset
      trackIndexOffset=trackOffset/dbH->dim;    // num vectors offset
    }    
    Uns32T qPos = usingQueryPoint?0:pp.qpos;// index for query point
    Uns32T sPos = trackIndexOffset+pp.spos; // index into l2norm table
    // Test power thresholds before computing distance
    if( ( !usingPower || powers_acceptable(qpPtr[qPos], sPower[sPos])) &&
	( qPos<numVectors-sequenceLength+1 && pp.spos<trackTable[pp.trackID]-sequenceLength+1 ) ){
      // Non-large ADB track data is loaded inside power test for efficiency
      if( !(dbH->flags & O2_FLAG_LARGE_ADB) && (currentTrack!=pp.trackID) ){
	// On currentTrack change, allocate and load track data
	currentTrack=pp.trackID;
	lseek(dbfid, dbH->dataOffset + trackOffset * sizeof(double), SEEK_SET);
	read_data(dbfid, currentTrack, &data_buffer, &data_buffer_size);
      }
      // Compute distance
      dist = dot_product_points(query+qPos*dbH->dim, data_buffer+pp.spos*dbH->dim, dbH->dim*sequenceLength);
      double qn = qnPtr[qPos];
      double sn = sNorm[sPos];
      if(normalizedDistance) 
	dist = 2 - (2/(qn*sn))*dist;
      else 
	if(no_unit_norming)
	  dist = qn*qn + sn*sn - 2*dist;
      // else
      // dist = dist;      
      if((!radius) || dist <= (O2_LSH_EXACT_MULT*radius+O2_DISTANCE_TOLERANCE)) 
	reporter->add_point(pp.trackID, pp.qpos, pp.spos, dist);    
    }
    exact_evaluation_queue->pop();
  }
  // Cleanup
  SAFE_DELETE_ARRAY(sNorm);
  SAFE_DELETE_ARRAY(sPower);
  SAFE_DELETE_ARRAY(meanDBdur);
}

// A completely unprotected dot-product method
// Caller is responsible for ensuring that memory is within bounds
inline double audioDB::dot_product_points(double* q, double* p, Uns32T  L){
  double dist = 0.0;
  while(L--)
    dist += *q++ * *p++;
  return dist;
}

void audioDB::query_loop(const char* dbName, Uns32T queryIndex) {
  
  unsigned int numVectors;
  double *query, *query_data;
  double *qNorm, *qnPtr, *qPower = 0, *qpPtr = 0;
  double meanQdur;

  if( dbH->flags & O2_FLAG_LARGE_ADB )
    error("error: LARGE_ADB requires indexed query");

  if(query_from_key)
    set_up_query_from_key(&query_data, &query, &qNorm, &qnPtr, &qPower, &qpPtr, &meanQdur, &numVectors, queryIndex);
  else
    set_up_query(&query_data, &query, &qNorm, &qnPtr, &qPower, &qpPtr, &meanQdur, &numVectors);

  unsigned int dbVectors;
  double *sNorm, *snPtr, *sPower = 0, *spPtr = 0;
  double *meanDBdur = 0;

  set_up_db(&sNorm, &snPtr, &sPower, &spPtr, &meanDBdur, &dbVectors);

  VERB_LOG(1, "matching tracks...");
  
  assert(pointNN>0 && pointNN<=O2_MAXNN);
  assert(trackNN>0 && trackNN<=O2_MAXNN);

  unsigned j,k,track,trackOffset=0, HOP_SIZE=sequenceHop, wL=sequenceLength;
  double **D = 0;    // Differences query and target 
  double **DD = 0;   // Matched filter distance

  D = new double*[numVectors]; // pre-allocate 
  DD = new double*[numVectors];

  gettimeofday(&tv1, NULL); 
  unsigned processedTracks = 0;
  off_t trackIndexOffset;
  char nextKey[MAXSTR];

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

    // skip identity on query_from_key
    if( query_from_key && (track == queryIndex) ) {
      if(queryIndex!=dbH->numFiles-1){
	track++;
	trackOffset = trackOffsetTable[track];
	lseek(dbfid, dbH->dataOffset + trackOffset * sizeof(double), SEEK_SET);
      }
      else{
	break;
      }
    }

    trackIndexOffset=trackOffset/dbH->dim; // numVectors offset

    read_data(dbfid, track, &data_buffer, &data_buffer_size);
    if(sequenceLength <= trackTable[track]) {  // test for short sequences
      
      VERB_LOG(7,"%u.%jd.%u | ", track, (intmax_t) trackIndexOffset, trackTable[track]);
      
      initialize_arrays(track, numVectors, query, data_buffer, D, DD);

      if(usingTimes) {
        VERB_LOG(3,"meanQdur=%f meanDBdur=%f\n", meanQdur, meanDBdur[track]);
      }

      if((!usingTimes) || fabs(meanDBdur[track]-meanQdur) < meanQdur*timesTol) {
        if(usingTimes) {
          VERB_LOG(3,"within duration tolerance.\n");
        }

	// Search for minimum distance by shingles (concatenated vectors)
	for(j = 0; j <= numVectors - wL; j += HOP_SIZE) {
	  for(k = 0; k <= trackTable[track] - wL; k += HOP_SIZE) {
            double thisDist;
            if(normalizedDistance) 
              thisDist = 2-(2/(qnPtr[j]*sNorm[trackIndexOffset+k]))*DD[j][k];
	    else 
	      if(no_unit_norming)
		thisDist = qnPtr[j]*qnPtr[j]+sNorm[trackIndexOffset+k]*sNorm[trackIndexOffset+k] - 2*DD[j][k];
	      else
		thisDist = DD[j][k];

	    // Power test
	    if ((!usingPower) || powers_acceptable(qpPtr[j], sPower[trackIndexOffset + k])) {
              // radius test
              if((!radius) || thisDist <= (radius+O2_DISTANCE_TOLERANCE)) {
                reporter->add_point(track, usingQueryPoint ? queryPoint : j, k, thisDist);
              }
            }
          }
        }
      } // Duration match            
      delete_arrays(track, numVectors, D, DD);
    }
  }

  free(data_buffer);

  gettimeofday(&tv2,NULL);
  VERB_LOG(1,"elapsed time: %ld msec\n",
           (tv2.tv_sec*1000 + tv2.tv_usec/1000) - 
           (tv1.tv_sec*1000 + tv1.tv_usec/1000))

  // Clean up
  if(query_data)
    delete[] query_data;
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
  if(meanDBdur)
    delete[] meanDBdur;
}

// Unit norm block of features
void audioDB::unitNorm(double* X, unsigned dim, unsigned n, double* qNorm){
  unsigned d;
  double L2, *p;

  VERB_LOG(2, "norming %u vectors...", n);
  while(n--) {
    p = X;
    L2 = 0.0;
    d = dim;
    while(d--) {
      L2 += *p * *p;
      p++;
    }
    if(qNorm) {
      *qNorm++=L2;
    }
    X += dim;
  }
  VERB_LOG(2, "done.\n");
}


