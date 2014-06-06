#include "audioDB.h"
#include "reporter.h"

#include <gsl/gsl_sf.h>

char* SERVER_ADB_ROOT;
char* SERVER_ADB_FEATURE_ROOT;

audioDB::audioDB(const unsigned argc, const char *argv[]): O2_AUDIODB_INITIALIZERS
{
  if(processArgs(argc, argv)<0){
    printf("No command found.\n");
    cmdline_parser_print_version ();
    if (strlen(gengetopt_args_info_purpose) > 0)
      printf("%s\n", gengetopt_args_info_purpose);
    printf("%s\n", gengetopt_args_info_usage);
    printf("%s\n", gengetopt_args_info_help[1]);
    printf("%s\n", gengetopt_args_info_help[2]);
    printf("%s\n", gengetopt_args_info_help[0]);
    error("No command found");
  }

  // Perform database prefix substitution
  if(dbName && adb_root)
    prefix_name((char** const)&dbName, adb_root);

  if(O2_ACTION(COM_SERVER)){
    startServer();
  }
  else  if(O2_ACTION(COM_CREATE))
    create(dbName);

  else if(O2_ACTION(COM_INSERT))
    insert(dbName, inFile);

  else if(O2_ACTION(COM_BATCHINSERT))
    batchinsert(dbName, inFile);

  else if(O2_ACTION(COM_QUERY))
    if(isClient){
      if(query_from_key){
	VERB_LOG(1, "Calling web services query %s on database %s, query=%s\n", radius>0?"(Radius)":"(NN)", dbName, (key&&strlen(key))?key:inFile);
	ws_query_by_key(dbName, key, inFile, (char*)hostport);	
      }
      else{
	VERB_LOG(1, "Calling web services query on database %s, query=%s\n", dbName, (key&&strlen(key))?key:inFile);
	ws_query(dbName, inFile, (char*)hostport);
      }
    }
    else
      query(dbName, inFile);

  else if(O2_ACTION(COM_STATUS))
    if(isClient)
      ws_status(dbName,(char*)hostport);
    else
      status(dbName);

  else if(O2_ACTION(COM_SAMPLE))
    sample(dbName);
  
  else if(O2_ACTION(COM_L2NORM))
    l2norm(dbName);
  
  else if(O2_ACTION(COM_POWER))
    power_flag(dbName);

  else if(O2_ACTION(COM_DUMP))
    dump(dbName);

  else if(O2_ACTION(COM_LISZT))
    if(isClient)
      ws_liszt(dbName, (char*) hostport);
    else
      liszt(dbName, lisztOffset, lisztLength);

  else if(O2_ACTION(COM_INDEX))
    index_index_db(dbName);
  
  else
    error("Unrecognized command",command);
}

audioDB::audioDB(const unsigned argc, const char *argv[], struct soap *soap, adb__queryResponse *adbQueryResponse): O2_AUDIODB_INITIALIZERS
{
  try {
    isServer = 1; // Set to make errors report over SOAP
    processArgs(argc, argv);
    // Perform database prefix substitution
    if(dbName && adb_root)
      prefix_name((char** const)&dbName, adb_root);
    assert(O2_ACTION(COM_QUERY));
    query(dbName, inFile, soap, adbQueryResponse);
  } catch(char *err) {
    cleanup();
    throw(err);
  }
}

audioDB::audioDB(const unsigned argc, const char *argv[], adb__statusResponse *adbStatusResponse): O2_AUDIODB_INITIALIZERS
{
  try {
    isServer = 1; // Set to make errors report over SOAP
    processArgs(argc, argv);
    // Perform database prefix substitution
    if(dbName && adb_root)
      prefix_name((char** const)&dbName, adb_root);
    assert(O2_ACTION(COM_STATUS));
    status(dbName, adbStatusResponse);
  } catch(char *err) {
    cleanup();
    throw(err);
  }
}

audioDB::audioDB(const unsigned argc, const char *argv[], struct soap *soap, adb__lisztResponse *adbLisztResponse): O2_AUDIODB_INITIALIZERS
{
  try {
    isServer = 1; // Set to make errors report over SOAP
    processArgs(argc, argv); 
    // Perform database prefix substitution
    if(dbName && adb_root)
      prefix_name((char** const)&dbName, adb_root);
    assert(O2_ACTION(COM_LISZT));
    liszt(dbName, lisztOffset, lisztLength, soap, adbLisztResponse);
  } catch(char *err) {
    cleanup();
    throw(err);
  }
}

void audioDB::cleanup() {
  cmdline_parser_free(&args_info);
  if(fileTable)
    munmap(fileTable, fileTableLength);
  if(trackTable)
    munmap(trackTable, trackTableLength);
  if(timesTable)
    munmap(timesTable, timesTableLength);
  if(powerTable)
    munmap(powerTable, powerTableLength);
  if(l2normTable)
    munmap(l2normTable, l2normTableLength);
  if(featureFileNameTable)
    munmap(featureFileNameTable, fileTableLength);
  if(timesFileNameTable)
    munmap(timesFileNameTable, fileTableLength);
  if(powerFileNameTable)
    munmap(powerFileNameTable, fileTableLength);
  if(reporter)
    delete reporter;
  if(infid>0) {
    close(infid);
    infid = 0;
  }
  if(powerfd) {
    close(powerfd);
    powerfd = 0;
  }
  if(timesFile) {
    delete timesFile;
    timesFile = 0;
  }
  if(adb) {
    audiodb_close(adb);
    adb = NULL;
  }
  if(lsh)
    delete lsh;
}

audioDB::~audioDB(){
  cleanup();
}

