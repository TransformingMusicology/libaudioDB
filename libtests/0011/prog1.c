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


    //if [ -f testdb ]; then rm -f testdb; fi
    /* remove old directory */
    clean_remove_db(databasename);


    //${AUDIODB} -d testdb -N
    /* create new db */
    mydbp=audiodb_create(databasename,0,0,0);


    /* create testfeature01 file */
    //intstring 2 > testfeature
    //floatstring 0 0.5 >> testfeature
    //floatstring 0.5 0 >> testfeature
    ivals[0]=2;
    dvals[0]=0; dvals[1]=0.5; dvals[2]=0.5; dvals[3]=0;
    maketestfile("testfeature",ivals,dvals,4);

    /* insert */
    //${AUDIODB} -d testdb -I -f testfeature
    myinsert.features="testfeature";
    myerr=audiodb_insert(mydbp,&myinsert);   

    /* create testquery file */
    //echo "query point (0.0,0.5)"
    //intstring 2 > testquery
    //floatstring 0 0.5 >> testquery
    ivals[0]=2;
    dvals[0]=0.0; dvals[1]=0.5; dvals[2]=0; dvals[3]=0;
    maketestfile("testquery",ivals,dvals,2);

    /* l2norm */
    //# sequence queries require L2NORM
    //${AUDIODB} -d testdb -L
    audiodb_l2norm(mydbp);

    /* query */
    //${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
    //echo testfeature 1 0 0 > test-expected-output
    //cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",1,0,0)) {returnval = -1;};

    /* query2 */
    //${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -n 1 > testoutput
    //echo testfeature 0 0 0 > test-expected-output
    //cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    myadbquery.numpoints="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",0,0,0)) {returnval = -1;};

    /* query3 */
    //echo "query point (0.5,0.0)"
    //intstring 2 > testquery
    //floatstring 0.5 0 >> testquery
    ivals[0]=2;
    dvals[0]=0.5; dvals[1]=0;
    maketestfile("testquery",ivals,dvals,2);

    //${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
    //echo testfeature 1 0 1 > test-expected-output
    //cmp testoutput test-expected-output
    myadbquery2.querytype="sequence";
    myadbquery2.feature="testquery";
    myadbquery2.sequencelength="1";
    audiodb_query(mydbp,&myadbquery2,&myadbqueryresult2);
    size=myadbqueryresult2.sizeRlist;


    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult2,0,"testfeature",1,0,1)) {returnval = -1;};


    /* query4 */
    //${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -n 1 > testoutput
    //echo testfeature 0 0 1 > test-expected-output
    //cmp testoutput test-expected-output
    myadbquery2.querytype="sequence";
    myadbquery2.feature="testquery";
    myadbquery2.sequencelength="1";
    myadbquery2.numpoints="1";
    audiodb_query(mydbp,&myadbquery2,&myadbqueryresult2);
    size=myadbqueryresult2.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult2,0,"testfeature",0,0,1)) {returnval = -1;};



    return(returnval);
}

