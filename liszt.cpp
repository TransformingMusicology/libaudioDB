#include "audioDB.h"

void audioDB::liszt(const char* dbName, unsigned offset, unsigned numLines, adb__lisztResponse* adbLisztResponse){
  if(!dbH) {
    initTables(dbName, 0);
  }

  assert(trackTable && fileTable);

  if(offset>dbH->numFiles){
    char tmpStr[MAXSTR];
    sprintf(tmpStr, "numFiles=%u, lisztOffset=%u", dbH->numFiles, offset);
    error("listKeys offset out of range", tmpStr);
  }

  if(!adbLisztResponse){
    for(Uns32T k=0; k<numLines && offset+k<dbH->numFiles; k++){
      fprintf(stdout, "[%d] %s (%d)\n", offset+k, fileTable+(offset+k)*O2_FILETABLE_ENTRY_SIZE, trackTable[offset+k]);
    }
  }
  else{
    adbLisztResponse->result.Rkey = new char*[numLines];
    adbLisztResponse->result.Rlen = new unsigned int[numLines];
    Uns32T k = 0;
    for( ; k<numLines && offset+k<dbH->numFiles; k++){    
      adbLisztResponse->result.Rkey[k] = new char[MAXSTR];
      snprintf(adbLisztResponse->result.Rkey[k], O2_MAXFILESTR, "%s", fileTable+(offset+k)*O2_FILETABLE_ENTRY_SIZE);
      adbLisztResponse->result.Rlen[k] = trackTable[offset+k];
    }
    adbLisztResponse->result.__sizeRkey = k;
    adbLisztResponse->result.__sizeRlen = k;
  }
  
}