int audioDB::processArgs(const unsigned argc, const char *argv[]){

  /* KLUDGE: gengetopt generates a function which is not completely
     const-clean in its declaration.  We cast argv here to keep the
     compiler happy.  -- CSR, 2008-10-08 */
  if (cmdline_parser (argc, (char **) argv, &args_info) != 0)
    error("Error parsing command line");
    
  if(argc<2){
    cmdline_parser_print_version ();
    if (strlen(gengetopt_args_info_purpose) > 0)
      printf("%s\n", gengetopt_args_info_purpose);
    printf("%s\n", gengetopt_args_info_usage);
    printf("%s\n", gengetopt_args_info_help[1]);
    printf("%s\n", gengetopt_args_info_help[2]);
    printf("%s\n", gengetopt_args_info_help[0]);
    exit(0);
  }

  if(args_info.help_given){
    cmdline_parser_print_help();
    exit(0);
  }

  if(args_info.verbosity_given){
    verbosity = args_info.verbosity_arg;
    if(verbosity < 0 || verbosity > 10){
      std::cerr << "Warning: verbosity out of range, setting to 1" << std::endl;
      verbosity = 1;
    }
  }

  if(args_info.size_given) {
    if(args_info.datasize_given) {
      error("both --size and --datasize given", "");
    }
    if(args_info.ntracks_given) {
      error("both --size and --ntracks given", "");
    }
    if(args_info.datadim_given) {
      error("both --size and --datadim given", "");
    }
    if (args_info.size_arg < 50 || args_info.size_arg > 32000) {
      error("Size out of range", "");
    }
    double ratio = (double) args_info.size_arg * 1000000 / ((double) O2_DEFAULTDBSIZE);
    /* FIXME: what's the safe way of doing this? */
    datasize = (unsigned int) ceil(datasize * ratio);
    ntracks = (unsigned int) ceil(ntracks * ratio);
  } else {
    if(args_info.datasize_given) {
      datasize = args_info.datasize_arg;
    }
    if(args_info.ntracks_given) {
      ntracks = args_info.ntracks_arg;
    }
    if(args_info.datadim_given) {
      datadim = args_info.datadim_arg;
    }
  }

  if(args_info.radius_given) {
    radius = args_info.radius_arg;
    if(radius < 0 || radius > 1000000000) {
      error("radius out of range");
    } else {
      VERB_LOG(3, "Setting radius to %f\n", radius);
    }
  }
  
  sequenceLength = args_info.sequencelength_arg;
  if(sequenceLength < 1 || sequenceLength > 1000) {
    error("seqlen out of range: 1 <= seqlen <= 1000");
  }
  sequenceHop = args_info.sequencehop_arg;
  if(sequenceHop < 1 || sequenceHop > 1000) {
    error("seqhop out of range: 1 <= seqhop <= 1000");
  }
  
  if (args_info.absolute_threshold_given) {
    if (args_info.absolute_threshold_arg >= 0) {
      error("absolute threshold out of range: should be negative");
    }
    use_absolute_threshold = true;
    absolute_threshold = args_info.absolute_threshold_arg;
  }
  if (args_info.relative_threshold_given) {
    use_relative_threshold = true;
    relative_threshold = args_info.relative_threshold_arg;
  }

  if (args_info.rotate_given) {
    use_rotate = true;
    if (args_info.rotate_arg < -1) {
      error("rotate out of range: should be -1 or greater");
    }
    rotate = args_info.rotate_arg;
  }

  if (args_info.adb_root_given){
    adb_root = args_info.adb_root_arg;
  }

  if (args_info.adb_feature_root_given){
    adb_feature_root = args_info.adb_feature_root_arg;
  }

  // perform dbName path prefix SERVER-side subsitution
  if(SERVER_ADB_ROOT && !adb_root)
    adb_root = SERVER_ADB_ROOT;
  if(SERVER_ADB_FEATURE_ROOT && !adb_feature_root)
    adb_feature_root = SERVER_ADB_FEATURE_ROOT;

  if(args_info.SERVER_given){
    command=COM_SERVER;
    port=args_info.SERVER_arg;
    if(port<100 || port > 100000)
      error("port out of range");
#if defined(O2_DEBUG)
    struct sigaction sa;
    sa.sa_sigaction = sigterm_action;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_sigaction = sighup_action;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    sigaction(SIGHUP, &sa, NULL);
#endif
    if(args_info.load_index_given){
      if(!args_info.database_given)
	error("load_index requires a --database argument");
      else
	dbName=args_info.database_arg;
      if(!args_info.radius_given)
	error("load_index requires a --radius argument");
      if(!args_info.sequencelength_given)
	error("load_index requires a --sequenceLength argument");
      WS_load_index = true;
    }
    return 0;
  }

  // No return on client command, find database command
  if(args_info.client_given){
    command=COM_CLIENT;
    hostport=args_info.client_arg;
    isClient=1;
  }

  if(args_info.NEW_given){
    command=COM_CREATE;
    dbName=args_info.database_arg;
    return 0;
  }

  if(args_info.STATUS_given){
    command=COM_STATUS;
    dbName=args_info.database_arg;
    return 0;
  }

  if(args_info.SAMPLE_given) {
    command = COM_SAMPLE;
    dbName = args_info.database_arg;
    sequenceLength = args_info.sequencelength_arg;
    if(sequenceLength < 1 || sequenceLength > 1000) {
      error("seqlen out of range: 1 <= seqlen <= 1000");
    }
    if(args_info.nsamples_given) {
      nsamples = args_info.nsamples_arg;
    } else if(args_info.resultlength_given) {
      nsamples = args_info.resultlength_arg;
    } else {
      nsamples = args_info.nsamples_arg;
    }
    if(args_info.key_given) {
      query_from_key = true;
      key = args_info.key_arg;
    } else if (args_info.features_given) {
      inFile = args_info.features_arg;
    }
    if(!args_info.exhaustive_flag){
      queryPoint = args_info.qpoint_arg;
      usingQueryPoint=1;
      if(queryPoint<0 || queryPoint >O2_MAX_VECTORS)
        error("queryPoint out of range: 0 <= queryPoint <= O2_MAX_VECTORS");
    }


    return 0;
  }

  if(args_info.DUMP_given){
    command=COM_DUMP;
    dbName=args_info.database_arg;
    output = args_info.output_arg;
    return 0;
  }

  if(args_info.L2NORM_given){
    command=COM_L2NORM;
    dbName=args_info.database_arg;
    return 0;
  }
       
  if(args_info.POWER_given){
    command=COM_POWER;
    dbName=args_info.database_arg;
    return 0;
  }
       
  if(args_info.INSERT_given) {
    command=COM_INSERT;
    dbName=args_info.database_arg;
    inFile=args_info.features_arg;
    if(args_info.key_given) {
      if(!args_info.features_given) {
	error("INSERT: '-k key' argument depends on '-f features'");
      } else {
	key=args_info.key_arg;
      }
    }
    if(args_info.times_given) {
      timesFileName=args_info.times_arg;
      if(strlen(timesFileName)>0) {
        if(!(timesFile = new std::ifstream(timesFileName,std::ios::in))) {
          error("Could not open times file for reading", timesFileName);
	}
        usingTimes=1;
      }
    }
    if (args_info.power_given) {
      powerFileName = args_info.power_arg;
      if (strlen(powerFileName) > 0) {
        if (!(powerfd = open(powerFileName, O_RDONLY))) {
          error("Could not open power file for reading", powerFileName, "open");
        }
        usingPower = 1;
      }
    }    
    return 0;
  }
  
  if(args_info.BATCHINSERT_given) {
    command=COM_BATCHINSERT;
    dbName=args_info.database_arg;
    inFile=args_info.featureList_arg;
    if(args_info.keyList_given) {
      if(!args_info.featureList_given) {
	error("BATCHINSERT: '-K keyList' argument depends on '-F featureList'");
      } else {
	key=args_info.keyList_arg;     // INCONSISTENT NO CHECK
      }
    }
    /* TO DO: REPLACE WITH
      if(args_info.keyList_given){
      trackFileName=args_info.keyList_arg;
      if(strlen(trackFileName)>0 && !(trackFile = new std::ifstream(trackFileName,std::ios::in)))
      error("Could not open keyList file for reading",trackFileName);
      }
      AND UPDATE BATCHINSERT()
    */
    
    if(args_info.timesList_given) {
      timesFileName=args_info.timesList_arg;
      if(strlen(timesFileName)>0) {
        if(!(timesFile = new std::ifstream(timesFileName,std::ios::in)))
          error("Could not open timesList file for reading", timesFileName);
        usingTimes=1;
      }
    }
    if(args_info.powerList_given) {
      powerFileName=args_info.powerList_arg;
      if(strlen(powerFileName)>0) {
        if(!(powerFile = new std::ifstream(powerFileName,std::ios::in)))
          error("Could not open powerList file for reading", powerFileName);
        usingPower=1;
      }
    }
    return 0;
  }

  // Set no_unit_norm flag  
  distance_kullback = args_info.distance_kullback_flag;
  no_unit_norming = args_info.no_unit_norming_flag;
  lsh_use_u_functions = args_info.lsh_use_u_functions_flag;

  // LSH Index Command
  if(args_info.INDEX_given){
    if(radius <= 0 )
      error("INDEXing requires a Radius argument");
    if(!(sequenceLength>0 && sequenceLength <= O2_MAXSEQLEN))
      error("INDEXing requires 1 <= sequenceLength <= 1000");
    command=COM_INDEX;
    if(!args_info.database_given)
      error("INDEXing requires a database");
    dbName=args_info.database_arg;

    // Whether to store LSH hash tables for query in core (FORMAT2)
    lsh_in_core = !args_info.lsh_on_disk_flag; // This flag is set to 0 if on_disk requested

    lsh_param_w = args_info.lsh_w_arg;
    if(!(lsh_param_w>0 && lsh_param_w<=O2_SERIAL_MAX_BINWIDTH))
      error("Indexing parameter w out of range (0.0 < w <= 100.0)");

    lsh_param_k = args_info.lsh_k_arg;      
    if(!(lsh_param_k>0 && lsh_param_k<=O2_SERIAL_MAX_FUNS))
      error("Indexing parameter k out of range (1 <= k <= 100)");

    lsh_param_m = args_info.lsh_m_arg;
    if(!(lsh_param_m>0 && lsh_param_m<= (1 + (sqrt(1 + O2_SERIAL_MAX_TABLES*8.0)))/2.0))
      error("Indexing parameter m out of range (1 <= m <= 20)");

    lsh_param_N = args_info.lsh_N_arg;    
    if(!(lsh_param_N>0 && lsh_param_N<=O2_SERIAL_MAX_ROWS))
      error("Indexing parameter N out of range (1 <= N <= 1000000)");
    
    lsh_param_b = args_info.lsh_b_arg;
    if(!(lsh_param_b>0 && lsh_param_b<=O2_SERIAL_MAX_TRACKBATCH))
      error("Indexing parameter b out of range (1 <= b <= 10000)");
    
    lsh_param_ncols = args_info.lsh_ncols_arg;    
    if(lsh_in_core) // We don't want to block rows with FORMAT2 indexing
      lsh_param_ncols = O2_SERIAL_MAX_COLS;
    if( !(lsh_param_ncols>0 && lsh_param_ncols<=O2_SERIAL_MAX_COLS))
      error("Indexing parameter ncols out of range (1 <= ncols <= 1000");

    return 0;
  }

  // Query command and arguments
  if(args_info.QUERY_given){
    command=COM_QUERY;
    dbName=args_info.database_arg;
    // XOR features and key search
    if((!args_info.features_given && !args_info.key_given) || (args_info.features_given && args_info.key_given))
      error("QUERY requires exactly one of either -f features or -k key");
    if(args_info.features_given)
      inFile=args_info.features_arg; // query from file
    else{
      query_from_key = true;
      key=args_info.key_arg;      // query from key
    }

    if(args_info.keyList_given){
      trackFileName=args_info.keyList_arg;
      if(strlen(trackFileName)>0 && !(trackFile = new std::ifstream(trackFileName,std::ios::in)))
        error("Could not open keyList file for reading",trackFileName);
    }
    
    if(args_info.times_given){
      timesFileName=args_info.times_arg;
      if(strlen(timesFileName)>0){
        if(!(timesFile = new std::ifstream(timesFileName,std::ios::in)))
          error("Could not open times file for reading", timesFileName);
        usingTimes=1;
      }
    }

    if(args_info.power_given){
      powerFileName=args_info.power_arg;
      if(strlen(powerFileName)>0){
        if (!(powerfd = open(powerFileName, O_RDONLY))) {
          error("Could not open power file for reading", powerFileName, "open");
        }
        usingPower = 1;
      }
    }
    
    // query type
    if(strncmp(args_info.QUERY_arg, "track", MAXSTR)==0)
      queryType=O2_TRACK_QUERY;
    else if(strncmp(args_info.QUERY_arg, "point", MAXSTR)==0)
      queryType=O2_POINT_QUERY;
    else if(strncmp(args_info.QUERY_arg, "sequence", MAXSTR)==0)
      queryType=O2_SEQUENCE_QUERY;
    else if(strncmp(args_info.QUERY_arg, "nsequence", MAXSTR)==0)
      queryType=O2_N_SEQUENCE_QUERY;
    else if(strncmp(args_info.QUERY_arg, "onetoonensequence", MAXSTR)==0)
      queryType=O2_ONE_TO_ONE_N_SEQUENCE_QUERY;
    else
      error("unsupported query type",args_info.QUERY_arg);
    
    if(!args_info.exhaustive_flag){
      queryPoint = args_info.qpoint_arg;
      usingQueryPoint=1;
      if(queryPoint<0 || queryPoint >O2_MAX_VECTORS)
        error("queryPoint out of range: 0 <= queryPoint <= O2_MAX_VECTORS");
    }

    // Whether to pre-load LSH hash tables for query (default on, if flag set then off)
    lsh_in_core = !args_info.lsh_on_disk_flag;

    // Whether to perform exact evaluation of points returned by LSH
    lsh_exact = args_info.lsh_exact_flag;

    pointNN = args_info.pointnn_arg;
    if(pointNN < 1 || pointNN > O2_MAXNN) {
      error("pointNN out of range: 1 <= pointNN <= 1000000");
    }
    trackNN = args_info.resultlength_arg;
    if(trackNN < 1 || trackNN > O2_MAXNN) {
      error("resultlength out of range: 1 <= resultlength <= 1000000");
    }
    return 0;
  }
  
  if(args_info.LISZT_given){
    command = COM_LISZT;
    dbName=args_info.database_arg;
    lisztOffset = args_info.lisztOffset_arg;
    lisztLength = args_info.lisztLength_arg;
    if(args_info.lisztOffset_arg<0) // check upper bound later when database is opened
      error("lisztOffset cannot be negative");
    if(args_info.lisztLength_arg<0)
      error("lisztLength cannot be negative");
    if(lisztLength >1000000)
      error("lisztLength too large (>1000000)");
    return 0;
  }
  
  return -1; // no command found
}

