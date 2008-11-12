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
    // int ivals[10];
    // double dvals[10];
    // adbinsert myinsert={0};
    // unsigned int myerr=0;
    char * databasename="testdb";
    // adbquery myadbquery={0};
    // adbqueryresult myadbqueryresult={0};
    // adbquery myadbquery2={0};
    // adbqueryresult myadbqueryresult2={0};
    int myerror=0;


    /* remove old directory */
    //if [ -f testdb ]; then rm -f testdb; fi
    clean_remove_db(databasename);

    /* create new db */
    //${AUDIODB} -N -d testdb
    mydbp=audiodb_create(databasename,0,0,0);

    /* power flag on */
    //${AUDIODB} -P -d testdb
    //${AUDIODB} -d testdb -P
    myerror=audiodb_power(mydbp); 
    if (myerror){
        returnval=-1;
    }

    //# should fail (no db given)
    //expect_clean_error_exit ${AUDIODB} -P
    /* not relevent, API wouldn't compile */

    return(returnval);
}

