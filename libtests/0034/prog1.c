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
    adb_status_t mystatus={0};
    adb_insert_t ins1[3]={{0},{0},{0}};



    /* remove old directory */
//if [ -f testdb ]; then rm -f testdb; fi
    clean_remove_db(databasename);

    /* create new db */
//${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);


//intstring 2 > testfeature
//floatstring 1 1 >> testfeature
//intstring 2 > testfeature01
//floatstring 0 1 >> testfeature01
//intstring 2 > testfeature10
//floatstring 1 0 >> testfeature10
    ivals[0]=2;
    dvals[0]=1; dvals[1]=1;
    maketestfile("testfeature",ivals,dvals,2);
    ivals[0]=2;
    dvals[0]=0; dvals[1]=1;
    maketestfile("testfeature01",ivals,dvals,2);
    ivals[0]=2;
    dvals[0]=1; dvals[1]=0;
    maketestfile("testfeature10",ivals,dvals,2);

//${AUDIODB} -d testdb -I -f testfeature
    myinsert.features="testfeature";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   

//${AUDIODB} -d testdb -S | grep "num files:1"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 1) { returnval = -1; }

//${AUDIODB} -d testdb -I -f testfeature
    myinsert.features="testfeature";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   

//${AUDIODB} -d testdb -S | grep "num files:1"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 1) { returnval = -1; }

//${AUDIODB} -d testdb -I -f testfeature01
    myinsert.features="testfeature01";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   

//${AUDIODB} -d testdb -S | grep "num files:2"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 2) { returnval = -1; }

//${AUDIODB} -d testdb -I -f testfeature10
    myinsert.features="testfeature10";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   

//${AUDIODB} -d testdb -S | grep "num files:3"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 3) { returnval = -1; }

//rm -f testdb
    clean_remove_db(databasename);

    /* create new db */
//${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);


//${AUDIODB} -d testdb -I -f testfeature01
    myinsert.features="testfeature01";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   

//${AUDIODB} -d testdb -S | grep "num files:1"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 1) { returnval = -1; }

//${AUDIODB} -d testdb -I -f testfeature01
    myinsert.features="testfeature01";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   

//${AUDIODB} -d testdb -S | grep "num files:1"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 1) { returnval = -1; }

//${AUDIODB} -d testdb -I -f testfeature10
    myinsert.features="testfeature10";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   

//${AUDIODB} -d testdb -S | grep "num files:2"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 2) { returnval = -1; }

//${AUDIODB} -d testdb -I -f testfeature
    myinsert.features="testfeature";
    if(audiodb_insert(mydbp,&myinsert)) {returnval = -1; };   

//${AUDIODB} -d testdb -S | grep "num files:3"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 3) { returnval = -1; }




//rm -f testdb
    clean_remove_db(databasename);

    /* create new db */
//${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);

//echo testfeature > testfeaturelist.txt
//echo testfeature01 >> testfeaturelist.txt
//echo testfeature10 >> testfeaturelist.txt
//${AUDIODB} -B -F testfeaturelist.txt -d testdb
    ins1[0].features="testfeature";
    ins1[1].features="testfeature01";
    ins1[2].features="testfeature10";
    if(audiodb_batchinsert(mydbp,ins1,3)){
        returnval=-1;
    };

//${AUDIODB} -d testdb -S | grep "num files:3"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 3) { returnval = -1; }




//rm -f testdb
    clean_remove_db(databasename);

    /* create new db */
//${AUDIODB} -d testdb -N
    mydbp=audiodb_create(databasename,0,0,0);


//echo testfeature01 > testfeaturelist.txt
//echo testfeature10 >> testfeaturelist.txt
//echo testfeature >> testfeaturelist.txt
//${AUDIODB} -B -F testfeaturelist.txt -d testdb
    ins1[0].features="testfeature";
    ins1[1].features="testfeature01";
    ins1[2].features="testfeature10";
    if(audiodb_batchinsert(mydbp,ins1,3)){
        returnval=-1;
    };

//${AUDIODB} -d testdb -S | grep "num files:3"
    if(audiodb_status(mydbp,&mystatus)) {returnval = -1; };
    if(mystatus.numFiles != 3) { returnval = -1; }



    fprintf(stderr,"returnval:%d\n",returnval); 
    return(returnval);
}
