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

//intstring 2 > testfeature1
//floatstring 0 1 >> testfeature1
    ivals[0]=2;
    dvals[0]=0; dvals[1]=1;
    maketestfile("testfeature1",ivals,dvals,2);

//intstring 2 > testfeature3
//floatstring 1 0 >> testfeature3
//floatstring 0 1 >> testfeature3
//floatstring 1 0 >> testfeature3
    ivals[0]=2;
    dvals[0]=1; dvals[1]=0;
    dvals[2]=0; dvals[3]=1;
    dvals[4]=1; dvals[5]=0;
    maketestfile("testfeature3",ivals,dvals,6);

//${AUDIODB} -d testdb -I -f testfeature1
    myinsert.features="testfeature1";
    if (audiodb_insert(mydbp,&myinsert)){ returnval=-1; } 
//${AUDIODB} -d testdb -I -f testfeature3
    myinsert.features="testfeature3";
    if (audiodb_insert(mydbp,&myinsert)){ returnval=-1; } 

//# sequence queries require L2NORM
//${AUDIODB} -d testdb -L
    if(audiodb_l2norm(mydbp)){ returnval=-1; };

//echo "query point (0 1, 1 0)"
//intstring 2 > testquery
//floatstring 0 1 >> testquery
//floatstring 1 0 >> testquery
    ivals[0]=2;
    dvals[0]=0; dvals[1]=1;
    dvals[2]=1; dvals[3]=0;
    maketestfile("testquery",ivals,dvals,4);

//audioDB -Q sequence -d testdb -f testquery -n 1 -l 2
//${AUDIODB} -d testdb -Q sequence -l 2 -f testquery -n 1 > testoutput
    myadbquery.querytype="sequence";
    myadbquery.feature="testquery";
    myadbquery.sequencelength="2";
    myadbquery.numpoints="1";
    audiodb_query(mydbp,&myadbquery,&myadbqueryresult);
    size=myadbqueryresult.sizeRlist;

    //dump_query(&myadbquery,&myadbqueryresult);

    /* check the test values */
////wc -l testoutput | grep "1 testoutput"
////grep "^testfeature3 .* 0 1$" testoutput
    if (size != 1) {returnval = -1;};
    if (strcmp(myadbqueryresult.Rlist[0],"testfeature3")){ returnval = -1; };

    //printf("returnval:%d\n",returnval);  
    return(returnval);
}