void audioDB::status(const char* dbName, adb__statusResponse *adbStatusResponse){
  adb_status_t status;
  if(!adb) {
    if(!(adb = audiodb_open(dbName, O_RDONLY))) {
      error("Failed to open database file", dbName);
    }
  }
  if(audiodb_status(adb, &status)) {
    error("Failed to retrieve database status", dbName);
  }
  
  if(adbStatusResponse == 0) {
    std::cout << "num files:" << status.numFiles << std::endl;
    std::cout << "data dim:" << status.dim <<std::endl;
    if(status.dim > 0) {
      size_t bytes_per_vector = sizeof(double) * status.dim;
      off_t nvectors = status.length / bytes_per_vector;
      off_t data_region_vectors = status.data_region_size / bytes_per_vector;
      std::cout << "total vectors:" << nvectors << std::endl;
      std::cout << "vectors available:";
      if(status.flags & O2_FLAG_LARGE_ADB) {
	std::cout << O2_MAX_VECTORS - nvectors << std::endl;
      } else {
	std::cout << data_region_vectors - nvectors << std::endl;
      }
    }
    if(!(status.flags & O2_FLAG_LARGE_ADB)) {
      double used_frac = ((double) status.length) / status.data_region_size;
      std::cout << "total bytes:" << status.length << 
	" (" << (100.0*used_frac) << "%)" << std::endl;
      std::cout << "bytes available:" << status.data_region_size - status.length << 
	" (" << (100.0*(1-used_frac)) << "%)" << std::endl;
    }
    std::cout << "flags:" << " l2norm[" << DISPLAY_FLAG(status.flags&O2_FLAG_L2NORM)
	      << "] minmax[" << DISPLAY_FLAG(status.flags&O2_FLAG_MINMAX)
	      << "] power[" << DISPLAY_FLAG(status.flags&O2_FLAG_POWER)
	      << "] times[" << DISPLAY_FLAG(status.flags&O2_FLAG_TIMES) 
	      << "] largeADB[" << DISPLAY_FLAG(status.flags&O2_FLAG_LARGE_ADB)
	      << "]" << endl;    
              
    std::cout << "null count: " << status.nullCount << " small sequence count " << status.dudCount-status.nullCount << std::endl;    
  } else {
    adbStatusResponse->result.numFiles = status.numFiles;
    adbStatusResponse->result.dim = status.dim;
    adbStatusResponse->result.length = status.length;
    adbStatusResponse->result.dudCount = status.dudCount;
    adbStatusResponse->result.nullCount = status.nullCount;
    adbStatusResponse->result.flags = status.flags;
  }
}

