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

#include "audioDB.h"
#include "ReporterBase.h"


/************************* LSH point index to audioDB conversion  *****************/
Uns32T audioDB::index_to_trackID(Uns32T lshID){
  return lshID>>LSH_N_POINT_BITS;
}

Uns32T audioDB::index_to_trackPos(Uns32T lshID){
  return lshID&LSH_POINT_MASK;
}

Uns32T audioDB::index_from_trackInfo(Uns32T trackID, Uns32T spos){
  return (trackID << LSH_N_POINT_BITS) | spos;
}

/************************* LSH indexing and query initialization  *****************/

char* audioDB::index_get_name(const char*dbName, double radius, Uns32T sequenceLength){
  char* indexName = new char[MAXSTR];  
  // Attempt to make new file
  if(strlen(dbName) > (MAXSTR - 32))
    error("dbName is too long for LSH index filename appendages");
  strncpy(indexName, dbName, MAXSTR);
  sprintf(indexName+strlen(dbName), ".lsh.%019.9f.%d", radius, sequenceLength);
  return indexName;
}

// return true if index exists else return false
int audioDB::index_exists(const char* dbName, double radius, Uns32T sequenceLength){
  // Test to see if file exists
  char* indexName = index_get_name(dbName, radius, sequenceLength);
  lshfid = open (indexName, O_RDONLY);
  delete[] indexName;
  close(lshfid);
  
  if(lshfid<0)
    return false;  
  else
    return true;
}

vector<vector<float> >* audioDB::index_initialize_shingles(Uns32T sz){
  if(vv)
    delete vv;
  vv = new vector<vector<float> >(sz);
  for(Uns32T i=0 ; i < sz ; i++)
    (*vv)[i]=vector<float>(dbH->dim*sequenceLength);  // allocate shingle storage
  return vv;
}

/********************  LSH indexing audioDB database access forall s \in {S} ***********************/

