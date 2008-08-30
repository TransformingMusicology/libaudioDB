#include "audioDB.h"
#include "adb.nsmap"

/* Command-line client definitions */

// FIXME: this can't propagate the sequence length argument (used for
// dudCount).  See adb__status() definition for the other half of
// this.  -- CSR, 2007-10-01
void audioDB::ws_status(const char*dbName, char* hostport){
  struct soap soap;
  adb__statusResponse adbStatusResponse;  
  
  // Query an existing adb database
  soap_init(&soap);
  if(soap_call_adb__status(&soap,hostport,NULL,(char*)dbName,adbStatusResponse)==SOAP_OK) {
    std::cout << "numFiles = " << adbStatusResponse.result.numFiles << std::endl;
    std::cout << "dim = " << adbStatusResponse.result.dim << std::endl;
    std::cout << "length = " << adbStatusResponse.result.length << std::endl;
    std::cout << "dudCount = " << adbStatusResponse.result.dudCount << std::endl;
    std::cout << "nullCount = " << adbStatusResponse.result.nullCount << std::endl;
    std::cout << "flags = " << (adbStatusResponse.result.flags & 0x00FFFFFF) << std::endl;
  } else {
    soap_print_fault(&soap,stderr);
  }
  
  soap_destroy(&soap);
  soap_end(&soap);
  soap_done(&soap);
}

// WS_QUERY (CLIENT SIDE)
void audioDB::ws_query(const char*dbName, const char *featureFileName, const char* hostport){
  struct soap soap;
  adb__queryResponse adbQueryResponse;  

  soap_init(&soap);  
  if(soap_call_adb__query(&soap,hostport,NULL,
			  (char*)dbName,(char*)featureFileName,(char*)trackFileName,(char*)timesFileName,
			  queryType, queryPoint, pointNN, trackNN, sequenceLength, adbQueryResponse)==SOAP_OK){
    //std::std::cerr << "result list length:" << adbQueryResponse.result.__sizeRlist << std::std::endl;
    for(int i=0; i<adbQueryResponse.result.__sizeRlist; i++)
      std::cout << adbQueryResponse.result.Rlist[i] << " " << adbQueryResponse.result.Dist[i] 
		<< " " << adbQueryResponse.result.Qpos[i] << " " << adbQueryResponse.result.Spos[i] << std::endl;
  }
  else
    soap_print_fault(&soap,stderr);
  
  soap_destroy(&soap);
  soap_end(&soap);
  soap_done(&soap);
}

// WS_QUERY_BY_KEY (CLIENT SIDE)
void audioDB::ws_query_by_key(const char*dbName, const char *trackKey, const char* featureFileName, const char* hostport){
  struct soap soap;
  adb__queryResponse adbQueryResponse;  
  /*  JUST TRY TO USE A DATA STRUCTURE WITH PHP
      adb__sequenceQueryParms asqp;
      asqp.keyList = (char*)trackFileName;
      asqp.timesFileName = (char*)timesFileName;
      asqp.queryPoint = queryPoint;
      asqp.pointNN = pointNN;
      asqp.trackNN = trackNN;
      asqp.sequenceLength = sequenceLength;
      asqp.radius = radius;
      asqp.relative_threshold = relative_threshold;
      asqp.absolute_threshold = absolute_threshold;
      asqp.usingQueryPoint = usingQueryPoint;
      asqp.lsh_exact = lsh_exact;
  */

  soap_init(&soap);  
  if(queryType==O2_SEQUENCE_QUERY || queryType==O2_N_SEQUENCE_QUERY){
    if(soap_call_adb__sequenceQueryByKey(&soap,hostport,NULL,
					 (char*)dbName,
					 (char*)trackKey,
					 (char*)featureFileName,
					 queryType,
					 (char*)trackFileName, // this means keyFileName 
					 (char*)timesFileName,
					   queryPoint,
					 pointNN,
					 trackNN,
					 sequenceLength,
					 radius,
					 absolute_threshold,
					 usingQueryPoint,
					   lsh_exact,
					 adbQueryResponse)==SOAP_OK){
      //std::std::cerr << "result list length:" << adbQueryResponse.result.__sizeRlist << std::std::endl;
      for(int i=0; i<adbQueryResponse.result.__sizeRlist; i++)
	std::cout << adbQueryResponse.result.Rlist[i] << " " << adbQueryResponse.result.Dist[i] 
		  << " " << adbQueryResponse.result.Qpos[i] << " " << adbQueryResponse.result.Spos[i] << std::endl;
    }
    else
      soap_print_fault(&soap,stderr);
  }else
    ;// FIX ME: WRITE NON-SEQUENCE QUERY BY KEY ?
  
  soap_destroy(&soap);
  soap_end(&soap);
  soap_done(&soap);
}


