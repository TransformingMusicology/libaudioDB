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
    adb_status_t mystatus={0};

    char * databasename="testdb";

//. ../test-utils.sh
//
//if [ -f testdb ]; then rm -f testdb; fi
//
    /* remove old directory */
    clean_remove_db(databasename);

    /* create new db */
//${AUDIODB} -N -d testdb
//
    mydbp=audiodb_create(databasename,0,0,0);


//# FIXME: at some point we will want to test that some relevant
//# information is being printed
//${AUDIODB} -S -d testdb
//${AUDIODB} -d testdb -S

    if(audiodb_status(mydbp,&mystatus)){
        returnval=-1;
    }

/* not relevent, caught by API */
//# should fail (no db given)
//expect_clean_error_exit ${AUDIODB} -S



    audiodb_close(mydbp);

    return(returnval);
}

