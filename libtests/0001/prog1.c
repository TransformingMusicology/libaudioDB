#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
/*
 *  * #define NDEBUG
 *   * */
#include <assert.h>

#include "../../audioDB_API.h"
#include "../test_utils_lib.h"


int main(int argc, char **argv){

    int returnval=0;
    adb_ptr mydbp={0};
    adb_ptr mydbp2={0};
    struct stat statbuf;
    int statval=0;

    char * databasename="testdb";

    //if [ -f testdb ]; then rm -f testdb; fi
    /* remove old directory */
    clean_remove_db(databasename);

    /* create new db */
    //# creation
    //${AUDIODB} -N -d testdb
    mydbp=audiodb_open(databasename);


    /* open should fail (return NULL), so create a new db */
    if (!mydbp){
        mydbp=audiodb_create(databasename,0,0,0);
    }



    if (!mydbp){
        printf("fail\n");
        returnval=-1;
    }
    

    /* stat testdb - let's make sure that it is there */
    //stat testdb
    statval=stat(databasename, &statbuf);

    if (statval){
       returnval=-1;
    }
    
    audiodb_close(mydbp);

    /* try to create should fail, because db exists now */
    mydbp2=audiodb_create(databasename,0,0,0);

    if (mydbp2){
        returnval=-1;
    }


/* should pass now - db exists */ 
//expect_clean_error_exit ${AUDIODB} -N -d testdb
    mydbp2=audiodb_open(databasename);
    if (!mydbp2){
       returnval=-1;
    }

//this test would fail at compile time because of the API interface
//# should fail (no db given)
//expect_clean_error_exit ${AUDIODB} -N


    audiodb_close(mydbp2);

//    printf("returnval:%d\n",returnval);

    return(returnval);
}