/* Server definitions */
int adb__status(struct soap* soap, xsd__string dbName, adb__statusResponse &adbStatusResponse){
  char* const argv[]={"./audioDB",COM_STATUS,"-d",dbName};
  const unsigned argc = 4;
  try {
    audioDB(argc, argv, &adbStatusResponse);
    return SOAP_OK;
  } catch(char *err) {
    soap_receiver_fault(soap, err, "");
    return SOAP_FAULT;
  }
}
 
// Literal translation of command line to web service
int adb__query(struct soap* soap, xsd__string dbName, xsd__string qKey, xsd__string keyList, xsd__string timesFileName, xsd__int qType, xsd__int qPos, xsd__int pointNN, xsd__int trackNN, xsd__int seqLen, adb__queryResponse &adbQueryResponse){
  char queryType[256];
  for(int k=0; k<256; k++)
    queryType[k]='\0';
  if(qType == O2_POINT_QUERY)
    strncpy(queryType, "point", strlen("point"));
  else if (qType == O2_SEQUENCE_QUERY)
    strncpy(queryType, "sequence", strlen("sequence"));
  else if(qType == O2_TRACK_QUERY)
    strncpy(queryType,"track", strlen("track"));
  else if(qType == O2_N_SEQUENCE_QUERY)
    strncpy(queryType,"nsequence", strlen("nsequence"));

  if(pointNN==0)
    pointNN=10;
  if(trackNN==0)
    trackNN=10;
  if(seqLen==0)
    seqLen=16;

  char qPosStr[256];
  sprintf(qPosStr, "%d", qPos);
  char pointNNStr[256];
  sprintf(pointNNStr,"%d",pointNN);
  char trackNNStr[256];
  sprintf(trackNNStr,"%d",trackNN);
  char seqLenStr[256];  
  sprintf(seqLenStr,"%d",seqLen);
  
  const  char* argv[] ={
    "./audioDB", 
    COM_QUERY, 
    queryType, // Need to pass a parameter
    COM_DATABASE,
    ENSURE_STRING(dbName),
    COM_FEATURES,
    ENSURE_STRING(qKey),
    COM_KEYLIST,
    ENSURE_STRING(keyList),
    COM_TIMES,
    ENSURE_STRING(timesFileName),
    COM_QPOINT, 
    qPosStr,
    COM_POINTNN,
    pointNNStr,
    COM_TRACKNN,
    trackNNStr, // Need to pass a parameter
    COM_SEQLEN,
    seqLenStr
  };

  const unsigned argc = 19;
  try {
    audioDB(argc, (char* const*)argv, &adbQueryResponse);
    return SOAP_OK;
  } catch (char *err) {
    soap_receiver_fault(soap, err, "");
    return SOAP_FAULT;
  }
}

