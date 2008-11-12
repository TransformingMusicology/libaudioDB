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
    // adbquery myadbquery2={0};
    // adbqueryresult myadbqueryresult2={0};
    int size=0;

    /* remove old directory */
    clean_remove_db(databasename);

    /* create new db */
    mydbp=audiodb_create(databasename,0,0,0);

//# tests that the lack of -l when the query sequence is shorter doesn't
//# segfault.

    /* make test file */
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
    if(myerr){ returnval=-1; };


    /* turn on l2norm */
    //# sequence queries require L2NORM
    //${AUDIODB} -d testdb -L
    if(audiodb_l2norm(mydbp)){ returnval=-1; };


    /* make query */
    //echo "query point (0.0,0.5)"
    //intstring 2 > testquery
    //floatstring 0 0.5 >> testquery
    ivals[0]=2;
    dvals[0]=0; dvals[1]=0.5;
    maketestfile("testquery",ivals,dvals,2);


/* should fail */

//audioDB -Q sequence -d testdb -f testquery
//expect_clean_error_exit ${AUDIODB} -d testdb -Q sequence -f testquery
    
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    //myadbquery.sequencelength="1";
    myerr=audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;
    if (!myerr){ returnval = -1;};


///* should fail */
//expect_clean_error_exit ${AUDIODB} -d testdb -Q sequence -f testquery -n 1
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.numpoints="1";
    myerr=audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    if(!myerr){ returnval=-1; };

/* query 2 */
//echo "query point (0.5,0.0)"
//intstring 2 > testquery
//floatstring 0.5 0 >> testquery
    ivals[0]=2;
    dvals[0]=0.5; dvals[1]=0.0;
    maketestfile("testquery",ivals,dvals,2);

/* should fail */
//expect_clean_error_exit ${AUDIODB} -d testdb -Q sequence -f testquery
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.numpoints=NULL;
    myerr=audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    if(!myerr){ returnval=-1; };

/* should fail */
//expect_clean_error_exit ${AUDIODB} -d testdb -Q sequence -f testquery -n 1
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.numpoints="1";
    myerr=audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    if(!myerr){ returnval=-1; };




    //printf("returnval:%d\n", returnval);
      
    return(returnval);
}