void audioDB::l2norm(const char* dbName) {
  if(!adb) {
    if(!(adb = audiodb_open(dbName, O_RDWR))) {
      error("Failed to open database file", dbName);
    }
  }
  if(audiodb_l2norm(adb)) {
    error("failed to turn on l2norm flag for database", dbName);
  }
}

void audioDB::power_flag(const char *dbName) {
  if(!adb) {
    if(!(adb = audiodb_open(dbName, O_RDWR))) {
      error("Failed to open database file", dbName);
    }
  }
  if(audiodb_power(adb)) {
    error("can't turn on power flag for database", dbName);
  }
}

void audioDB::create(const char *dbName) {
  if(adb) {
    error("Already have an adb in this object", "");
  }
  if(!(adb = audiodb_create(dbName, datasize, ntracks, datadim))) {
    error("Failed to create database file", dbName);
  }
}

void audioDB::dump(const char *dbName) {
  if(!adb) {
    if(!(adb = audiodb_open(dbName, O_RDONLY))) {
      error("Failed to open database file", dbName);
    }
  }
  if(audiodb_dump(adb, output)) {
    error("Failed to dump database to ", output);
  }
  status(dbName);
}

void audioDB::insert(const char* dbName, const char* inFile) {
  if(!adb) {
    if(!(adb = audiodb_open(dbName, O_RDWR))) {
      error("failed to open database", dbName);
    }
  }

  /* at this point, we have powerfd (an fd), timesFile (a
   * std::ifstream *) and inFile (a char *).  Wacky, huh?  Ignore
   * the wackiness and just use the names. */
  adb_insert_t insert;
  insert.features = inFile;
  insert.times = timesFileName;
  insert.power = powerFileName;
  insert.key = key;

  if(audiodb_insert(adb, &insert)) {
    error("insertion failure", inFile);
  }
  status(dbName);
}

