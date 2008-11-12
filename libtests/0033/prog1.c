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

//${AUDIODB} -d testdb -I -f testfeature01
//${AUDIODB} -d testdb -I -f testfeature10
    myinsert.features="testfeature01";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   
    myinsert.features="testfeature10";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   

//# sequence queries require L2NORM
//${AUDIODB} -d testdb -L
    if (audiodb_l2norm(mydbp)) {returnval=-1;};

//echo "query point (0.0,0.5)"
//intstring 2 > testquery
//floatstring 0 0.5 >> testquery
    ivals[0]=2;
    dvals[0]=0; dvals[1]=0.5;
    maketestfile("testquery",ivals,dvals,2);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
//audioDB -Q sequence -d testdb -f testquery -R 5 -l 1
//echo testfeature01 1 > test-expected-output
//echo testfeature10 1 >> test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    myadbquery.radius="5";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 2) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature01",1)) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,1,"testfeature10",1)) {returnval = -1;};

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K /dev/null -R 5 > testoutput
//cat /dev/null > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.keylist="/dev/null";
    myadbquery.sequencelength="1";
    myadbquery.radius="5";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 0) {returnval = -1;};



//echo testfeature01 > testkl.txt
    makekeylistfile("testkl.txt","testfeature01");
//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K testkl.txt -R 5 > testoutput
//echo testfeature01 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    myadbquery.radius="5";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature01",1)) {returnval = -1;};

//echo testfeature10 > testkl.txt
    makekeylistfile("testkl.txt","testfeature10");
//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K testkl.txt -R 5 > testoutput
//echo testfeature10 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    myadbquery.radius="5";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature10",1)) {returnval = -1;};

//echo testfeature10 > testkl.txt
//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K testkl.txt -r 1 -R 5 > testoutput
//echo testfeature10 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    myadbquery.radius="5";
    myadbquery.resultlength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature10",1)) {returnval = -1;};


//# NB: one might be tempted to insert a test here for having both keys
//# in the keylist, but in non-database order, and then checking that
//# the result list is also in that non-database order.  I think that
//# would be misguided, as the efficient way of dealing with such a
//# keylist is to advance as-sequentially-as-possible through the
//# database; it just so happens that our current implementation is not
//# so smart.

//echo "query point (0.5,0.0)"
//intstring 2 > testquery
//floatstring 0.5 0 >> testquery
    ivals[0]=2;
    dvals[0]=0.5; dvals[1]=0.0;
    maketestfile("testquery",ivals,dvals,2);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -R 5 > testoutput
//echo testfeature01 1 > test-expected-output
//echo testfeature10 1 >> test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.keylist=NULL;
    myadbquery.sequencelength="1";
    myadbquery.radius="5";
    myadbquery.resultlength=NULL;
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 2) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature01",1)) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,1,"testfeature10",1)) {returnval = -1;};

//echo testfeature10 > testkl.txt
//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -K testkl.txt -r 1 -R 5 > testoutput
//echo testfeature10 1 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.keylist="testkl.txt";
    myadbquery.sequencelength="1";
    myadbquery.radius="5";
    myadbquery.resultlength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    /* check the test values */
    if (size != 1) {returnval = -1;};
    if (testoneradiusresult(&myadbqueryresult,0,"testfeature10",1)) {returnval = -1;};


    //fprintf(stderr,"returnval:%d\n",returnval); 
    return(returnval);
}

