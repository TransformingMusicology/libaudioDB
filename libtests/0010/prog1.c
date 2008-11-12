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
    int size=0;

/* clean */
//if [ -f testdb ]; then rm -f testdb; fi
    clean_remove_db(databasename);

/* new db */
//${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);

/* test feature files */
//intstring 2 > testfeature01
//floatstring 0 1 >> testfeature01
    ivals[0]=2;
    dvals[0]=0; dvals[1]=1;
    maketestfile("testfeature01",ivals,dvals,2);
//intstring 2 > testfeature10
//floatstring 1 0 >> testfeature10
    ivals[0]=2;
    dvals[0]=1; dvals[1]=0;
    maketestfile("testfeature10",ivals,dvals,2);

/* inserts */
//${AUDIODB} -d testdb -I -f testfeature01
//${AUDIODB} -d testdb -I -f testfeature10
    myinsert.features="testfeature01";
    myerr=audiodb_insert(mydbp,&myinsert);   
    myinsert.features="testfeature10";
    myerr=audiodb_insert(mydbp,&myinsert);   

/* l2norm */
//# sequence queries require L2NORM
//${AUDIODB} -d testdb -L
    audiodb_l2norm(mydbp);

/* query 1 */
//echo "query point (0.0,0.5)"
//intstring 2 > testquery
//floatstring 0 0.5 >> testquery
    ivals[0]=2;
    dvals[0]=0; dvals[1]=0.5; dvals[2]=0; dvals[3]=0;
    maketestfile("testquery",ivals,dvals,2);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
//echo testfeature01 1 > test-expected-output
//echo testfeature10 1 >> test-expected-output
//cmp testoutput test-expected-output
  
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    myadbquery.radius="5";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    dump_query(&myadbquery,&myadbqueryresult);
    /* check the test values */
    if (size != 2) {returnval = -1;};
    //if (testoneresult(&myadbqueryresult,0,"testfeature",1,0,0)) {returnval = -1;};
    //if (testoneresult(&myadbqueryresult,1,"testfeature",1,0,0)) {returnval = -1;};
  
  
/* query 2 */
//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 -R 5 > testoutput
//echo testfeature01 1 > test-expected-output
//cmp testoutput test-expected-output
//
//echo "query point (0.5,0.0)"
//intstring 2 > testquery
//floatstring 0.5 0 >> testquery


/* query 3 */
//# FIXME: because there's only one point in each track (and the query),
//# the ordering is essentially database order.  We need these test
//# cases anyway because we need to test non-segfaulting, non-empty
//# results...
//
//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
//echo testfeature01 1 > test-expected-output
//echo testfeature10 1 >> test-expected-output
//cmp testoutput test-expected-output



/* query 4 */
//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 -R 5 > testoutput
//echo testfeature01 1 > test-expected-output
//cmp testoutput test-expected-output


    printf("returnval:%d\n",returnval);
    //returnval=-1;
      
    return(returnval);
}

