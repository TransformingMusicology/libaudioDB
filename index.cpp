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

/*******  LSH indexing audioDB database access forall s \in {S} *******/

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
