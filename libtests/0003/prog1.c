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
    int ivals[10]={0};
    double dvals[10]={0.0};
    adb_insert_t myinsert={0};
    unsigned int myerr=0;
    adb_query_t myadbquery={0};
    adb_queryresult_t myadbqueryresult={0};
    char * databasename="testdb";
    int size=0;


    /* remove old directory */
    //if [ -f testdb ]; then rm -f testdb; fi
    //
    clean_remove_db(databasename);


    /* create new db */
    //${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);

    /* turn on l2norm */
    //# point query now implemented as sequence search
    //${AUDIODB} -d testdb -L
    audiodb_l2norm(mydbp);

    /* make a test file */
    //# FIXME: endianness!
    //intstring 1 > testfeature
    //floatstring 1 >> testfeature
    ivals[0]=1;
    dvals[0]=1;
    maketestfile("testfeature",ivals,dvals,1);
    
    /* insert */ 
    //${AUDIODB} -d testdb -I -f testfeature
    myinsert.features="testfeature";
    myerr=audiodb_insert(mydbp,&myinsert);   
    
    /* query */ 
    //${AUDIODB} -d testdb -Q point -f testfeature > test-query-output
    myadbquery.querytype="point"; 
    myadbquery.feature="testfeature"; 
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    
    /* check the test values */
    //echo testfeature 1 0 0 > test-expected-query-output
    //cmp test-query-output test-expected-query-output
    size=myadbqueryresult.sizeRlist;
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",1,0,0)) {returnval = -1;};   
    

//#
//## failure cases
//expect_clean_error_exit ${AUDIODB} -d testdb -I
//expect_clean_error_exit ${AUDIODB} -d testdb -f testfeature
//expect_clean_error_exit ${AUDIODB} -I -f testfeature
//expect_clean_error_exit ${AUDIODB} -d testdb -Q notpoint -f testfeature
//expect_clean_error_exit ${AUDIODB} -Q point -f testfeature
/* all of these will fail at compile time because of API */

    audiodb_close(mydbp);
      
    return(returnval);
}





