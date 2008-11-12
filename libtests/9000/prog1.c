#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
/*
 *  * #define NDEBUG
 *   * */
#include <assert.h>

#include "../../audioDB_API.h"
#include "../test_utils_lib.h"


int main(int argc, char **argv){

    int returnval=0;
    adb_ptr mydbp={0};
    int ivals[10];
    double dvals[10];
    adb_insert_t myinsert={0};
    unsigned int myerr=0;
    char * databasename="testdb";
    adb_query_t myadbquery={0};
    adb_queryresult_t myadbqueryresult={0};
    adb_query_t myadbquery2={0};
    adb_queryresult_t myadbqueryresult2={0};
    int size=0;








    returnval=-1;
      
    return(returnval);
}