void audioDB::batchinsert(const char* dbName, const char* inFile) {
  if(!adb) {
    if(!(adb = audiodb_open(dbName, O_RDWR))) {
      error("failed to open database", dbName);
    }
  }

  if(!key)
    key=inFile;
  std::ifstream *filesIn = 0;
  std::ifstream *keysIn = 0;

  if(!(filesIn = new std::ifstream(inFile)))
    error("Could not open batch in file", inFile);
  if(key && key!=inFile)
    if(!(keysIn = new std::ifstream(key)))
      error("Could not open batch key file",key);

  unsigned totalVectors=0;
  char *thisFile = new char[MAXSTR];
  char *thisKey = 0;
  if (key && (key != inFile)) {
    thisKey = new char[MAXSTR];
  }
  char *thisTimesFileName = new char[MAXSTR];
  char *thisPowerFileName = new char[MAXSTR];

  do {
    filesIn->getline(thisFile,MAXSTR);
    if(key && key!=inFile) {
      keysIn->getline(thisKey,MAXSTR);
    } else {
      thisKey = thisFile;
    }
    if(usingTimes) {
      timesFile->getline(thisTimesFileName,MAXSTR);
    }
    if(usingPower) {
      powerFile->getline(thisPowerFileName, MAXSTR);
    }
    
    if(filesIn->eof()) {
      break;
    }
    if(usingTimes){
      if(timesFile->eof()) {
        error("not enough timestamp files in timesList", timesFileName);
      }
    }
    if (usingPower) {
      if(powerFile->eof()) {
        error("not enough power files in powerList", powerFileName);
      }
    }
    adb_insert_t insert;
    insert.features = thisFile;
    insert.times = usingTimes ? thisTimesFileName : NULL;
    insert.power = usingPower ? thisPowerFileName : NULL;
    insert.key = thisKey;
    if(audiodb_insert(adb, &insert)) {
      error("insertion failure", thisFile);
    }
  } while(!filesIn->eof());

  VERB_LOG(0, "%s %s %u vectors %ju bytes.\n", COM_BATCHINSERT, dbName, totalVectors, (intmax_t) (totalVectors * adb->header->dim * sizeof(double)));

  delete [] thisPowerFileName;
  if(key && (key != inFile)) {
    delete [] thisKey;
  }
  delete [] thisFile;
  delete [] thisTimesFileName;
  
  delete filesIn;
  delete keysIn;

  // Report status
  status(dbName);
}

void audioDB::datumFromFiles(adb_datum_t *datum) {
  int fd;
  struct stat st;

  /* FIXME: around here error conditions will cause all sorts of
     hideous leaks. */
  fd = open(inFile, O_RDONLY);
  if(fd < 0) {
    error("failed to open feature file", inFile);
  }
  fstat(fd, &st);
  read(fd, &(datum->dim), sizeof(uint32_t));
  datum->nvectors = (st.st_size - sizeof(uint32_t)) / (datum->dim * sizeof(double));
  datum->data = (double *) malloc(st.st_size - sizeof(uint32_t));
  read(fd, datum->data, st.st_size - sizeof(uint32_t));
  close(fd);
  if(usingPower) {
    uint32_t one;
    fd = open(powerFileName, O_RDONLY);
    if(fd < 0) {
      error("failed to open power file", powerFileName);
    }
    read(fd, &one, sizeof(uint32_t));
    if(one != 1) {
      error("malformed power file dimensionality", powerFileName);
    }
    datum->power = (double *) malloc(datum->nvectors * sizeof(double));
    if(read(fd, datum->power, datum->nvectors * sizeof(double)) != (ssize_t) (datum->nvectors * sizeof(double))) {
      error("malformed power file", powerFileName);
    }
    close(fd);
  }
  if(usingTimes) {
    datum->times = (double *) malloc(2 * datum->nvectors * sizeof(double));
    insertTimeStamps(datum->nvectors, timesFile, datum->times);
  }
}

