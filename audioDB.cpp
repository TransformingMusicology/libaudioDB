#include "audioDB.h"

LSH* SERVER_LSH_INDEX_SINGLETON;
char* SERVER_ADB_ROOT;
char* SERVER_ADB_FEATURE_ROOT;

PointPair::PointPair(Uns32T a, Uns32T b, Uns32T c):trackID(a),qpos(b),spos(c){};

bool operator<(const PointPair& a, const PointPair& b){
  return ( (a.trackID<b.trackID) ||
	   ( (a.trackID==b.trackID) &&  
	     ( (a.spos<b.spos) || ( (a.spos==b.spos) && (a.qpos < b.qpos) )) ) );
}

bool operator>(const PointPair& a, const PointPair& b){
  return ( (a.trackID>b.trackID) ||
	   ( (a.trackID==b.trackID) &&  
	     ( (a.spos>b.spos) || ( (a.spos==b.spos) && (a.qpos > b.qpos) )) ) );
}

bool operator==(const PointPair& a, const PointPair& b){
  return ( (a.trackID==b.trackID) && (a.qpos==b.qpos) && (a.spos==b.spos) );
}

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

  if(O2_ACTION(COM_SERVER))
#ifdef LIBRARY
    ;
#else
    startServer();
#endif

  else  if(O2_ACTION(COM_CREATE))
    create(dbName);

  else if(O2_ACTION(COM_INSERT))
    insert(dbName, inFile);

  else if(O2_ACTION(COM_BATCHINSERT))
    batchinsert(dbName, inFile);

  else if(O2_ACTION(COM_QUERY))
    if(isClient){
#ifdef LIBRARY
      ;
#else
      if(query_from_key){
	VERB_LOG(1, "Calling web services query %s on database %s, query=%s\n", radius>0?"(Radius)":"(NN)", dbName, (key&&strlen(key))?key:inFile);
	ws_query_by_key(dbName, key, inFile, (char*)hostport);	
      }
      else{
	VERB_LOG(1, "Calling web services query on database %s, query=%s\n", dbName, (key&&strlen(key))?key:inFile);
	ws_query(dbName, inFile, (char*)hostport);
      }
#endif
    }
    else
      query(dbName, inFile);

  else if(O2_ACTION(COM_STATUS))
    if(isClient)
#ifdef LIBRARY
      ;
#else
      ws_status(dbName,(char*)hostport);
#endif
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
#ifdef LIBRARY
      ;
#else 
      ws_liszt(dbName, (char*) hostport);
#endif
    else
      liszt(dbName, lisztOffset, lisztLength);

  else if(O2_ACTION(COM_INDEX))
    index_index_db(dbName);
  
  else
    error("Unrecognized command",command);
}

