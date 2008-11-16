void delete_dir(char * dirname);
void clean_remove_db(char * dirname);
void test_status(adb_ptr d, adb_status_ptr b);
unsigned int test_insert( adb_ptr d, char * features, char * power, char * key);
void dump_query(adb_query_ptr adbq, adb_queryresult_ptr myadbqueryresult);
int testoneresult(adb_queryresult_ptr myadbqueryresult, int i, char * Rlist, double Dist,double Qpos,double Spos);
double doubleabs(double foo);
void maketestfile(char * filename, int * ivals, double * dvals, int dvalsize);
int testoneradiusresult(adb_queryresult_ptr myadbqueryresult, int i, char * Rlist, int count);
void makekeylistfile(char * filename, char * item);




/* clean remove */
void clean_remove_db(char * dbname){

    FILE* db=0;

    db=fopen(dbname,"r");

    if (!db){
        return;
    }


    fclose(db);
    remove(dbname);

    return;

}


/* delete directory */
void delete_dir(char * dirname){

    struct dirent *d;
    DIR *dir;
    char buf[256];

    printf("Deleting directory '%s' and all files\n", dirname);
    dir = opendir(dirname);

    if (dir){
        while((d = readdir(dir))) {
            //printf("Deleting %s in %s\n",d->d_name, dirname);
            sprintf(buf, "%s/%s", dirname, d->d_name);
            remove(buf);
        }
    }
    closedir(dir);

    rmdir(dirname);


    return;

}


unsigned int test_insert(
    adb_ptr d,
    char * features,
    char * power,
    char * key
){

    adb_insert_t myinsert={0};
    unsigned int myerr=0;

    printf("Insert:\n");
    myinsert.features=features;
    myinsert.power=power; 
    myinsert.key=key; 
    myerr=audiodb_insert(d,&myinsert); 
    printf("\n");

    return myerr;

}

void test_status(adb_ptr d, adb_status_ptr b){

    /* get the status of the database */
    audiodb_status(d,b);

    /* could probably make this look a bit more clever, but it works for now */
    printf("numFiles:\t%d\n",b->numFiles);
    printf("dim:\t%d\n",b->dim);
    printf("length:\t%d\n",b->length);
    printf("dudCount:\t%d\n",b->dudCount);
    printf("nullCount:\t%d\n",b->nullCount);
    printf("flags:\t%d\n",b->flags);

    return;
}


void dump_query(adb_query_ptr adbq, adb_queryresult_ptr myadbqueryresult){

    int size=0;
    int i=0;

    size=myadbqueryresult->sizeRlist;

    printf("Dumping query:\n");
    for(i=0; i<size; i++){
        printf("\t'%s' query: Result %02d:%s is dist:%f qpos:%d spos:%d\n",
                adbq->querytype,
                i,
                myadbqueryresult->Rlist[i],
                myadbqueryresult->Dist[i],
                myadbqueryresult->Qpos[i],
                myadbqueryresult->Spos[i]
              );
    }
    printf("\n");

}



int testoneresult(adb_queryresult_ptr myadbqueryresult, int i, char * Rlist, double Dist,double Qpos,double Spos){

    int ret=0;
    double tolerance=.0001;


    
    if (strcmp(Rlist,myadbqueryresult->Rlist[i])){
        ret=-1;
    } 


    if (doubleabs((double)Dist - (double)myadbqueryresult->Dist[i]) > tolerance){
        ret=-1;
    } 
    
    if (doubleabs((double)Qpos - (double)myadbqueryresult->Qpos[i]) > tolerance){
        ret=-1;
    } 

    if (doubleabs((double)Spos - (double)myadbqueryresult->Spos[i]) > tolerance){
        ret=-1;
    } 

    return ret;
}


int testoneradiusresult(adb_queryresult_ptr myadbqueryresult, int i, char * Rlist, int count){

    int ret=0;

    if (strcmp(Rlist,myadbqueryresult->Rlist[i])){
        ret=-1;
    } 

    /* KLUDGE: at the moment, the structure returned from "sequence"
       queries with a radius has two unused fields, Dist and Qpos, and
       the Spos field is punned to indicate the count of hits from
       that track.  This is really ugly and needs to die. */
    if (count != myadbqueryresult->Spos[i]) {
        ret=-1;
    } 

    return ret;
}


double doubleabs(double foo){

    double retval=foo;

    if (foo < 0.0) {
        retval=foo * -1.0;
    }

    return retval;
}



void maketestfile(char * filename, int * ivals, double * dvals, int dvalsize) {

    FILE * myfile;

    myfile=fopen(filename,"w");
    fwrite(ivals,sizeof(int),1,myfile);
    fwrite(dvals,sizeof(double),dvalsize,myfile);
    fflush(myfile);
    fclose(myfile);

    /* should probably test for success, but then it is a test suite already... */
}



void makekeylistfile(char * filename, char * item){

    FILE * myfile;

    myfile=fopen(filename,"w");
    fprintf(myfile,"%s\n",item);
   fflush(myfile);
    fclose(myfile);

}





