/* for API questions contact 
 * Christophe Rhodes c.rhodes@gold.ac.uk
 * Ian Knopke mas01ik@gold.ac.uk, ian.knopke@gmail.com */


/*******************************************************************/
/* Data types for API */

/* The main struct that stores the name of the database, and in future will hold all
 * kinds of other interesting information */
/* This basically gets passed around to all of the other functions */
struct adb {

    char * dbname;
    unsigned int ntracks;       /* number of tracks */
    unsigned int datadim;       /* dimensionality of stored data */

};
typedef struct adb adb_t, *adb_ptr;

//used for both insert and batchinsert
struct adbinsert {

    char * features;
    char * power;
    char * key;
    char * times;

};
typedef struct adbinsert adb_insert_t, *adb_insert_ptr;

/* struct for returning status results */
struct adbstatus {

    unsigned int numFiles;  
    unsigned int dim;
    unsigned int length;
    unsigned int dudCount;
    unsigned int nullCount;
    unsigned int flags;

};
typedef struct adbstatus adb_status_t, *adb_status_ptr;

/* needed for constructing a query */
struct adbquery {

    char * querytype;
    char * feature; //usually a file of some kind
    char * power; //also a file
    char * keylist; //also a file
    char * qpoint;  //position 
    char * numpoints;
    char * radius; 
    char * resultlength; //how many results to make
    char * sequencelength; 
    char * sequencehop; 
    double absolute_threshold; 
    double relative_threshold;
    int exhaustive; //hidden option in gengetopt
    double expandfactor; //hidden
    int rotate; //hidden

};
typedef struct adbquery adb_query_t,*adb_query_ptr;

/* ... and for getting query results back */
struct adbqueryresult {

    int sizeRlist; /* do I really need to return all 4 sizes here */
    int sizeDist;
    int sizeQpos;
    int sizeSpos;
    char **Rlist;
    double *Dist;
    unsigned int *Qpos;
    unsigned int *Spos;

};
typedef struct adbqueryresult adb_queryresult_t, *adb_queryresult_ptr;


/*******************************************************************/
/* Function prototypes for API */


/* open an existing database */
/* returns a struct or NULL on failure */
adb_ptr audiodb_open(char * path);

/* create a new database */
/* returns a struct or NULL on failure */
//adb_ptr audiodb_create(char * path,long ntracks, long datadim);
adb_ptr audiodb_create(char * path,long datasize, long ntracks, long datadim);

/* close a database */
void audiodb_close(adb_ptr db);

/* You'll need to turn both of these on to do anything useful */
int audiodb_l2norm(adb_ptr mydb);
int audiodb_power(adb_ptr mydb);

/* insert functions */
int audiodb_insert(adb_ptr mydb, adb_insert_ptr ins);
int audiodb_batchinsert(adb_ptr mydb, adb_insert_ptr ins, unsigned int size);

/* query function */
int audiodb_query(adb_ptr mydb, adb_query_ptr adbq, adb_queryresult_ptr adbqres);
  
/* database status */  
int audiodb_status(adb_ptr mydb, adb_status_ptr status);

/* varoius dump formats */
int audiodb_dump(adb_ptr mydb);
int audiodb_dump_withdir(adb_ptr mydb, char * outputdir);


