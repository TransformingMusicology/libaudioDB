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
    char * databasename="testdb";
    adb_query_t myadbquery={0};
    adb_queryresult_t myadbqueryresult={0};
    int size=0;

    /* remove old directory */
    //if [ -f testdb ]; then rm -f testdb; fi
    clean_remove_db(databasename);

    /* create new db */
    //${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);

//intstring 2 > testfeature
//floatstring 0 1 >> testfeature
//floatstring 1 0 >> testfeature
//floatstring 1 0 >> testfeature
//floatstring 0 1 >> testfeature
    ivals[0]=2;
    dvals[0]=0; dvals[1]=1; dvals[2]=1; dvals[3]=0;
    dvals[4]=1; dvals[5]=0; dvals[6]=0; dvals[7]=1;
    maketestfile("testfeature",ivals,dvals,8);

//intstring 1 > testpower
//floatstring -0.5 >> testpower
//floatstring -1 >> testpower
//floatstring -1 >> testpower
//floatstring -0.5 >> testpower
    ivals[0]=1;
    dvals[0]=-0.5; dvals[1]=-1; dvals[2]=-1; dvals[3]=-0.5;
    maketestfile("testpower",ivals,dvals,4);

//expect_clean_error_exit ${AUDIODB} -d testdb -I -f testfeature -w testpower
    myinsert.features="testfeature";
    myinsert.power="testpower";
    if (!audiodb_insert(mydbp,&myinsert)){ returnval=-1; } 

//${AUDIODB} -d testdb -P
    if(audiodb_power(mydbp)){ returnval=-1; };

//expect_clean_error_exit ${AUDIODB} -d testdb -I -f testfeature
    myinsert.features="testfeature";
    myinsert.power=NULL;
    if (!audiodb_insert(mydbp,&myinsert)){ returnval=-1; } 

//${AUDIODB} -d testdb -I -f testfeature -w testpower
    myinsert.features="testfeature";
    myinsert.power="testpower";
    if (audiodb_insert(mydbp,&myinsert)){ returnval=-1; } 
    printf("returnval:%d\n",returnval);

//# sequence queries require L2NORM
//${AUDIODB} -d testdb -L
    if(audiodb_l2norm(mydbp)){ returnval=-1; };

//echo "query points (0.0,0.5),(0.0,0.5),(0.5,0.0)"
//intstring 2 > testquery
//floatstring 0 0.5 >> testquery
//floatstring 0 0.5 >> testquery
//floatstring 0.5 0 >> testquery
    ivals[0]=2;
    dvals[0]=0; dvals[1]=0.5; dvals[2]=0; dvals[3]=0.5; dvals[4]=0.5; dvals[5]=0;
    maketestfile("testquery",ivals,dvals,6);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 0.1 > testoutput
//audioDB -Q sequence -d testdb -f testquery -R 0.1 -l 1
//echo testfeature 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    //myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    //myadbquery.qpoint="0";
    //myadbquery.absolute_threshold=0.0;
    //myadbquery.relative_threshold=0.1;
    myadbquery.radius="0.1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature",1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -p 0 -R 0.1 > testoutput
//echo testfeature 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    //myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.qpoint="0";
    //myadbquery.absolute_threshold=0.0;
    //myadbquery.relative_threshold=0.1;
    myadbquery.radius="0.1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature",1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -p 1 -R 0.1 > testoutput
//echo testfeature 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    //myadbquery.power="testquerypower";
    myadbquery.sequencelength="1";
    myadbquery.qpoint="1";
    //myadbquery.absolute_threshold=0.0;
    //myadbquery.relative_threshold=0.1;
    myadbquery.radius="0.1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature",1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -p 0 -R 1.1 > testoutput
//echo testfeature 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    //myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="0";
    //myadbquery.absolute_threshold=0.0;
    //myadbquery.relative_threshold=0.1;
    myadbquery.radius="1.1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature",1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -p 0 -R 0.9 > testoutput
//cat /dev/null > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    //myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="0";
    //myadbquery.absolute_threshold=0.0;
    //myadbquery.relative_threshold=0.1;
    myadbquery.radius="0.9";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 0) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -p 1 -R 0.9 > testoutput
//echo testfeature 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    //myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="1";
    myadbquery.radius="0.9";
    //myadbquery.absolute_threshold=0.0;
    //myadbquery.relative_threshold=0.1;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature",1)) {returnval = -1;};

//echo "query points (0.0,0.5)p=-0.5,(0.0,0.5)p=-1,(0.5,0.0)p=-1"
//intstring 1 > testquerypower
//floatstring -0.5 -1 -1 >> testquerypower
    ivals[0]=1;
    dvals[0]=-0.5; dvals[1]=-1; dvals[2]=-1;
    maketestfile("testquerypower",ivals,dvals,3);

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-1.4 -p 0 -R 1.1 > testoutput
//echo testfeature 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="0";
    myadbquery.absolute_threshold=-1.4;
    myadbquery.relative_threshold=0.0;
    myadbquery.radius="1.1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature",1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-0.8 -p 0 -R 1.1 > testoutput
//echo testfeature 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="0";
    myadbquery.absolute_threshold=-0.8;
    //myadbquery.relative_threshold=0.1;
    myadbquery.radius="1.1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature",1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-0.7 -p 0 -R 1.1 > testoutput
//cat /dev/null > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="0";
    myadbquery.absolute_threshold=-0.7;
    myadbquery.relative_threshold=0.0;
    myadbquery.radius="1.1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 0) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-1.4 -p 1 -R 0.9 > testoutput
//echo testfeature 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="1";
    myadbquery.absolute_threshold=-1.4;
    myadbquery.relative_threshold=0.0;
    myadbquery.radius="0.9";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature",1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --absolute-threshold=-0.9 -p 1 -R 0.9 > testoutput
//cat /dev/null > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="1";
    myadbquery.absolute_threshold=-0.9;
    myadbquery.relative_threshold=0.0;
    myadbquery.radius="0.9";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 0) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --relative-threshold=0.1 -p 0 -R 1.1 > testoutput
//echo testfeature 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="0";
    myadbquery.absolute_threshold=0.0;
    myadbquery.relative_threshold=0.1;
    myadbquery.radius="1.1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature",1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -w testquerypower --relative-threshold=0.1 -p 0 -R 0.9 > testoutput
//cat /dev/null > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.power="testquerypower";
    myadbquery.sequencelength="2";
    myadbquery.qpoint="0";
    myadbquery.absolute_threshold=0.0;
    myadbquery.relative_threshold=0.1;
    myadbquery.radius="0.9";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 0) {returnval = -1;};

    //returnval=-1;
    printf("returnval:%d\n",returnval);
      
    return(returnval);
}

