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
    char fault[MAXSTR];
    soap_sprint_fault(&soap, fault, MAXSTR);
    error(fault);
  }
  
  soap_destroy(&soap);
  soap_end(&soap);
  soap_done(&soap);
}

void audioDB::ws_liszt(const char* dbName, char* Hostport){
  struct soap soap;
  adb__lisztResponse adbLisztResponse;

  soap_init(&soap);
  if(soap_call_adb__liszt(&soap, hostport, NULL, (char*)dbName, lisztOffset, lisztLength, adbLisztResponse)==SOAP_OK){
    for(int i = 0; i < adbLisztResponse.result.__sizeRkey; i++) {
      std::cout << "[" << i+lisztOffset << "] " << adbLisztResponse.result.Rkey[i] << " (" 
		<< adbLisztResponse.result.Rlen[i] << ")" << std::endl;
    }
  } else {
    char fault[MAXSTR];
    soap_sprint_fault(&soap, fault, MAXSTR);
    error(fault);
  }
  soap_destroy(&soap);
  soap_end(&soap);
  soap_done(&soap);
}

// WS_QUERY (CLIENT SIDE)
void audioDB::ws_query(const char*dbName, const char *featureFileName, const char* hostport){
  struct soap soap;
  adb__queryResponse adbQueryResponse;  
  VERB_LOG(1, "Calling fileName query on database %s with featureFile=%s\n", dbName, featureFileName);
  soap_init(&soap);
  if(soap_call_adb__query(&soap, hostport, NULL, (char *) dbName,
			  (char *)featureFileName, (char *)trackFileName,
			  (char *)timesFileName, (char *) powerFileName,
			  queryType, queryPoint, 
			  pointNN, trackNN, sequenceLength, 
			  radius, absolute_threshold, relative_threshold,
			  !usingQueryPoint, lsh_exact, no_unit_norming,
			  adbQueryResponse) 
     == SOAP_OK) {
    if(radius == 0) {
      for(int i=0; i<adbQueryResponse.result.__sizeRlist; i++) {
	std::cout << adbQueryResponse.result.Rlist[i] << " "
		  << adbQueryResponse.result.Dist[i] << " "
		  << adbQueryResponse.result.Qpos[i] << " " 
		  << adbQueryResponse.result.Spos[i] << std::endl;
      }
    } else {
      for(int i = 0; i < adbQueryResponse.result.__sizeRlist; i++) {
	std::cout << adbQueryResponse.result.Rlist[i] << " " 
		  << adbQueryResponse.result.Spos[i] << std::endl;
      }
    }
  } else {
    char fault[MAXSTR];
    soap_sprint_fault(&soap, fault, MAXSTR);
    error(fault);
  }

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
  VERB_LOG(1, "Calling %s query on database %s with %s=%s\n", (trackKey&&strlen(trackKey))?"KEY":"FILENAME", dbName, (trackKey&&strlen(trackKey))?"KEY":"FILENAME",(trackKey&&strlen(trackKey))?trackKey:featureFileName);
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
					 no_unit_norming,
					 adbQueryResponse)==SOAP_OK){
      //std::std::cerr << "result list length:" << adbQueryResponse.result.__sizeRlist << std::std::endl;
      for(int i=0; i<adbQueryResponse.result.__sizeRlist; i++)
	std::cout << adbQueryResponse.result.Rlist[i] << " " << adbQueryResponse.result.Dist[i] 
		  << " " << adbQueryResponse.result.Qpos[i] << " " << adbQueryResponse.result.Spos[i] << std::endl;
    } else {
      char fault[MAXSTR];
      soap_sprint_fault(&soap, fault, MAXSTR);
      error(fault);
    }
  } else
    ;// FIX ME: WRITE NON-SEQUENCE QUERY BY KEY ?
  
  soap_destroy(&soap);
  soap_end(&soap);
  soap_done(&soap);
}


/* handy macros */
#define INTSTRINGIFY(val, str) \
  char str[256]; \
  snprintf(str, 256, "%d", val);
#define DOUBLESTRINGIFY(val, str) \
  char str[256]; \
  snprintf(str, 256, "%f", val);

/* Server definitions */
int adb__status(struct soap* soap, xsd__string dbName, adb__statusResponse &adbStatusResponse){
  const char *argv[]={"./audioDB",COM_STATUS,"-d",dbName};
  const unsigned argc = 4;
  try {
    audioDB(argc, argv, &adbStatusResponse);
    return SOAP_OK;
  } catch(char *err) {
    soap_receiver_fault(soap, err, "");
    return SOAP_FAULT;
  }
}

int adb__liszt(struct soap* soap, xsd__string dbName, xsd__int lisztOffset, xsd__int lisztLength, 
	       adb__lisztResponse& adbLisztResponse){

  INTSTRINGIFY(lisztOffset, lisztOffsetStr);
  INTSTRINGIFY(lisztLength, lisztLengthStr);

  const char *argv[] = {"./audioDB", COM_LISZT, "-d",dbName, "--lisztOffset", lisztOffsetStr, "--lisztLength", lisztLengthStr};
  const unsigned argc = 8;
  try{
    audioDB(argc, argv, soap, &adbLisztResponse);
    return SOAP_OK;
  } catch(char *err) {
    soap_receiver_fault(soap, err, "");
    return SOAP_FAULT;
  }
} 

