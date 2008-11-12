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
    int myerror=0;


    /* remove old directory */
    //if [ -f testdb ]; then rm -f testdb; fi
    clean_remove_db(databasename);

    /* create new db */
    //${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);


    /* make a test file */
    //intstring 2 > testfeature
    //floatstring 0 1 >> testfeature
    //floatstring 1 0 >> testfeature
    ivals[0]=2;
    dvals[0]=0; dvals[1]=1; dvals[2]=1; dvals[3]=0;
    maketestfile("testfeature",ivals,dvals,4);

    
    /* insert */ 
    //${AUDIODB} -d testdb -I -f testfeature
    myinsert.features="testfeature";
    myerr=audiodb_insert(mydbp,&myinsert);   

    /* turn on l2norm */
    //echo running L2Norm
    //${AUDIODB} -d testdb -L
    myerror=audiodb_l2norm(mydbp);
    if (myerror){
        returnval=-1;
    }


    /* close */
    audiodb_close(mydbp);


      
    return(returnval);
}