audioDB::audioDB(const unsigned argc, const char *argv[], adb__queryResponse *adbQueryResponse): O2_AUDIODB_INITIALIZERS
{
  try {
    isServer = 1; // Set to make errors report over SOAP
    processArgs(argc, argv);
    // Perform database prefix substitution
    if(dbName && adb_root)
      prefix_name((char** const)&dbName, adb_root);
    assert(O2_ACTION(COM_QUERY));
    query(dbName, inFile, adbQueryResponse);
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

audioDB::audioDB(const unsigned argc, const char *argv[], adb__lisztResponse *adbLisztResponse): O2_AUDIODB_INITIALIZERS
{
  try {
    isServer = 1; // Set to make errors report over SOAP
    processArgs(argc, argv); 
    // Perform database prefix substitution
    if(dbName && adb_root)
      prefix_name((char** const)&dbName, adb_root);
    assert(O2_ACTION(COM_LISZT));
    liszt(dbName, lisztOffset, lisztLength, adbLisztResponse);
  } catch(char *err) {
    cleanup();
    throw(err);
  }
}


//for the lib / API
audioDB::audioDB(const unsigned argc, const char *argv[], int * apierror): O2_AUDIODB_INITIALIZERS
{

    try {
        UseApiError=1;

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

        adb__queryResponse adbq;

        if(O2_ACTION(COM_CREATE))
            create(dbName);

        else if(O2_ACTION(COM_INSERT))
            insert(dbName, inFile);

        else if(O2_ACTION(COM_BATCHINSERT))
            batchinsert(dbName, inFile);

        else if(O2_ACTION(COM_QUERY))
            if(isClient)
                ;//ws_query(dbName, inFile, (char*)hostport);
            else
                query(dbName, inFile, &adbq);
        //query(dbName, inFile);

        else if(O2_ACTION(COM_STATUS))
            if(isClient)
                ;//ws_status(dbName,(char*)hostport);
            else
                status(dbName);

        else if(O2_ACTION(COM_L2NORM))
            l2norm(dbName);

        else if(O2_ACTION(COM_POWER))
            power_flag(dbName);

        else if(O2_ACTION(COM_DUMP))
            dump(dbName);

        else
            error("Unrecognized command",command);

    } catch(int a) {
        *apierror=a;
        return;

    }
    *apierror=apierrortemp;
    return;

}

//for API status
audioDB::audioDB(const unsigned argc, const char *argv[], cppstatusptr stat, int * apierror): O2_AUDIODB_INITIALIZERS
{

    try {
        UseApiError=1;


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

        status(dbName, stat);


    } catch(int a) {
        *apierror=a;
        return;

    }
    *apierror=apierrortemp;
    return;

}


//for API query
audioDB::audioDB(const unsigned argc, const char *argv[],adb__queryResponse *adbQueryResponse, int * apierror): O2_AUDIODB_INITIALIZERS
{

    try {
        UseApiError=1;

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

        query(dbName, inFile, adbQueryResponse);

    } catch(int a) {
        *apierror=a;
        return;

    }
    *apierror=apierrortemp;
    return;

}





void audioDB::cleanup() {
  cmdline_parser_free(&args_info);
  if(indata)
    munmap(indata,statbuf.st_size);
  if(db)
    munmap(db,getpagesize());
  if(fileTable)
    munmap(fileTable, fileTableLength);
  if(trackTable)
    munmap(trackTable, trackTableLength);
  if(dataBuf)
    munmap(dataBuf, dataBufLength);
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
  if(trackOffsetTable)
    delete [] trackOffsetTable;
  if(reporter)
    delete reporter;
  if(exact_evaluation_queue)
    delete exact_evaluation_queue;
  if(rng)
    gsl_rng_free(rng);
  if(vv)
    delete vv;
  if(dbfid>0)
    close(dbfid);
  if(infid>0)
    close(infid);
  if(dbH)
    delete dbH;
  if(lsh!=SERVER_LSH_INDEX_SINGLETON)
    delete lsh;
}

audioDB::~audioDB(){
  cleanup();
}

int audioDB::processArgs(const unsigned argc, const char *argv[]){

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

  /* KLUDGE: gengetopt generates a function which is not completely
     const-clean in its declaration.  We cast argv here to keep the
     compiler happy.  -- CSR, 2008-10-08 */
  if (cmdline_parser (argc, (char *const *) argv, &args_info) != 0)
    error("Error parsing command line");

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
    nsamples = args_info.nsamples_arg;
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
      if(queryPoint<0 || queryPoint >10000)
        error("queryPoint out of range: 0 <= queryPoint <= 10000");
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
  if(!dbH)
    initTables(dbName, 0);

  unsigned dudCount=0;
  unsigned nullCount=0;
  for(unsigned k=0; k<dbH->numFiles; k++){
    if(trackTable[k]<sequenceLength){
      dudCount++;
      if(!trackTable[k])
        nullCount++;
    }
  }
  
  if(adbStatusResponse == 0) {

    // Update Header information
    std::cout << "num files:" << dbH->numFiles << std::endl;
    std::cout << "data dim:" << dbH->dim <<std::endl;
    if(dbH->dim>0){
      std::cout << "total vectors:" << dbH->length/(sizeof(double)*dbH->dim)<<std::endl;
      if(dbH->flags & O2_FLAG_LARGE_ADB)
	std::cout << "vectors available:" << O2_MAX_VECTORS - (dbH->length / (sizeof(double)*dbH->dim)) << std::endl;
      else
	std::cout << "vectors available:" << (dbH->timesTableOffset-(dbH->dataOffset+dbH->length))/(sizeof(double)*dbH->dim) << std::endl;
    }
    if( ! (dbH->flags & O2_FLAG_LARGE_ADB) ){
      std::cout << "total bytes:" << dbH->length << " (" << (100.0*dbH->length)/(dbH->timesTableOffset-dbH->dataOffset) << "%)" << std::endl;
      std::cout << "bytes available:" << dbH->timesTableOffset-(dbH->dataOffset+dbH->length) << " (" <<
	(100.0*(dbH->timesTableOffset-(dbH->dataOffset+dbH->length)))/(dbH->timesTableOffset-dbH->dataOffset) << "%)" << std::endl;
    }
    std::cout << "flags:" << " l2norm[" << DISPLAY_FLAG(dbH->flags&O2_FLAG_L2NORM)
	      << "] minmax[" << DISPLAY_FLAG(dbH->flags&O2_FLAG_MINMAX)
	      << "] power[" << DISPLAY_FLAG(dbH->flags&O2_FLAG_POWER)
	      << "] times[" << DISPLAY_FLAG(dbH->flags&O2_FLAG_TIMES) 
	      << "] largeADB[" << DISPLAY_FLAG(dbH->flags&O2_FLAG_LARGE_ADB)
	      << "]" << endl;    
              
    std::cout << "null count: " << nullCount << " small sequence count " << dudCount-nullCount << std::endl;    
  } else {
    adbStatusResponse->result.numFiles = dbH->numFiles;
    adbStatusResponse->result.dim = dbH->dim;
    adbStatusResponse->result.length = dbH->length;
    adbStatusResponse->result.dudCount = dudCount;
    adbStatusResponse->result.nullCount = nullCount;
    adbStatusResponse->result.flags = dbH->flags;
  }
}

///used by lib/API
void audioDB::status(const char* dbName, cppstatusptr status){
    if(!dbH) {
        initTables(dbName, 0);
    }

  unsigned dudCount=0;
  unsigned nullCount=0;
  for(unsigned k=0; k<dbH->numFiles; k++){
    if(trackTable[k]<sequenceLength){
      dudCount++;
      if(!trackTable[k])
        nullCount++;
    }
  }
  
  status->numFiles = dbH->numFiles;
  status->dim = dbH->dim;
  status->length = dbH->length;
  status->dudCount = dudCount;
  status->nullCount = nullCount;
  status->flags = dbH->flags;
  
  return;
}




void audioDB::l2norm(const char* dbName) {
  forWrite = true;
  initTables(dbName, 0);
  if( !(dbH->flags & O2_FLAG_LARGE_ADB ) && (dbH->length>0) ){
    /* FIXME: should probably be uint64_t */
    unsigned numVectors = dbH->length/(sizeof(double)*dbH->dim);
    CHECKED_MMAP(double *, dataBuf, dbH->dataOffset, dataBufLength);
    unitNormAndInsertL2(dataBuf, dbH->dim, numVectors, 0); // No append
  }
  // Update database flags
  dbH->flags = dbH->flags|O2_FLAG_L2NORM;
  memcpy (db, dbH, O2_HEADERSIZE);
}

void audioDB::power_flag(const char *dbName) {
  forWrite = true;
  initTables(dbName, 0);  
  if( !(dbH->flags & O2_FLAG_LARGE_ADB ) && (dbH->length>0) ){
    error("cannot turn on power storage for non-empty database", dbName);
  }
  dbH->flags |= O2_FLAG_POWER;
  memcpy(db, dbH, O2_HEADERSIZE);
}

// Unit norm block of features

/* FIXME: in fact this does not unit norm a block of features, it just
   records the L2 norms somewhere.  unitNorm() does in fact unit norm
   a block of features. */
void audioDB::unitNormAndInsertL2(double* X, unsigned dim, unsigned n, unsigned append=0){
  unsigned d;
  double *p;
  unsigned nn = n;

  assert(l2normTable);

  if( !(dbH->flags & O2_FLAG_LARGE_ADB) && !append && (dbH->flags & O2_FLAG_L2NORM) )
    error("Database is already L2 normed", "automatic norm on insert is enabled");

  VERB_LOG(2, "norming %u vectors...", n);

  double* l2buf = new double[n];
  double* l2ptr = l2buf;
  assert(l2buf);
  assert(X);

  while(nn--){
    p=X;
    *l2ptr=0.0;
    d=dim;
    while(d--){
      *l2ptr+=*p**p;
      p++;
    }
    l2ptr++;
    X+=dim;
  }
  unsigned offset;
  if(append) {
    // FIXME: a hack, a very palpable hack: the vectors have already
    // been inserted, and dbH->length has already been updated.  We
    // need to subtract off again the number of vectors that we've
    // inserted this time...
    offset=(dbH->length/(dbH->dim*sizeof(double)))-n; // number of vectors
  } else {
    offset=0;
  }
  memcpy(l2normTable+offset, l2buf, n*sizeof(double));
  if(l2buf)
    delete[] l2buf;
  VERB_LOG(2, " done.");
}

// This entry point is visited once per instance
// so it is a good place to set any global state variables
int main(const int argc, const char* argv[]){
  SERVER_LSH_INDEX_SINGLETON = 0; // Initialize global variables
  SERVER_ADB_ROOT = 0;            // Server-side database root prefix
  SERVER_ADB_FEATURE_ROOT = 0;    // Server-side features root prefix
  audioDB(argc, argv);
}


extern "C" {

/* for API questions contact 
 * Christophe Rhodes c.rhodes@gold.ac.uk
 * Ian Knopke mas01ik@gold.ac.uk, ian.knopke@gmail.com */

#include "audioDB_API.h"


    //adb_ptr audiodb_create(char * path,long ntracks, long datadim) {
    adb_ptr audiodb_create(char * path,long datasize,long ntracks, long datadim) {
        const char *argv[12];
        int argvctr=0;
        char tempstr1[200];
        char tempstr2[200];
        char tempstr3[200];
        int apierror=0;


        argv[argvctr++] = "audioDB";
        argv[argvctr++] = "--NEW";
        argv[argvctr++] = "-d";
        argv[argvctr++] = path;

        if (datasize >0){
            argv[argvctr++]="--datasize";
            snprintf(tempstr1,sizeof(tempstr1),"%ld",datasize);
            argv[argvctr++]=tempstr1;
        }

        if (ntracks >0){
            argv[argvctr++]="--ntracks";
            snprintf(tempstr2,sizeof(tempstr2),"%ld",ntracks);
            argv[argvctr++]=tempstr2;
        }

        if (datadim > 0){
            argv[argvctr++]="--datadim";
            snprintf(tempstr3,sizeof(tempstr3),"%ld",datadim);
            argv[argvctr++]=tempstr3;
        }

        argv[argvctr+1]='\0';

        audioDB::audioDB(argvctr, argv, &apierror);

        if (!apierror){ 
            return audiodb_open(path);
        }

        /* database exists, so fail and pass NULL */
        return NULL;
    }



  int audiodb_insert(adb_ptr mydb, adb_insert_ptr ins) {
    const char *argv[15];
    int argvctr=0;
    int apierror=0;

    argv[argvctr++]="audioDB";
    argv[argvctr++]="-I";
    argv[argvctr++]="-d";
    argv[argvctr++]=mydb->dbname;
    argv[argvctr++]="-f";
    argv[argvctr++]=ins->features;

    if (ins->times){
        argv[argvctr++]="--times";
        argv[argvctr++]=ins->times;
    }

    if (ins->power){
        argv[argvctr++]="-w";
        argv[argvctr++]=ins->power;
    }

    if (ins->key){
        argv[argvctr++]="--key";
        argv[argvctr++]=ins->key;
    }
    argv[argvctr+1]='\0';

    audioDB::audioDB(argvctr,argv,&apierror);
    return apierror;
  }


  int audiodb_batchinsert(adb_ptr mydb, adb_insert_ptr ins, unsigned int size) {

    const char *argv[22];
    int argvctr=0;
    unsigned int i=0;
    char tempfeaturename[]="tempfeatureXXXXXX";
    char temppowername[]="temppowerXXXXXX";
    char tempkeyname[]="tempkeyXXXXXX";
    char temptimesname[]="temptimesXXXXXX";
    int tempfeaturefd = -1;
    int temppowerfd = -1;
    int tempkeyfd = -1;
    int temptimesfd = -1;

    int flags[4]={0};
    int apierror=0;

    /*  So the final API should take an array of structs. However, the current 
    *   version requires four separate text files. So temporarily, we need to
    *   unpack the struct array, make four separate text files, and then reinsert
    *   them into the command-line call. This should change as soon as possible */


    argv[argvctr++]="audioDB";
    argv[argvctr++]="-B";
    argv[argvctr++]="-d";
    argv[argvctr++]=mydb->dbname;


    /* assume struct is well formed for all entries */
    if (ins[0].features){ flags[0]++;} else {
        /* short circuit the case where there are no features in the structs */
        return -1;
    } ;
    if (ins[0].power){ flags[1]++;};
    if (ins[0].key){ flags[2]++;};
    if (ins[0].times){ flags[3]++;};


    /* make four temp files */
    if ((tempfeaturefd = mkstemp(tempfeaturename)) == -1)
      goto error;
    if ((temppowerfd = mkstemp(temppowername)) == -1)
      goto error;
    if ((tempkeyfd=mkstemp(tempkeyname)) == -1)
      goto error;
    if ((temptimesfd=mkstemp(temptimesname)) == -1)
      goto error;

    /* Ok, so we should have a working set of files to write to */ 
    /* I'm going to assume that the same format is kept for all structs in the array */
    /* That is, each struct should be correctly formed, and contain at least a features file, because I'm just going to pass the terms along to the text files */
    for (i = 0; i < size; i++) {
      if (write(tempfeaturefd,ins[i].features,strlen(ins[i].features)) != (ssize_t) strlen(ins[i].features))
	goto error;
      if (write(tempfeaturefd,"\n",1) != 1)
	goto error;

      if (flags[1]) {
	if (write(temppowerfd,ins[i].power,strlen(ins[i].power)) != (ssize_t) strlen(ins[i].power))
	  goto error;
	if (write(temppowerfd,"\n",1) != 1)
	  goto error;
      }
      if (flags[2]) {
	if (write(tempkeyfd,ins[i].key,strlen(ins[i].key)) != (ssize_t) strlen(ins[i].key))
	  goto error;
	if (write(tempkeyfd,"\n",1) != 1)
	  goto error;
      }
      if (flags[3]) {
	if (write(temptimesfd,ins[i].times,strlen(ins[i].times)) != (ssize_t) strlen(ins[i].times))
	  goto error;
	if (write(temptimesfd,"\n",1) != 1)
	  goto error;
      }
    }

    argv[argvctr++]="-F";
    argv[argvctr++]=tempfeaturename;
    close(tempfeaturefd);
    close(temppowerfd);
    close(tempkeyfd);
    close(temptimesfd);

    if (flags[1]){
        argv[argvctr++]="--powerList";
        argv[argvctr++]=temppowername;
    }

    if (flags[2]){
        argv[argvctr++]="--keyList";
        argv[argvctr++]=tempkeyname;
    }

    if (flags[3]){
        argv[argvctr++]="--timesList";
        argv[argvctr++]=temptimesname;
    }

    argv[argvctr+1]='\0';

    audioDB::audioDB(argvctr,argv,&apierror);

    remove(tempfeaturename);
    remove(temppowername);
    remove(tempkeyname);
    remove(temptimesname);


    return apierror;

  error:
    if(tempfeaturefd != -1) {
      close(tempfeaturefd);
      remove(tempfeaturename);
    }
    if(temppowerfd != -1) {
      close(temppowerfd);
      remove(temppowername);
    }
    if(tempkeyfd != -1) {
      close(tempkeyfd);
      remove(tempkeyname);
    }
    if(temptimesfd != -1) {
      close(temptimesfd);
      remove(temptimesname);
    }
    return -1;
  }


  int audiodb_query(adb_ptr mydb, adb_query_ptr adbq, adb_queryresult_ptr adbqr){

    const char *argv[32];
    int argvctr=0;
    char tempstr1[200];
    char tempstr2[200];
    char tempstr3[200];
    int apierror=0;

    adb__queryResponse adbQueryResponse; 

    /* TODO: may need error checking here */
    /* currently counting on audioDB binary to fail for me */
    argv[argvctr++]="audioDB";
    
    if(adbq->querytype){
        argv[argvctr++]="-Q";
        argv[argvctr++]=adbq->querytype;
    }

    if(mydb->dbname){
        argv[argvctr++]="-d";
        argv[argvctr++]=mydb->dbname;
    }

    if (adbq->feature){
        argv[argvctr++]="-f";
        argv[argvctr++]=adbq->feature;
    }

    if (adbq->power){
        argv[argvctr++]="-w";
        argv[argvctr++]=adbq->power;
    }

    if (adbq->qpoint){
        argv[argvctr++]="-p";
        argv[argvctr++]=adbq->qpoint;
    }
    if (adbq->numpoints){
        argv[argvctr++]="-n";
        argv[argvctr++]=adbq->numpoints;
    }
    if (adbq->radius){
        argv[argvctr++]="-R";
        argv[argvctr++]=adbq->radius;
    }
    if(adbq->resultlength){
        argv[argvctr++]="-r";
        argv[argvctr++]=adbq->resultlength;
    }
    if(adbq->sequencelength){
        argv[argvctr++]="-l";
        argv[argvctr++]=adbq->sequencelength;
    }
    if(adbq->sequencehop){
        argv[argvctr++]="-h";
        argv[argvctr++]=adbq->sequencehop;
    }

    if (adbq->absolute_threshold){
        argv[argvctr++]="--absolute-threshold";
        snprintf(tempstr1,sizeof(tempstr1),"%f",adbq->absolute_threshold);
        argv[argvctr++]=tempstr1;
    }

    if (adbq->relative_threshold){
        argv[argvctr++]="--relative-threshold";
        snprintf(tempstr2,sizeof(tempstr2),"%f",adbq->relative_threshold);
        argv[argvctr++]=tempstr2;
    }

    if (adbq->exhaustive){
        argv[argvctr++]="--exhaustive";
    }

    if (adbq->expandfactor){
        argv[argvctr++]="--expandfactor";
        snprintf(tempstr3,sizeof(tempstr3),"%f",adbq->expandfactor);
        argv[argvctr++]=tempstr3;
    }

    if (adbq->rotate){
        argv[argvctr++]="--rotate";
    }

    if (adbq->keylist){
        argv[argvctr++]="-K";
        argv[argvctr++]=adbq->keylist;
    }
    argv[argvctr+1]='\0';

    /* debugging */

    audioDB::audioDB(argvctr,argv, &adbQueryResponse, &apierror);

    //copy data over here from adbQueryResponse to adbqr
    adbqr->sizeRlist=adbQueryResponse.result.__sizeRlist;
    adbqr->sizeDist=adbQueryResponse.result.__sizeDist;
    adbqr->sizeQpos=adbQueryResponse.result.__sizeQpos;
    adbqr->sizeSpos=adbQueryResponse.result.__sizeSpos;
    adbqr->Rlist=adbQueryResponse.result.Rlist;
    adbqr->Dist=adbQueryResponse.result.Dist;
    adbqr->Qpos=adbQueryResponse.result.Qpos;
    adbqr->Spos=adbQueryResponse.result.Spos;

    return apierror;
  }

  ///* status command */
  int audiodb_status(adb_ptr mydb, adb_status_ptr status){

      cppstatus sss;
      int apierror=0;

      const char *argv[5];

      apierror=0;
      argv[0]="audioDB";
      argv[1]="--STATUS";
      argv[2]="-d";
      argv[3]=mydb->dbname;
      argv[4]='\0';

      audioDB::audioDB(4,argv,&sss ,&apierror);
      
      status->numFiles=sss.numFiles;
      status->dim=sss.dim;
      status->length=sss.length;
      status->dudCount=sss.dudCount;
      status->nullCount=sss.nullCount;
      status->flags=sss.flags;

      return apierror;
  }

  int audiodb_dump(adb_ptr mydb){
      return audiodb_dump_withdir(mydb,"audioDB.dump");
  }

  int audiodb_dump_withdir(adb_ptr mydb, const char *outputdir){

      const char *argv[7];
      int argvctr=0;
      int apierror=0;

      argv[argvctr++]="audioDB";
      argv[argvctr++]="--DUMP";
      argv[argvctr++]="-d";
      argv[argvctr++]=mydb->dbname;
      argv[argvctr++]="--output";
      argv[argvctr++]=(char *)outputdir;
      argv[argvctr]='\0';

      audioDB::audioDB(6,argv,&apierror);

      return apierror;
  }

  int audiodb_l2norm(adb_ptr mydb){

      const char *argv[5];
      int apierror=0;

      argv[0]="audioDB";
      argv[1]="--L2NORM";
      argv[2]="-d";
      argv[3]=mydb->dbname;
      argv[4]='\0';

      audioDB::audioDB(4,argv,&apierror);
      return apierror;
  }

  int audiodb_power(adb_ptr mydb){

      const char *argv[5];
      int apierror=0;

      argv[0]="audioDB";
      argv[1]="--POWER";
      argv[2]="-d";
      argv[3]=mydb->dbname;
      argv[4]='\0';

      audioDB::audioDB(4,argv,&apierror);
      return apierror;
  }

  adb_ptr audiodb_open(char * path){

        adb_ptr mydbp;
        adbstatus mystatus;

        /* if db exists */

        if (open(path, O_EXCL) != -1){

            mydbp=(adb_ptr)malloc(sizeof(adb));
            mydbp->dbname=(char *)malloc(sizeof(path));

            strcpy(mydbp->dbname,path); 

            audiodb_status(mydbp, &mystatus);
            mydbp->ntracks=mystatus.numFiles;
            mydbp->datadim=mystatus.dim;

            return mydbp;
        }

        return NULL;
  };



  void audiodb_close(adb_ptr db){

      free(db->dbname);
      free(db);

  }


}