int adb__sequenceQueryByKey(struct soap* soap,xsd__string dbName,
			    xsd__string trackKey,
			    xsd__string featureFileName,
			    xsd__int queryType,
			    xsd__string keyFileName,
			    xsd__string timesFileName,
			    xsd__int queryPoint,
			    xsd__int pointNN,
			    xsd__int trackNN,
			    xsd__int sequenceLength,
			    xsd__double radius,
			    xsd__double absolute_threshold,
			    xsd__int usingQueryPoint,
			    xsd__int lsh_exact,
			    struct adb__queryResponse& adbQueryResponse){
  char radiusStr[256];
  char qPosStr[256];
  char pointNNStr[256];
  char trackNNStr[256];
  char seqLenStr[256];
  char absolute_thresholdStr[256];
  char qtypeStr[256];

  /* When the branch is merged, move this to a header and use it
     elsewhere */
#define INTSTRINGIFY(val, str) \
  snprintf(str, 256, "%d", val);
#define DOUBLESTRINGIFY(val, str) \
  snprintf(str, 256, "%f", val);

  INTSTRINGIFY(queryPoint, qPosStr);
  INTSTRINGIFY(pointNN, pointNNStr);
  INTSTRINGIFY(trackNN, trackNNStr);
  INTSTRINGIFY(sequenceLength, seqLenStr);
  DOUBLESTRINGIFY(absolute_threshold, absolute_thresholdStr);
  DOUBLESTRINGIFY(radius, radiusStr);

  // WS queries only support 1-nearest neighbour point reporting
  // at the moment, until we figure out how to better serve results
  snprintf(qtypeStr, 256, "nsequence");
  const char *argv[]={
    "./audioDB",
    COM_QUERY,
    qtypeStr,
    COM_DATABASE,
    dbName, 
    strlen(trackKey)?COM_QUERYKEY:COM_FEATURES,
    strlen(trackKey)?ENSURE_STRING(trackKey):ENSURE_STRING(featureFileName),
    COM_KEYLIST,
    ENSURE_STRING(keyFileName),
    usingQueryPoint?COM_QPOINT:COM_EXHAUSTIVE,
    usingQueryPoint?qPosStr:"",
    COM_POINTNN,
    pointNNStr,
    COM_TRACKNN,
    trackNNStr,
    COM_RADIUS,
    radiusStr,
    COM_SEQLEN,
    seqLenStr,
    COM_ABSOLUTE_THRESH,
    absolute_thresholdStr,
    lsh_exact?COM_LSH_EXACT:""
  };

  const unsigned argc = 22;
  
 
  try {
    audioDB(argc, (char* const*)argv, &adbQueryResponse);
    return SOAP_OK;
  } catch (char *err) {
    soap_receiver_fault(soap, err, "");
    return SOAP_FAULT;
  }
}

/* Server loop */
void audioDB::startServer(){
  struct soap soap;
  int m, s; // master and slave sockets
  soap_init(&soap);
  // FIXME: largely this use of SO_REUSEADDR is to make writing (and
  // running) test cases more convenient, so that multiple test runs
  // in close succession don't fail because of a bin() error.
  // Investigate whether there are any potential drawbacks in this,
  // and also whether there's a better way to write the tests.  --
  // CSR, 2007-10-03
  soap.bind_flags |= SO_REUSEADDR;
  m = soap_bind(&soap, NULL, port, 100);
  if (m < 0)
    soap_print_fault(&soap, stderr);
  else
    {
      fprintf(stderr, "Socket connection successful: master socket = %d\n", m);
      // Make a global Web Services LSH Index (SINGLETON)
      if(WS_load_index && dbName && !index_exists(dbName, radius, sequenceLength)){
        error("Can't find requested index file:", index_get_name(dbName,radius,sequenceLength));
      }
      if(WS_load_index && dbName && index_exists(dbName, radius, sequenceLength)){
	char* indexName = index_get_name(dbName, radius, sequenceLength);
	fprintf(stderr, "Loading LSH hashtables: %s...\n", indexName);
	lsh = new LSH(indexName, true);
	assert(lsh);
	SERVER_LSH_INDEX_SINGLETON = lsh;
	fprintf(stderr, "LSH INDEX READY\n");
	fflush(stderr);
	delete[] indexName;
      }
      
      // Server-side path prefix to databases and features
      if(adb_root)
	SERVER_ADB_ROOT = (char*)adb_root; // Server-side database root
      if(adb_feature_root)
	SERVER_ADB_FEATURE_ROOT = (char*)adb_feature_root; // Server-side features root

      for (int i = 1; ; i++)
	{
	  s = soap_accept(&soap);
	  if (s < 0)
	    {
	      soap_print_fault(&soap, stderr);
	      break;
	    }
          /* FIXME: find a way to play nice with logging when run from
             /etc/init.d scripts: at present this just goes nowhere */
	  fprintf(stderr, "%d: accepted connection from IP=%lu.%lu.%lu.%lu socket=%d\n", i,
		  (soap.ip >> 24)&0xFF, (soap.ip >> 16)&0xFF, (soap.ip >> 8)&0xFF, soap.ip&0xFF, s);
	  if (soap_serve(&soap) != SOAP_OK) // process RPC request
	    soap_print_fault(&soap, stderr); // print error
	  fprintf(stderr, "request served\n");
	  soap_destroy(&soap); // clean up class instances
	  soap_end(&soap); // clean up everything and close socket
	}
    }
  soap_done(&soap); // close master socket and detach environment
} 