void audioDB::rotateDatum(adb_datum_t *datum, int amount) {
  printf("amount: %d\n", amount);
  printf("dim: %d\n", datum->dim);
  if (!datum->data)
    error("no data in datum to be rotated");
  if (amount < - (int) (datum->dim))
    error("silly value of rotation amount");
  if (amount < 0)
    amount += datum->dim;

  amount = amount % datum->dim;
  printf("amount: %d\n", amount);

  double *buf = (double *) malloc(amount * sizeof(double));

  for (uint32_t i = 0; i < datum->nvectors; i++) {
    double *start = datum->data + i * datum->dim;
    memcpy(buf, start, amount * sizeof(double));
    memmove(start, start + amount, (datum->dim - amount) * sizeof(double));
    memcpy(start + (datum->dim - amount), buf, amount * sizeof(double));
  }

  free(buf);
}

void audioDB::query(const char* dbName, const char* inFile, struct soap *soap, adb__queryResponse *adbQueryResponse) {

  if(!adb) {
    if(!(adb = audiodb_open(dbName, O_RDONLY))) {
      error("failed to open database", dbName);
    }
  }

  /* FIXME: we only need this for getting nfiles, which we only need
   * because the reporters aren't desperately well implemented,
   * relying on statically-sized vectors rather than adjustable data
   * structures.  Rework reporter.h to be less lame. */
  adb_status_t status;
  audiodb_status(adb, &status);
  uint32_t nfiles = status.numFiles;

  adb_query_spec_t qspec;
  adb_datum_t datum = {0};

  qspec.refine.flags = 0;
  if(trackFile) {
    qspec.refine.flags |= ADB_REFINE_INCLUDE_KEYLIST;
    std::vector<const char *> v;
    char *k = new char[MAXSTR];
    trackFile->getline(k, MAXSTR);    
    while(!trackFile->eof()) {
      v.push_back(k);
      k = new char[MAXSTR];
      trackFile->getline(k, MAXSTR);    
    }
    delete [] k;
    qspec.refine.include.nkeys = v.size();
    qspec.refine.include.keys = new const char *[qspec.refine.include.nkeys];
    for(unsigned int k = 0; k < qspec.refine.include.nkeys; k++) {
      qspec.refine.include.keys[k] = v[k];
    }
  }
  if(query_from_key) {
    qspec.refine.flags |= ADB_REFINE_EXCLUDE_KEYLIST;
    qspec.refine.exclude.nkeys = 1;
    qspec.refine.exclude.keys = &key;
  }
  if(radius) {
    qspec.refine.flags |= ADB_REFINE_RADIUS;
    qspec.refine.radius = radius;
  }
  if(use_absolute_threshold) {
    qspec.refine.flags |= ADB_REFINE_ABSOLUTE_THRESHOLD;
    qspec.refine.absolute_threshold = absolute_threshold;
  }
  if(use_relative_threshold) {
    qspec.refine.flags |= ADB_REFINE_RELATIVE_THRESHOLD;
    qspec.refine.relative_threshold = relative_threshold;
  }
  if(usingTimes) {
    qspec.refine.flags |= ADB_REFINE_DURATION_RATIO;
    qspec.refine.duration_ratio = timesTol;
  }

  qspec.refine.qhopsize = sequenceHop;
  qspec.refine.ihopsize = sequenceHop;
  if(sequenceHop != 1) {
    qspec.refine.flags |= ADB_REFINE_HOP_SIZE;
  }

  if(query_from_key) {
    datum.key = key;
  } else {
    datumFromFiles(&datum);
  }

  qspec.qid.datum = &datum;
  qspec.qid.sequence_length = sequenceLength;
  qspec.qid.flags = 0;
  qspec.qid.flags |= usingQueryPoint ? 0 : ADB_QID_FLAG_EXHAUSTIVE;
  qspec.qid.flags |= lsh_exact ? 0 : ADB_QID_FLAG_ALLOW_FALSE_POSITIVES;
  qspec.qid.sequence_start = queryPoint;

  switch(queryType) {
  case O2_POINT_QUERY:
    qspec.qid.sequence_length = 1;
    qspec.params.accumulation = ADB_ACCUMULATION_DB;
    qspec.params.distance = ADB_DISTANCE_DOT_PRODUCT;
    qspec.params.npoints = pointNN;
    qspec.params.ntracks = 0;
    reporter = new pointQueryReporter< std::greater < NNresult > >(pointNN);
    break;
  case O2_TRACK_QUERY:
    qspec.qid.sequence_length = 1;
    qspec.params.accumulation = ADB_ACCUMULATION_PER_TRACK;
    qspec.params.distance = ADB_DISTANCE_DOT_PRODUCT;
    qspec.params.npoints = pointNN;
    qspec.params.ntracks = trackNN;
    reporter = new trackAveragingReporter< std::greater< NNresult > >(pointNN, trackNN, nfiles);
    break;
  case O2_SEQUENCE_QUERY:
  case O2_N_SEQUENCE_QUERY:
    qspec.params.accumulation = ADB_ACCUMULATION_PER_TRACK;
    if (distance_kullback)
      qspec.params.distance = ADB_DISTANCE_KULLBACK_LEIBLER_DIVERGENCE;
    else
      qspec.params.distance = no_unit_norming ? ADB_DISTANCE_EUCLIDEAN : ADB_DISTANCE_EUCLIDEAN_NORMED;
    qspec.params.npoints = pointNN;
    qspec.params.ntracks = trackNN;
    switch(queryType) {
    case O2_SEQUENCE_QUERY:
      if(!(qspec.refine.flags & ADB_REFINE_RADIUS)) {
        reporter = new trackAveragingReporter< std::less< NNresult > >(pointNN, trackNN, nfiles);
      } else {
	reporter = new trackSequenceQueryRadReporter(trackNN, nfiles);
      }
      break;
    case O2_N_SEQUENCE_QUERY:
      if(!(qspec.refine.flags & ADB_REFINE_RADIUS)) {
        reporter = new trackSequenceQueryNNReporter< std::less < NNresult > >(pointNN, trackNN, nfiles);
      } else {
	reporter = new trackSequenceQueryRadNNReporter(pointNN, trackNN, nfiles);
      }
      break;
    }
    break;
  case O2_ONE_TO_ONE_N_SEQUENCE_QUERY:
    qspec.params.accumulation = ADB_ACCUMULATION_ONE_TO_ONE;
    if (distance_kullback)
      qspec.params.distance = ADB_DISTANCE_KULLBACK_LEIBLER_DIVERGENCE;
    else
      qspec.params.distance = no_unit_norming ? ADB_DISTANCE_EUCLIDEAN : ADB_DISTANCE_EUCLIDEAN_NORMED;
    qspec.params.npoints = 0;
    qspec.params.ntracks = 0;
    if(!(qspec.refine.flags & ADB_REFINE_RADIUS)) {
      error("query-type not yet supported");
    } else {
      reporter = new trackSequenceQueryRadNNReporterOneToOne(pointNN,trackNN, adb->header->numFiles);
    }
    break;
  default:
    error("unrecognized queryType");
  }

  adb_query_results_t *rs = NULL;
  if(use_rotate) {
    int rotate_min = 0;
    int rotate_max = 0;
    if(rotate == -1) {
      adb_status_t s = {0};
      audiodb_status(adb, &s);
      rotate_min = 0;
      rotate_max = s.dim - 1;
    } else {
      rotate_min = -rotate;
      rotate_max = rotate;
    }
    adb_query_results_t *ors = NULL;
    if(query_from_key) {
      audiodb_retrieve_datum(adb, key, qspec.qid.datum);
    }
    rotateDatum(qspec.qid.datum, rotate_min);
    const char *const sentinel = "";
    for (int i = rotate_min; i <= rotate_max; i++) {
      ors = rs;
      rs = audiodb_query_spec_given_sofar(adb, &qspec, ors);
      if(ors) {
        audiodb_query_free_results(adb, &qspec, ors);
      }
      for(unsigned int k = 0; k < rs->nresults; k++) {
        adb_result_t r = rs->results[k];
        if (r.ikey != sentinel)
          reporter->add_point(audiodb_key_index(adb, r.ikey), r.qpos, r.ipos, r.dist, i);
      }
      for(uint32_t j = 0; j < rs->nresults; j++) {
        rs->results[j].ikey = sentinel;
      }
      rotateDatum(qspec.qid.datum, 1);
    }
  } else {
    rs = audiodb_query_spec(adb, &qspec);

    if(rs == NULL) {
      error("audiodb_query_spec failed");
    }

    for(unsigned int k = 0; k < rs->nresults; k++) {
      adb_result_t r = rs->results[k];
      reporter->add_point(audiodb_key_index(adb, r.ikey), r.qpos, r.ipos, r.dist);
    }
    audiodb_query_free_results(adb, &qspec, rs);
  }

  // FIXME: we don't yet free everything up if there are error
  // conditions during the construction of the query spec (including
  // the datum itself).
  if(datum.data) {
    free(datum.data);
    datum.data = NULL;
  }
  if(datum.power) {
    free(datum.power);
    datum.power = NULL;
  }
  if(datum.times) {
    free(datum.times);
    datum.times = NULL;
  }

  reporter->report(adb, soap, adbQueryResponse, use_rotate);
}

