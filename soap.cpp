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
    std::cout << "flags = " << adbStatusResponse.result.flags << std::endl;
  } else {
    soap_print_fault(&soap,stderr);
  }
  
  soap_destroy(&soap);
  soap_end(&soap);
  soap_done(&soap);
}

void audioDB::ws_query(const char*dbName, const char *trackKey, const char* hostport){
  struct soap soap;
  adb__queryResponse adbQueryResponse;  

  soap_init(&soap);  
  if(soap_call_adb__query(&soap,hostport,NULL,
			  (char*)dbName,(char*)trackKey,(char*)trackFileName,(char*)timesFileName,
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

/* Server definitions */
int adb__status(struct soap* soap, xsd__string dbName, adb__statusResponse &adbStatusResponse){
  char* const argv[]={"audioDB",COM_STATUS,"-d",dbName};
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
  else
    strncpy(queryType, "", strlen(""));

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

int adb__sequenceQuery(struct soap* soap, xsd__string dbName, xsd__string qKey,
		       adb__sequenceQueryParms *parms,
		       adb__queryResponse &adbQueryResponse) {

  char qPosStr[256];
  char pointNNStr[256];
  char trackNNStr[256];
  char seqLenStr[256];
  char relative_thresholdStr[256];
  char absolute_thresholdStr[256];

  /* When the branch is merged, move this to a header and use it
     elsewhere */
#define INTSTRINGIFY(val, str) \
  snprintf(str, 256, "%d", val);
#define DOUBLESTRINGIFY(val, str) \
  snprintf(str, 256, "%f", val);

  INTSTRINGIFY(parms->qPos, qPosStr);
  INTSTRINGIFY(parms->pointNN, pointNNStr);
  INTSTRINGIFY(parms->segNN, trackNNStr);
  /* FIXME: decide which of segLen and seqLen should live */
  INTSTRINGIFY(parms->segLen, seqLenStr);

  DOUBLESTRINGIFY(parms->relative_threshold, relative_thresholdStr);
  DOUBLESTRINGIFY(parms->absolute_threshold, absolute_thresholdStr);
  
  const char *argv[] = {
    "./audioDB",
    COM_QUERY,
    "sequence",
    COM_DATABASE,
    dbName, 
    COM_FEATURES,
    qKey,
    COM_KEYLIST,
    /* FIXME: when this branch is merged, use ENSURE_STRING */
    parms->keyList==0?"":parms->keyList,
    COM_TIMES,
    parms->timesFileName==0?"":parms->timesFileName,
    COM_QUERYPOWER,
    parms->powerFileName==0?"":parms->powerFileName,
    COM_QPOINT, 
    qPosStr,
    COM_POINTNN,
    pointNNStr,
    COM_TRACKNN,
    trackNNStr,
    COM_SEQLEN,
    seqLenStr,
    COM_RELATIVE_THRESH,
    relative_thresholdStr,
    COM_ABSOLUTE_THRESH,
    absolute_thresholdStr
  };

  const unsigned argc = 25;

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