// Prepare the AudioDB database for read access and allocate auxillary memory
void audioDB::index_initialize(double **snp, double **vsnp, double **spp, double **vspp, Uns32T *dvp) {
  *dvp = dbH->length / (dbH->dim * sizeof(double)); // number of database vectors
  *snp = new double[*dvp];  // songs norm pointer: L2 norm table for each vector

  double *snpp = *snp, *sppp = 0;
  memcpy(*snp, l2normTable, *dvp * sizeof(double));

  if (!(dbH->flags & O2_FLAG_POWER)) {
    error("database not power-enabled", dbName);
  }
  *spp = new double[*dvp]; // song powertable pointer
  sppp = *spp;
  memcpy(*spp, powerTable, *dvp * sizeof(double));

  for(Uns32T i = 0; i < dbH->numFiles; i++){
    if(trackTable[i] >= sequenceLength) {
      sequence_sum(snpp, trackTable[i], sequenceLength);
      sequence_sqrt(snpp, trackTable[i], sequenceLength);
      
      sequence_sum(sppp, trackTable[i], sequenceLength);
      sequence_average(sppp, trackTable[i], sequenceLength);
    }
    snpp += trackTable[i];
    sppp += trackTable[i];
  }

  *vsnp = *snp;
  *vspp = *spp;

  // Move the feature vector read pointer to start of fetures in database
  lseek(dbfid, dbH->dataOffset, SEEK_SET);
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

  newIndexName = index_get_name(dbName, radius, sequenceLength);

  // Set unit norming flag override
  audioDB::normalizedDistance = !audioDB::no_unit_norming;

  printf("INDEX: dim %d\n", dbH->dim);
  printf("INDEX: R %f\n", radius);
  printf("INDEX: seqlen %d\n", sequenceLength);
  printf("INDEX: lsh_w %f\n", lsh_param_w);
  printf("INDEX: lsh_k %d\n", lsh_param_k);
  printf("INDEX: lsh_m %d\n", lsh_param_m);
  printf("INDEX: lsh_N %d\n", lsh_param_N);
  printf("INDEX: lsh_C %d\n", lsh_param_ncols);
  printf("INDEX: lsh_b %d\n", lsh_param_b);
  printf("INDEX: normalized? %s\n", normalizedDistance?"true":"false"); 
  fflush(stdout);


  index_initialize(&sNorm, &snPtr, &sPower, &spPtr, &dbVectors);
  
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
    index_insert_tracks(0, endTrack, &fvp, &sNorm, &snPtr, &sPower, &spPtr);    
    lsh->serialize(newIndexName, lsh_in_core?O2_SERIAL_FILEFORMAT2:O2_SERIAL_FILEFORMAT1);
    
    // Clean up
    delete lsh;
    close(lshfid);
  }
  
  // Attempt to open LSH file
  if((lshfid = open(newIndexName,O_RDONLY))>0){
    printf("INDEX: merging with existing LSH index\n");
    fflush(stdout);

    // Get the lsh header info and find how many tracks are inserted already
    lsh = new LSH(newIndexName, false); // lshInCore=false to avoid loading hashTables here
    assert(lsh);
    Uns32T maxs = index_to_trackID(lsh->get_maxp())+1;
    delete lsh;

    // This allows for updating index after more tracks are inserted into audioDB
    for(Uns32T startTrack = maxs; startTrack < dbH->numFiles; startTrack+=lsh_param_b){

      Uns32T endTrack = startTrack + lsh_param_b;
      if( endTrack > dbH->numFiles)
	endTrack = dbH->numFiles;
      printf("Indexing track range: %d - %d\n", startTrack, endTrack);
      fflush(stdout);
      lsh = new LSH(newIndexName, lsh_in_core); // Initialize core memory for LSH tables
      assert(lsh);
      
      // Insert up to lsh_param_b database tracks
      index_insert_tracks(startTrack, endTrack, &fvp, &sNorm, &snPtr, &sPower, &spPtr);

      // Serialize to file
      lsh->serialize(newIndexName, lsh_in_core?O2_SERIAL_FILEFORMAT2:O2_SERIAL_FILEFORMAT1); // Serialize core LSH heap to disk
      delete lsh;
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
  if(sNorm)
    delete[] sNorm;
  if(sPower)
    delete[] sPower;


}

void audioDB::index_insert_tracks(Uns32T start_track, Uns32T end_track,
				  double** fvpp, double** sNormpp,double** snPtrp, 
				  double** sPowerp, double** spPtrp){  
  size_t nfv = 0;
  double* fvp = 0; // Keep pointer for memory allocation and free() for track data
  Uns32T trackID = 0;

  VERB_LOG(1, "indexing tracks...");


  for(trackID = start_track ; trackID < end_track ; trackID++ ){
    read_data(trackID, &fvp, &nfv); // over-writes fvp and nfv
    *fvpp = fvp; // Protect memory allocation and free() for track data
    if(!index_insert_track(trackID, fvpp, snPtrp, spPtrp))
      break;    
  }
  std::cout << "finished inserting." << endl;
}

int audioDB::index_insert_track(Uns32T trackID, double** fvpp, double** snpp, double** sppp){
  // Loop over the current input track's vectors
  Uns32T numVecs = (trackTable[trackID]>O2_MAXTRACKLEN?O2_MAXTRACKLEN:trackTable[trackID]) - sequenceLength + 1 ;
  vv = index_initialize_shingles(numVecs);

  for( Uns32T pointID = 0 ; pointID < numVecs; pointID++ )
    index_make_shingle(vv, pointID, *fvpp, dbH->dim, sequenceLength);
  
  Uns32T numVecsAboveThreshold = index_norm_shingles(vv, *snpp, *sppp);
  Uns32T collisionCount = index_insert_shingles(vv, trackID, *sppp);
  float meanCollisionCount = numVecsAboveThreshold?(float)collisionCount/numVecsAboveThreshold:0;

  /* index_norm_shingles() only goes as far as the end of the
     sequence, which is right, but the space allocated is for the
     whole track.  */

  /* But numVecs will be <trackTable[track] if trackTable[track]>O2_MAXTRACKLEN
   * So let's be certain the pointers are in the correct place
   */

  *snpp += trackTable[trackID];
  *sppp += trackTable[trackID];
  *fvpp += trackTable[trackID] * dbH->dim;

  std::cout << " n=" << trackTable[trackID] << " n'=" << numVecsAboveThreshold << " E[#c]=" << lsh->get_mean_collision_rate() << " E[#p]=" << meanCollisionCount << endl;
  std::cout.flush();  
  return true;
}

Uns32T audioDB::index_insert_shingles(vector<vector<float> >* vv, Uns32T trackID, double* spp){
  Uns32T collisionCount = 0;
  cout << "[" << trackID << "]" << fileTable+trackID*O2_FILETABLE_ENTRY_SIZE;
  for( Uns32T pointID=0 ; pointID < (*vv).size(); pointID++)
    if(!use_absolute_threshold || (use_absolute_threshold && (*spp++ >= absolute_threshold)))
      collisionCount += lsh->insert_point((*vv)[pointID], index_from_trackInfo(trackID, pointID));
  return collisionCount;
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

void audioDB::index_make_shingle(vector<vector<float> >* vv, Uns32T idx, double* fvp, Uns32T dim, Uns32T seqLen){
  assert(idx<(*vv).size());
  vector<float>::iterator ve = (*vv)[idx].end();
  vi=(*vv)[idx].begin();        // shingle iterator  
  // First feature vector in shingle
  if(idx==0){
    while(vi!=ve)
      *vi++ = (float)(*fvp++);
  }
  // Not first feature vector in shingle
  else{
    vector<float>::iterator ui=(*vv)[idx-1].begin() + dim; // previous shingle iterator
    // Previous seqLen-1 dim-vectors
    while(vi!=ve-dim)
      *vi++=*ui++;
    // Move data pointer to next feature vector
    fvp += ( seqLen + idx - 1 ) * dim ;
    // New d-vector
    while(vi!=ve)
      *vi++ = (float)(*fvp++);
  }
}

// norm shingles
// in-place norming, no deletions
// If using power, return number of shingles above power threshold
int audioDB::index_norm_shingles(vector<vector<float> >* vv, double* snp, double* spp){  
  int z = 0; // number of above-threshold shingles
  float l2norm;
  double power;
  float oneOverRadius = 1./(float)sqrt(radius); // Passed radius is really radius^2
  float oneOverSqrtl2NormDivRad = oneOverRadius;
  if(!spp)
    error("LSH indexing and query requires a power feature using -w or -W");
  Uns32T shingleSize = sequenceLength*dbH->dim;
  for(Uns32T a=0; a<(*vv).size(); a++){
    l2norm = (float)(*snp++);
    if(audioDB::normalizedDistance)
      oneOverSqrtl2NormDivRad = (1./l2norm)*oneOverRadius;
    
    for(Uns32T b=0; b < shingleSize ; b++)
      (*vv)[a][b]*=oneOverSqrtl2NormDivRad;

    power = *spp++;
    if(use_absolute_threshold){
      if ( power >= absolute_threshold )
	z++;
    }
    else
      z++;	
  }
  return z;
}
  

/*********************** LSH retrieval ****************************/


// return true if indexed query performed else return false
int audioDB::index_init_query(const char* dbName){

  if(!(index_exists(dbName, radius, sequenceLength)))
    return false;

  char* indexName = index_get_name(dbName, radius, sequenceLength);  

  // Test to see if file exists
  if((lshfid = open (indexName, O_RDONLY)) < 0){
    delete[] indexName;
    return false;  
  }

  printf("INDEX: initializing header\n");
  
  lsh = new LSH(indexName, false); // Get the header only here
  assert(lsh);
  sequenceLength = lsh->get_lshHeader()->dataDim / dbH->dim; // shingleDim / vectorDim
  

  if( fabs(radius - lsh->get_radius())>fabs(O2_DISTANCE_TOLERANCE))
    printf("*** Warning: adb_radius (%f) != lsh_radius (%f) ***\n", radius, lsh->get_radius());

  printf("INDEX: dim %d\n", dbH->dim);
  printf("INDEX: R %f\n", lsh->get_radius());
  printf("INDEX: seqlen %d\n", sequenceLength);
  printf("INDEX: w %f\n", lsh->get_lshHeader()->get_binWidth());
  printf("INDEX: k %d\n", lsh->get_lshHeader()->get_numFuns());
  printf("INDEX: L (m*(m-1))/2 %d\n", lsh->get_lshHeader()->get_numTables());
  printf("INDEX: N %d\n", lsh->get_lshHeader()->get_numRows());
  printf("INDEX: s %d\n", index_to_trackID(lsh->get_maxp()));
  printf("INDEX: Opened LSH index file %s\n", indexName);
  fflush(stdout);

  // Check to see if we are loading hash tables into core, and do so if true
  if((lsh->get_lshHeader()->flags&O2_SERIAL_FILEFORMAT2) || lsh_in_core){
    printf("INDEX: loading hash tables into core %s\n", (lsh->get_lshHeader()->flags&O2_SERIAL_FILEFORMAT2)?"FORMAT2":"FORMAT1");
    delete lsh;
    lsh = new LSH(indexName, true);
  }
  
  delete[] indexName;
  return true;
}

// *Static* approximate NN point reporter callback method for lshlib
void audioDB::index_add_point_approximate(void* instancePtr, Uns32T pointID, Uns32T qpos, float dist){
  assert(instancePtr); // We need an instance for this callback
  audioDB* myself = (audioDB*) instancePtr; // Use explicit cast to recover "this" instance
  Uns32T trackID = index_to_trackID(pointID);
  Uns32T spos = index_to_trackPos(pointID);
  // Skip identity in query_from_key
  if( !myself->query_from_key || (myself->query_from_key && ( trackID != myself->query_from_key_index )) )
    myself->reporter->add_point(trackID, qpos, spos, dist);
}

// *Static* exact NN point reporter callback method for lshlib
// Maintain a queue of points to pass to query_points() for exact evaluation
void audioDB::index_add_point_exact(void* instancePtr, Uns32T pointID, Uns32T qpos, float dist){
  assert(instancePtr); // We need an instance for this callback
  audioDB* myself = (audioDB*) instancePtr; // Use explicit cast to recover "this" instance  
  Uns32T trackID = index_to_trackID(pointID);
  Uns32T spos = index_to_trackPos(pointID);
  // Skip identity in query_from_key
  if( !myself->query_from_key || (myself->query_from_key && ( trackID != myself->query_from_key_index )) )
    myself->index_insert_exact_evaluation_queue(trackID, qpos, spos);
}

void audioDB::initialize_exact_evalutation_queue(){
  if(exact_evaluation_queue)
    delete exact_evaluation_queue;
  exact_evaluation_queue = new priority_queue<PointPair, std::vector<PointPair>, std::less<PointPair> >;
}

void audioDB::index_insert_exact_evaluation_queue(Uns32T trackID, Uns32T qpos, Uns32T spos){
  PointPair p(trackID, qpos, spos);
  exact_evaluation_queue->push(p);
}

// return 0: if index does not exist
// return nqv: if index exists
int audioDB::index_query_loop(const char* dbName, Uns32T queryIndex) {
  
  unsigned int numVectors;
  double *query, *query_data;
  double *qNorm, *qnPtr, *qPower = 0, *qpPtr = 0;
  double meanQdur;
  void (*add_point_func)(void*,Uns32T,Uns32T,float);

  // Set the point-reporter callback based on the value of lsh_exact
  if(lsh_exact){
    initialize_exact_evalutation_queue();
    add_point_func = &index_add_point_exact;
  }
  else
    add_point_func = &index_add_point_approximate;  

  if(!index_init_query(dbName)) // sets-up LSH index structures for querying
    return 0;

  char* database = index_get_name(dbName, radius, sequenceLength);

  if(query_from_key)
    set_up_query_from_key(&query_data, &query, &qNorm, &qnPtr, &qPower, &qpPtr, &meanQdur, &numVectors, queryIndex);
  else
    set_up_query(&query_data, &query, &qNorm, &qnPtr, &qPower, &qpPtr, &meanQdur, &numVectors); // get query vectors

  VERB_LOG(1, "retrieving tracks...");
  
  assert(pointNN>0 && pointNN<=O2_MAXNN);
  assert(trackNN>0 && trackNN<=O2_MAXNN);

  gettimeofday(&tv1, NULL);   
  // query vector index
  Uns32T Nq = (numVectors>O2_MAXTRACKLEN?O2_MAXTRACKLEN:numVectors) - sequenceLength + 1;
  vv = index_initialize_shingles(Nq); // allocate memory to copy query vectors to shingles
  cout << "Nq=" << Nq;   cout.flush();
  // Construct shingles from query features  
  for( Uns32T pointID = 0 ; pointID < Nq ; pointID++ ) 
    index_make_shingle(vv, pointID, query, dbH->dim, sequenceLength);
  
  // Normalize query vectors
  Uns32T numVecsAboveThreshold = index_norm_shingles( vv, qnPtr, qpPtr );
  cout << " Nq'=" << numVecsAboveThreshold << endl; cout.flush();

  // Nq contains number of inspected points in query file, 
  // numVecsAboveThreshold is number of points with power >= absolute_threshold
  double* qpp = qpPtr; // Keep original qpPtr for possible exact evaluation
  if(usingQueryPoint && numVecsAboveThreshold){
    if((lsh->get_lshHeader()->flags&O2_SERIAL_FILEFORMAT2) || lsh_in_core)
      lsh->retrieve_point((*vv)[0], queryPoint, add_point_func, (void*)this);
    else
      lsh->serial_retrieve_point(database, (*vv)[0], queryPoint, add_point_func, (void*)this);
  }
  else if(numVecsAboveThreshold)
    for( Uns32T pointID = 0 ; pointID < Nq; pointID++ )
      if(!use_absolute_threshold || (use_absolute_threshold && (*qpp++ >= absolute_threshold)))
	if((lsh->get_lshHeader()->flags&O2_SERIAL_FILEFORMAT2) || lsh_in_core)
	  lsh->retrieve_point((*vv)[pointID], pointID, add_point_func, (void*)this);
	else
	  lsh->serial_retrieve_point(database, (*vv)[pointID], pointID, add_point_func, (void*)this);   

  if(lsh_exact)
    // Perform exact distance computation on point pairs in exact_evaluation_queue
    query_loop_points(query, qnPtr, qpPtr, meanQdur, numVectors); 
  
  gettimeofday(&tv2,NULL);
  VERB_LOG(1,"elapsed time: %ld msec\n",
           (tv2.tv_sec*1000 + tv2.tv_usec/1000) - 
           (tv1.tv_sec*1000 + tv1.tv_usec/1000))

  // Close the index file
  close(lshfid);
    
 // Clean up
  if(query_data)
    delete[] query_data;
  if(qNorm)
    delete[] qNorm;
  if(qPower)
    delete[] qPower;
  if(database)
    delete[] database;

  return Nq;
}

