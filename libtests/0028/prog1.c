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
    char * databasename="testdb";
    adb_query_t myadbquery={0};
    adb_queryresult_t myadbqueryresult={0};
    int size=0;
    adb_insert_t ins1[2]={{0},{0}};


    /* remove old directory */
    //if [ -f testdb ]; then rm -f testdb; fi
    clean_remove_db(databasename);

    /* create new db */
    //${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);

//intstring 2 > testfeature
//floatstring 0 1 >> testfeature
//floatstring 1 0 >> testfeature
    ivals[0]=2;
    dvals[0]=0; dvals[1]=1; dvals[2]=1; dvals[3]=0;
    maketestfile("testfeature",ivals,dvals,4);

//intstring 1 > testpower
//floatstring -0.5 >> testpower
//floatstring -1 >> testpower
    ivals[0]=1;
    dvals[0]=-0.5; dvals[1]=-1;
    maketestfile("testpower",ivals,dvals,2);

//echo testfeature > testFeatureList.txt
//echo testpower > testPowerList.txt
    ins1[0].features="testfeature";
    ins1[0].power="testpower";

//expect_clean_error_exit ${AUDIODB} -d testdb -B -F testFeatureList.txt -W testPowerList.txt
    if(!audiodb_batchinsert(mydbp,ins1,1)){
        returnval=-1;
    };

//${AUDIODB} -d testdb -P
    if(audiodb_power(mydbp)){
        returnval=-1;
    };

//expect_clean_error_exit ${AUDIODB} -d testdb -B -F testFeatureList.txt
    ins1[0].features="testfeature";
    ins1[0].power=NULL;
    if(!audiodb_batchinsert(mydbp,ins1,1)){
        returnval=-1;
    };

//${AUDIODB} -d testdb -B -F testFeatureList.txt -W testPowerList.txt
    ins1[0].features="testfeature";
    ins1[0].power="testpower";
    if(audiodb_batchinsert(mydbp,ins1,1)){
        returnval=-1;
    };

//# sequence queries require L2NORM
//${AUDIODB} -d testdb -L
    if(audiodb_l2norm(mydbp)){
        returnval=-1;
    };

//# queries without power files should run as before
//echo "query point (0.0,0.5)"
//intstring 2 > testquery
//floatstring 0 0.5 >> testquery
    ivals[0]=2;
    dvals[0]=-0.0; dvals[1]=0.5;
    maketestfile("testquery",ivals,dvals,2);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
//echo testfeature 1 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",1,0,0)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -n 1 > testoutput
//echo testfeature 0 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    myadbquery.numpoints="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",0,0,0)) {returnval = -1;};

//echo "query point (0.5,0.0)"
//intstring 2 > testquery
//floatstring 0.5 0 >> testquery
    ivals[0]=2;
    dvals[0]=0.5; dvals[1]=0.0;
    maketestfile("testquery",ivals,dvals,2);


//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
//echo testfeature 1 0 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",1,0,1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -n 1 > testoutput
//echo testfeature 0 0 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    myadbquery.numpoints="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",0,0,1)) {returnval = -1;};

//# queries with power files might do something different
//echo "query point (0.0,0.5), p=-0.5"
//intstring 2 > testquery
//floatstring 0 0.5 >> testquery
    ivals[0]=2;
    dvals[0]=0.0; dvals[1]=0.5;
    maketestfile("testquery",ivals,dvals,2);

//intstring 1 > testquerypower
//floatstring -0.5 >> testquerypower
    ivals[0]=1;
    dvals[0]=-0.5;
    maketestfile("testquerypower",ivals,dvals,1);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-1.4 > testoutput
//echo testfeature 1 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.absolute_threshold=-1.4;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",1,0,0)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-0.6 > testoutput
//echo testfeature 0 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.absolute_threshold=-0.6;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",0,0,0)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-0.2 > testoutput
//cat /dev/null > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.absolute_threshold=-0.2;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 0) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --relative-threshold=1 > testoutput
//audioDB -Q sequence -d testdb -f testquery -w testquerypower -l 1 --relative-threshold 1.000000
//echo testfeature 1 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.absolute_threshold=0.0;
    myadbquery.relative_threshold=1.0;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",1,0,0)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --relative-threshold=0.2 > testoutput
//echo testfeature 0 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.absolute_threshold=0.0;
    myadbquery.relative_threshold=0.2;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",0,0,0)) {returnval = -1;};

////echo "query point (0.5,0.0), p=-0.5"
////intstring 2 > testquery
////floatstring 0.5 0 >> testquery
    ivals[0]=2;
    dvals[0]=0.5; dvals[1]=0.0;
    maketestfile("testquery",ivals,dvals,2);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-1.4 > testoutput
//echo testfeature 1 0 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.absolute_threshold=-1.4;
    myadbquery.relative_threshold=0.0;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

//    dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",1,0,1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-0.6 > testoutput
//echo testfeature 2 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.absolute_threshold=-0.6;
    myadbquery.relative_threshold=0.0;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",2,0,0)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --absolute-threshold=-0.2 > testoutput
//cat /dev/null > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.absolute_threshold=-0.2;
    myadbquery.relative_threshold=0.0;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 0) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --relative-threshold=1 > testoutput
//echo testfeature 1 0 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.relative_threshold=1.0;
    myadbquery.absolute_threshold=0.0;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",1,0,1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -w testquerypower --relative-threshold=0.2 > testoutput
//echo testfeature 2 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.relative_threshold=0.2;
    myadbquery.absolute_threshold=0.0;
    myadbquery.numpoints=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    ///* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature",2,0,0)) {returnval = -1;};


    audiodb_close(mydbp);
    //fprintf(stderr,"returnval:%d\n",returnval);
      
    return(returnval);
}