void audioDB::liszt(const char* dbName, unsigned offset, unsigned numLines, struct soap *soap, adb__lisztResponse* adbLisztResponse) {
  if(!adb) {
    if(!(adb = audiodb_open(dbName, O_RDONLY))) {
      error("failed to open database", dbName);
    }
  }

  adb_liszt_results_t *results = audiodb_liszt(adb);
  if(!results) {
    error("audiodb_liszt() failed");
  }

  if(offset > results->nresults) {
    audiodb_liszt_free_results(adb, results);
    error("listKeys offset out of range");
  }

  if(!adbLisztResponse){
    for(uint32_t k = 0; k < numLines && offset + k < results->nresults; k++) {
      uint32_t index = offset + k;
      printf("[%d] %s (%d)\n", index, results->entries[index].key, results->entries[index].nvectors);
    }
  } else {
    adbLisztResponse->result.Rkey = (char **) soap_malloc(soap, numLines * sizeof(char *));
    adbLisztResponse->result.Rlen = (unsigned int *) soap_malloc(soap, numLines * sizeof(unsigned int));
    uint32_t k;
    for(k = 0; k < numLines && offset + k < results->nresults; k++) {
      uint32_t index = offset + k;
      adbLisztResponse->result.Rkey[k] = (char *) soap_malloc(soap, O2_MAXFILESTR);
      snprintf(adbLisztResponse->result.Rkey[k], O2_MAXFILESTR, "%s", results->entries[index].key);
      adbLisztResponse->result.Rlen[k] = results->entries[index].nvectors;
    }
    adbLisztResponse->result.__sizeRkey = k;
    adbLisztResponse->result.__sizeRlen = k;
  }
  audiodb_liszt_free_results(adb, results);  
}