// Literal translation of command line to web service
int adb__query(struct soap* soap, xsd__string dbName, 
	       xsd__string qKey, xsd__string keyList, 
	       xsd__string timesFileName, xsd__string powerFileName, 
	       xsd__int qType, 
	       xsd__int qPos, xsd__int pointNN, xsd__int trackNN, 
	       xsd__int seqLen,
	       xsd__double radius, 
	       xsd__double absolute_threshold, xsd__double relative_threshold,
	       xsd__int exhaustive, xsd__int lsh_exact, xsd__int no_unit_norming,
	       adb__queryResponse &adbQueryResponse){
  char queryType[256];

  fprintf(stderr,"Calling fileName query on database %s with featureFile=%s\n", dbName, qKey);

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

  INTSTRINGIFY(qPos, qPosStr);
  INTSTRINGIFY(pointNN, pointNNStr);
  INTSTRINGIFY(trackNN, trackNNStr);
  INTSTRINGIFY(seqLen, seqLenStr);

  /* We don't necessarily use these, but because of scope we do this
     anyway.  We waste 756 bytes of stack this way. */
  DOUBLESTRINGIFY(radius, radiusStr);
  DOUBLESTRINGIFY(absolute_threshold, absolute_thresholdStr);
  DOUBLESTRINGIFY(relative_threshold, relative_thresholdStr);

  unsigned int argc = 19;
  if (powerFileName) {
    argc += 2;
  }
  if (radius != 0) {
    argc += 2;
  }
  /* we can't use use_absolute_threshold and friends because we're not
     in the audioDB class here. */
  if (absolute_threshold != 0) {
    argc += 2;
  }
  if (relative_threshold != 0) {
    argc += 2;
  }
  if (exhaustive) {
    argc++;
  }
  if (lsh_exact) {
    argc++;
  }

  if(no_unit_norming){
    argc++;
  }

  const char **argv = new const char*[argc+1];
  argv[0] = "./audioDB";
  argv[1] = COM_QUERY;
  argv[2] = queryType;
  argv[3] = COM_DATABASE;
  argv[4] = (char *) (ENSURE_STRING(dbName));
  argv[5] = COM_FEATURES;
  argv[6] = (char *) (ENSURE_STRING(qKey));
  argv[7] = COM_KEYLIST;
  argv[8] = (char *) (ENSURE_STRING(keyList));
  argv[9] = COM_TIMES;
  argv[10] = (char *) (ENSURE_STRING(timesFileName));
  argv[11] = COM_QPOINT;
  argv[12] = qPosStr;
  argv[13] = COM_POINTNN;
  argv[14] = pointNNStr;
  argv[15] = COM_TRACKNN;
  argv[16] = trackNNStr;
  argv[17] = COM_SEQLEN;
  argv[18] = seqLenStr;
  int argv_counter = 19;
  if (powerFileName) {
    argv[argv_counter++] = COM_QUERYPOWER;
    argv[argv_counter++] = powerFileName;
  }
  if (radius != 0) {
    argv[argv_counter++] = COM_RADIUS;
    argv[argv_counter++] = radiusStr;
  }
  if (absolute_threshold != 0) {
    argv[argv_counter++] = COM_ABSOLUTE_THRESH;
    argv[argv_counter++] = absolute_thresholdStr;
  }
  if (relative_threshold != 0) {
    argv[argv_counter++] = COM_RELATIVE_THRESH;
    argv[argv_counter++] = relative_thresholdStr;
  }
  if (exhaustive) {
    argv[argv_counter++] = COM_EXHAUSTIVE;
  }
  if (lsh_exact) {
    argv[argv_counter++] = COM_LSH_EXACT;
  }

  if (no_unit_norming) {
    argv[argv_counter++] = COM_NO_UNIT_NORMING;
  }

  argv[argv_counter] = NULL;

  try {
    audioDB(argc, argv, soap, &adbQueryResponse);
    delete [] argv;
    return SOAP_OK;
  } catch (char *err) {
    soap_receiver_fault(soap, err, "");
    delete [] argv;
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
			    xsd__int lsh_exact, xsd__int no_unit_norming,
			    struct adb__queryResponse& adbQueryResponse){
  char qtypeStr[256];

  fprintf(stderr, "Calling %s query on database %s with %s=%s, distFun:%s\n", (trackKey&&strlen(trackKey))?"KEY":"FILENAME", dbName, (trackKey&&strlen(trackKey))?"KEY":"FILENAME",(trackKey&&strlen(trackKey))?trackKey:featureFileName, no_unit_norming?"Euclidean":"Normed Euclidean");

  INTSTRINGIFY(queryPoint, qPosStr);
  INTSTRINGIFY(pointNN, pointNNStr);
  INTSTRINGIFY(trackNN, trackNNStr);
  INTSTRINGIFY(sequenceLength, seqLenStr);
  DOUBLESTRINGIFY(absolute_threshold, absolute_thresholdStr);
  DOUBLESTRINGIFY(radius, radiusStr);

  snprintf(qtypeStr, 256, "nsequence");
  const char *argv[]={
    "./audioDB",
    COM_QUERY,
    qtypeStr,
    COM_DATABASE,
    dbName, 
    (trackKey&&strlen(trackKey))?COM_QUERYKEY:COM_FEATURES,
    (trackKey&&strlen(trackKey))?ENSURE_STRING(trackKey):ENSURE_STRING(featureFileName),
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
    lsh_exact?COM_LSH_EXACT:"",
    no_unit_norming?COM_NO_UNIT_NORMING:"",
  };

  const unsigned argc = 23;
  
 
  try {
    audioDB(argc, argv, soap, &adbQueryResponse);
    return SOAP_OK;
  } catch (char *err) {
    soap_receiver_fault(soap, err, "");
    return SOAP_FAULT;
  }
}

#if defined(WIN32)
int mkstemp(char *tmpFileName) {
  int fd = -1;
  mktemp(tmpFileName);
  fd = open(tmpFileName,O_RDWR|O_BINARY|O_CREAT|O_EXCL, _S_IREAD|_S_IWRITE);
  return fd;
}
#endif

// Query an audioDB database by vector (serialized)
int adb__shingleQuery(struct soap* soap, xsd__string dbName, struct adb__queryVector qVector, xsd__string keyList, xsd__string timesFileName, xsd__int queryType, xsd__int queryPos, xsd__int pointNN, xsd__int trackNN, xsd__int sequenceLength, xsd__double radius, xsd__double absolute_threshold, xsd__double relative_threshold, xsd__int exhaustive, xsd__int lsh_exact, xsd__int no_unit_norming, struct adb__queryResponse &adbQueryResponse){

  // open a tmp file on the server, write shingle, query as a file with query point 0
  // and shingle length l/dim
  char tmpFileName[] = "/tmp/adb_XXXXXX";
  int tmpFid = mkstemp(tmpFileName);
  if(tmpFid==-1){
    cerr << "Cannot make tmpfile <" << tmpFileName << "> on server" << endl;
    return SOAP_FAULT;
  }

 FILE* tmpFile = fdopen(tmpFid, "r+b");
  if(!tmpFile){
    cerr << "error opening <" << tmpFileName << "> for write" << endl;
    return SOAP_FAULT;
  }

  if(fwrite(&qVector.dim, sizeof(int), 1, tmpFile)!=1){
    cerr << "error writing tmp file dim <"<< tmpFileName << ">" << endl;
    return SOAP_FAULT;
  }

  if(fwrite(qVector.v, sizeof(double), qVector.__sizev, tmpFile)!=(size_t)qVector.__sizev){
    cerr << "error writing tmp file doubles <" << tmpFileName << ">" << endl;
    return SOAP_FAULT;
  }

  // Close the file so that a new FD can be opened
  fclose(tmpFile);

  char tmpFileName2[] = "/tmp/adbP_XXXXXX";
  int tmpFid2 = 0;
  FILE* tmpFile2 = NULL;

  // Check if powers have been passed and write accordingly
  if(qVector.__sizep){
    tmpFid2 = mkstemp(tmpFileName2);
    tmpFile2 = fdopen(tmpFid2, "r+b");
    if(!tmpFile2){
      cerr << "error opening power file <" << tmpFileName2 << "> for write" << endl;
      return SOAP_FAULT;
    }
    int pSize=1;
    if(fwrite(&pSize, sizeof(int), 1, tmpFile2)!=1){
      cerr << "error writing tmp power file dim <"<< tmpFileName2 << ">" << endl;
      return SOAP_FAULT;
    }
    
    if(fwrite(qVector.p, sizeof(double), qVector.__sizep, tmpFile2)!=(size_t)qVector.__sizep){
      cerr << "error writing tmp power file doubles <" << tmpFileName2 << ">" << endl;
      return SOAP_FAULT;
    }
    fclose(tmpFile2);
  }

  // fix up sequenceLength if it isn't provided, we know what the caller wants by the size of the shingle
  // and the feature dimensionality
  if(!sequenceLength)
    sequenceLength = qVector.__sizev/qVector.dim;

  int retVal = adb__query(soap, dbName, tmpFileName, keyList, timesFileName, qVector.__sizep?tmpFileName2:0,
			  queryType, queryPos, pointNN, trackNN, sequenceLength, radius, 
			  absolute_threshold, relative_threshold, exhaustive, lsh_exact, no_unit_norming, adbQueryResponse);

  return retVal;
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
      /* FIXME: we used to have a global cache of a single LSH index
       * here.  CSR removed it because it interacted badly with
       * APIification of querying, replacing it with a per-open-adb
       * cache; we should try to take advantage of that instead.
       */

      // Server-side path prefix to databases and features
      if(adb_root)
	SERVER_ADB_ROOT = (char*)adb_root; // Server-side database root
      if(adb_feature_root)
	SERVER_ADB_FEATURE_ROOT = (char*)adb_feature_root; // Server-side features root
      
      isServer = 1;  // From this point, errors are reported via SOAP to the client
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
