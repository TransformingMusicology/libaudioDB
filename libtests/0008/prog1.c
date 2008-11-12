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




    /* remove old directory */
    clean_remove_db(databasename);



//${AUDIODB} -d testdb -N
    /* create new db */
    mydbp=audiodb_create(databasename,0,0,0);


//intstring 2 > testfeature01
//floatstring 0 1 >> testfeature01
//intstring 2 > testfeature10
//floatstring 1 0 >> testfeature10

    /* create testfeature01 file */
    ivals[0]=2;
    dvals[0]=0; dvals[1]=1; dvals[2]=0; dvals[3]=0;
    maketestfile("testfeature01",ivals,dvals,2);

    /* create testfeature10 file */
    ivals[0]=2;
    dvals[0]=1; dvals[1]=0; dvals[2]=0; dvals[3]=0;
    maketestfile("testfeature10",ivals,dvals,2);

//${AUDIODB} -d testdb -I -f testfeature01
//${AUDIODB} -d testdb -I -f testfeature10

    /* insert */
    myinsert.features="testfeature01";
    myerr=audiodb_insert(mydbp,&myinsert);   

    myinsert.features="testfeature10";
    myerr=audiodb_insert(mydbp,&myinsert);   

//# sequence queries require L2NORM
//${AUDIODB} -d testdb -L
    audiodb_l2norm(mydbp);

//echo "query point (0.0,0.5)"
//intstring 2 > testquery
//floatstring 0 0.5 >> testquery

    /* create testquery file */
    ivals[0]=2;
    dvals[0]=0.0; dvals[1]=0.5; dvals[2]=0; dvals[3]=0;
    maketestfile("testquery",ivals,dvals,2);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
//echo testfeature01 0 0 0 > test-expected-output
//echo testfeature10 2 0 0 >> test-expected-output
//cmp testoutput test-expected-output

    /* query */
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;



    /* check the test values */
    if (size != 2) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature01",0,0,0)) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,1,"testfeature10",2,0,0)) {returnval = -1;};

////${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 > testoutput
////echo testfeature01 0 0 0 > test-expected-output
////cmp testoutput test-expected-output

    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    myadbquery.resultlength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature01",0,0,0)) {returnval = -1;};

//echo "query point (0.5,0.0)"
//intstring 2 > testquery
//floatstring 0.5 0 >> testquery

    /* create testquery file */
    ivals[0]=2;
    dvals[0]=0.5; dvals[1]=0.0; dvals[2]=0; dvals[3]=0;
    maketestfile("testquery",ivals,dvals,2);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
//echo testfeature10 0 0 0 > test-expected-output
//echo testfeature01 2 0 0 >> test-expected-output
//cmp testoutput test-expected-output

    myadbquery2.querytype="sequence";
    myadbquery2.feature="testquery";
    myadbquery2.sequencelength="1";
    audiodb_query(mydbp,&myadbquery2,&myadbqueryresult2);
    size=myadbqueryresult2.sizeRlist;

    /* check the test values */
    if (size != 2) {returnval = -1;};
    if (testoneresult(&myadbqueryresult2,0,"testfeature10",0,0,0)) {returnval = -1;};
    if (testoneresult(&myadbqueryresult2,1,"testfeature01",2,0,0)) {returnval = -1;};


//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 > testoutput
//echo testfeature10 0 0 0 > test-expected-output
//cmp testoutput test-expected-output


    myadbquery2.querytype="sequence";
    myadbquery2.feature="testquery";
    myadbquery2.sequencelength="1";
    myadbquery2.resultlength="1";
    audiodb_query(mydbp,&myadbquery2,&myadbqueryresult2);
    size=myadbqueryresult2.sizeRlist;


    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult2,0,"testfeature10",0,0,0)) {returnval = -1;};


      
    return(returnval);
}

