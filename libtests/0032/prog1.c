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

    /* turn on l2 power */
//${AUDIODB} -d testdb -L
    if (audiodb_l2norm(mydbp)) {returnval=-1;};

    /* make feature files */
//intstring 2 > testfeature01
//floatstring 0 1 >> testfeature01
//intstring 2 > testfeature10
//floatstring 1 0 >> testfeature10
    ivals[0]=2;
    dvals[0]=0; dvals[1]=1;
    maketestfile("testfeature01",ivals,dvals,2);
    ivals[0]=2;
    dvals[0]=1; dvals[1]=0;
    maketestfile("testfeature10",ivals,dvals,2);


    /* insertions */
    //${AUDIODB} -d testdb -I -f testfeature01
    //${AUDIODB} -d testdb -I -f testfeature10
    myinsert.features="testfeature01";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   
    myinsert.features="testfeature10";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   
    

    /* query */
    //echo "query point (0.0,0.5)"
    //intstring 2 > testquery
    //floatstring 0 0.5 >> testquery
    ivals[0]=2;
    dvals[0]=0; dvals[1]=0.5;
    maketestfile("testquery",ivals,dvals,2);

    /* test a sequence query */
    //${AUDIODB} -d testdb -Q track -l 1 -f testquery > testoutput
    //echo testfeature01 0.5 0 0 > test-expected-output
    //echo testfeature10 0 0 0 >> test-expected-output
    //cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 2) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature01",0.5,0,0)) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,1,"testfeature10",0,0,0)) {returnval = -1;};

//${AUDIODB} -d testdb -Q track -l 1 -f testquery -K /dev/null > testoutput
//cat /dev/null > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.keylist="/dev/null";
    myadbquery.sequencelength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 0) {returnval = -1;};



//echo testfeature01 > testkl.txt
    makekeylistfile("testkl.txt","testfeature01");
//${AUDIODB} -d testdb -Q track -l 1 -f testquery -K testkl.txt > testoutput
//echo testfeature01 0.5 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);

    size=myadbqueryresult.sizeRlist;
    //dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature01",0.5,0,0)) {returnval = -1;};




//echo testfeature10 > testkl.txt
    makekeylistfile("testkl.txt","testfeature10");
//${AUDIODB} -d testdb -Q track -l 1 -f testquery -K testkl.txt > testoutput
//echo testfeature10 0 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);

    size=myadbqueryresult.sizeRlist;
    //dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature10",0,0,0)) {returnval = -1;};



//echo testfeature10 > testkl.txt
    makekeylistfile("testkl.txt","testfeature10");
//${AUDIODB} -d testdb -Q track -l 1 -f testquery -K testkl.txt -r 1 > testoutput
//echo testfeature10 0 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    myadbquery.resultlength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);

    size=myadbqueryresult.sizeRlist;
    //dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature10",0,0,0)) {returnval = -1;};

//echo "query point (0.5,0.0)"
//intstring 2 > testquery
//floatstring 0.5 0 >> testquery
    ivals[0]=2;
    dvals[0]=0.5; dvals[1]=0.0;
    maketestfile("testquery",ivals,dvals,2);

//${AUDIODB} -d testdb -Q track -l 1 -f testquery > testoutput
//echo testfeature10 0.5 0 0 > test-expected-output
//echo testfeature01 0 0 0 >> test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.keylist=NULL;
    myadbquery.sequencelength="1";
    myadbquery.resultlength=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);

    size=myadbqueryresult.sizeRlist;
    //dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
    if (size != 2) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature10",0.5,0,0)) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,1,"testfeature01",0,0,0)) {returnval = -1;};

//${AUDIODB} -d testdb -Q track -l 1 -f testquery -K /dev/null > testoutput
//cat /dev/null > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.keylist="/dev/null";
    myadbquery.sequencelength="1";
    myadbquery.resultlength=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);

    size=myadbqueryresult.sizeRlist;
    //dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
    if (size != 0) {returnval = -1;};

//echo testfeature10 > testkl.txt
    makekeylistfile("testkl.txt","testfeature10");
//${AUDIODB} -d testdb -Q track -l 1 -f testquery -K testkl.txt > testoutput
//echo testfeature10 0.5 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    myadbquery.resultlength=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);

    size=myadbqueryresult.sizeRlist;
    //dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature10",0.5,0,0)) {returnval = -1;};

//echo testfeature01 > testkl.txt
    makekeylistfile("testkl.txt","testfeature01");
//${AUDIODB} -d testdb -Q track -l 1 -f testquery -K testkl.txt > testoutput
//echo testfeature01 0 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);

    size=myadbqueryresult.sizeRlist;
    //dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature01",0,0,0)) {returnval = -1;};

//echo testfeature01 > testkl.txt
    makekeylistfile("testkl.txt","testfeature01");
//${AUDIODB} -d testdb -Q track -l 1 -f testquery -K testkl.txt -r 1 > testoutput
//echo testfeature01 0 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="track";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    myadbquery.resultlength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);

    size=myadbqueryresult.sizeRlist;
    //dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature01",0,0,0)) {returnval = -1;};




//    printf("returnval:%d\n", returnval);
      
    return(returnval);
}

