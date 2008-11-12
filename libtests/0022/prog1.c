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
    adb_query_t myadbquery2={0};
    adb_queryresult_t myadbqueryresult2={0};
    adb_insert_t ins1[2]={{0},{0}};
    int size=0;

    /* remove old directory */
    //if [ -f testdb ]; then rm -f testdb; fi
    clean_remove_db(databasename);

    /* create new db */
    //${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);

    if (!mydbp){ returnval=-1;};

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


//cat > testfeaturefiles <<EOF
//testfeature01
//testfeature10
//EOF

    ins1[0].features="testfeature01";
    ins1[1].features="testfeature10";

//audioDB --BATCHINSERT -d testdb --featureList tempfeatures 
//${AUDIODB} -d testdb -B -F testfeaturefiles

    if(audiodb_batchinsert(mydbp,ins1,2)){
        returnval=-1;
    };


//# sequence queries require L2NORM
//${AUDIODB} -d testdb -L
    if(audiodb_l2norm(mydbp)){
        returnval=-1;
    };

//echo "query point (0.0,0.5)"
//intstring 2 > testquery
//floatstring 0 0.5 >> testquery
    ivals[0]=2;
    dvals[0]=0; dvals[1]=0.5;
    maketestfile("testquery",ivals,dvals,2);

//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
//echo testfeature01 0 0 0 > test-expected-output
//echo testfeature10 2 0 0 >> test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);
    ///* check the test values */
    if (size != 2) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature01",0,0,0)) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,1,"testfeature10",2,0,0)) {returnval = -1;};


//${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 > testoutput
//echo testfeature01 0 0 0 > test-expected-output
//cmp testoutput test-expected-output
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="1";
    myadbquery.resultlength="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult,0,"testfeature01",0,0,0)) {returnval = -1;};
//echo "query point (0.5,0.0)"
//intstring 2 > testquery
//floatstring 0.5 0 >> testquery
    ivals[0]=2;
    dvals[0]=0.5; dvals[1]=0;
    maketestfile("testquery",ivals,dvals,2);

////${AUDIODB} -d testdb -Q sequence -l 1 -f testquery > testoutput
////echo testfeature10 0 0 0 > test-expected-output
////echo testfeature01 2 0 0 >> test-expected-output
////cmp testoutput test-expected-output
    myadbquery2.querytype="sequence";
    myadbquery2.feature="testquery";
    myadbquery2.sequencelength="1";
    audiodb_query(mydbp,&myadbquery2,&myadbqueryresult2);
    size=myadbqueryresult2.sizeRlist;

    if (size != 2) {returnval = -1;};
    if (testoneresult(&myadbqueryresult2,0,"testfeature10",0,0,0)) {returnval = -1;};
    if (testoneresult(&myadbqueryresult2,1,"testfeature01",2,0,0)) {returnval = -1;};


////${AUDIODB} -d testdb -Q sequence -l 1 -f testquery -r 1 > testoutput
////echo testfeature10 0 0 0 > test-expected-output
////cmp testoutput test-expected-output
    myadbquery2.querytype="sequence";
    myadbquery2.feature="testquery";
    myadbquery2.sequencelength="1";
    myadbquery2.resultlength="1";
    audiodb_query(mydbp,&myadbquery2,&myadbqueryresult2);
    size=myadbqueryresult2.sizeRlist;

    if (size != 1) {returnval = -1;};
    if (testoneresult(&myadbqueryresult2,0,"testfeature10",0,0,0)) {returnval = -1;};

    audiodb_close(mydbp);
//    fprintf(stderr,"returnval:%d\n",returnval);
      
    return(returnval);
}