static
double yfun(double d) {
  return gsl_sf_log(d) - gsl_sf_psi(d);
}

static
double yinv(double y) {
  double a = 1.0e-5;
  double b = 1000.0;

  double ay = yfun(a);
  double by = yfun(b);

  double c = 0;
  double cy;

  /* FIXME: simple binary search; there's probably some clever solver
     in gsl somewhere which is less sucky. */
  while ((b - a) > 1.0e-5) {
    c = (a + b) / 2;
    cy = yfun(c);
    if (cy > y) {
      a = c;
      ay = cy;
    } else {
      b = c;
      by = cy;
    }
  }

  return c;
}

void audioDB::sample(const char *dbName) {
  if(!adb) {
    if(!(adb = audiodb_open(dbName, O_RDONLY))) {
      error("failed to open database", dbName);
    }
  }

  adb_status_t status;
  if(audiodb_status(adb, &status)) {
    error("error getting status");
  }

  double sumdist = 0;
  double sumlogdist = 0;

  adb_query_results_t *results;
  adb_query_spec_t spec = {{0},{0},{0}};
  adb_datum_t datum = {0};

  spec.refine.qhopsize = sequenceHop;
  spec.refine.ihopsize = sequenceHop;
  if(sequenceHop != 1) {
    spec.refine.flags |= ADB_REFINE_HOP_SIZE;
  }

  if(query_from_key) {
    datum.key = key;
    spec.qid.datum = &datum;
    spec.refine.flags |= ADB_REFINE_EXCLUDE_KEYLIST;
    spec.refine.exclude.nkeys = 1;
    spec.refine.exclude.keys = &key;
  } else if(inFile) {
    datumFromFiles(&datum);
    spec.qid.datum = &datum;
  } else {
    spec.qid.datum = NULL; /* full db sample */
  }
  spec.qid.sequence_length = sequenceLength;
  spec.qid.flags |= usingQueryPoint ? 0 : ADB_QID_FLAG_EXHAUSTIVE;
  spec.qid.sequence_start = queryPoint;
  if (distance_kullback)
    spec.params.distance = ADB_DISTANCE_KULLBACK_LEIBLER_DIVERGENCE;
  else
    spec.params.distance = no_unit_norming ? ADB_DISTANCE_EUCLIDEAN : ADB_DISTANCE_EUCLIDEAN_NORMED;
  spec.params.accumulation = ADB_ACCUMULATION_DB;
  spec.params.npoints = nsamples;

  if(!(results = audiodb_sample_spec(adb, &spec))) {
    error("error in audiodb_sample_spec");
  }

  if(datum.data) {
    free(datum.data);
    datum.data = NULL;
  }
  if(datum.power) {
    free(datum.power);
    datum.power = NULL;
  }
  if(datum.times) {
    free(datum.times);
    datum.times = NULL;
  }

  if(results->nresults != nsamples) {
    error("mismatch in sample count");
  }

  for(uint32_t i = 0; i < nsamples; i++) {
    double d = results->results[i].dist;
    sumdist += d;
    sumlogdist += log(d);
  }

  audiodb_query_free_results(adb, &spec, results);

  unsigned total = 0;
  unsigned count = 0;
  adb_liszt_results_t *liszt;
  if(!(liszt = audiodb_liszt(adb))) {
    error("liszt failed");
  }
  for(uint32_t i = 0; i < liszt->nresults; i++) {
    int prop = (liszt->entries[i].nvectors - sequenceLength)/sequenceHop + 1;
    prop = prop > 0 ? prop : 0;
    if (prop > 0) {
      count++;
    }
    total += prop;
  }
  audiodb_liszt_free_results(adb, liszt);

  /* FIXME: the mean isn't really what we should be using here; it's
     more a question of "how many independent sequences of length
     sequenceLength are there in the database? */
  unsigned meanN = total / count;

  double sigma2 = sumdist / (sequenceLength * status.dim * nsamples);
  double d = 2 * yinv(log(sumdist/nsamples) - sumlogdist/nsamples);

  std::cout << "Summary statistics" << std::endl;
  std::cout << "number of samples: " << nsamples << std::endl;
  std::cout << "sum of distances (S): " << sumdist << std::endl;
  std::cout << "sum of log distances (L): " << sumlogdist << std::endl;

  /* FIXME: we'll also want some more summary statistics based on
     propTable, for the minimum-of-X estimate */
  std::cout << "mean number of applicable sequences (N): " << meanN << std::endl;
  std::cout << std::endl;
  std::cout << "Estimated parameters" << std::endl;
  std::cout << "sigma^2: " << sigma2 << "; ";
  std::cout << "Msigma^2: " << sumdist / nsamples << std::endl;
  std::cout << "d: " << d << std::endl;

  double logw = (2 / d) * gsl_sf_log(-gsl_sf_log(0.99));
  double logxthresh = gsl_sf_log(sumdist / nsamples) + logw
    - (2 / d) * gsl_sf_log(meanN)
    - gsl_sf_log(d/2)
    - (2 / d) * gsl_sf_log(2 / d)
    + (2 / d) * gsl_sf_lngamma(d / 2);

  std::cout << "track xthresh: " << exp(logxthresh) << std::endl;
}


// This entry point is visited once per instance
// so it is a good place to set any global state variables
int main(const int argc, const char* argv[]){
  SERVER_ADB_ROOT = 0;            // Server-side database root prefix
  SERVER_ADB_FEATURE_ROOT = 0;    // Server-side features root prefix
  audioDB(argc, argv);
}
