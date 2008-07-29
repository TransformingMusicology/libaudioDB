// UNIT_TEST_LSH.cpp

#include <vector>
#include "lshlib.h"
#include "reporter.h"

#define LSH_IN_CORE


#define N_POINT_BITS 14
#define POINT_BIT_MASK 0x00003FFF

// Callback method for LSH point retrieval
void add_point(void* reporter, Uns32T pointID, Uns32T qpos, float dist)
{
  ReporterBase* pr = (ReporterBase*)reporter;
  pr->add_point(pointID>>N_POINT_BITS, qpos, pointID&POINT_BIT_MASK, dist);
}

int main(int argc, char* argv[]){

  int nT = 100; // num tracks 
  int nP = 1000;  // num points-per-track
  float w = 4.0;// LSH bucket width
  int k = 10;
  int m = 2;
  int d = 10;
  int N = 100000;
  int C = 200;

  float radius = 0.001;
  char FILENAME[] = "foo.lsh";

  assert(nP>=nT);

  int fid = open(FILENAME,O_RDONLY);
  LSH* lsh;
  bool serialized = false;
  Uns32T trackBase = 0;

  if(fid< 0){ // Make a new serial LSH file
    lsh = new LSH(w,k,m,d,N,C,radius);
    assert(lsh);
    cout << "NEW LSH:" << endl;
    }
  else{
    close(fid); // Load LSH structures from disk
    lsh = new LSH(FILENAME); 
    assert(lsh);
    cout << "MERGE WITH EXISTING LSH:" << FILENAME << endl;
    serialized=true;
    trackBase = (lsh->get_maxp()>>N_POINT_BITS)+1; // Our encoding of tracks and points
  }  
  cout << "k:" << lsh->k << " ";
  cout << "m:" << lsh->m << "(L:" << lsh->L << ") ";
  cout << "d:" << lsh->d << " ";
  cout << "N:" << lsh->N << " ";
  cout << "C:" << lsh->C << " ";
  cout << "R:" << lsh->get_radius() << endl;
  cout << "p:" << lsh->p << endl;
  cout.flush();

  cout << endl << "Constructing " << nT << " tracks with " << nP << " vectors of dimension " << d << endl;
  cout.flush();
  // Construct sets of database vectors, use one point from each set for testing
  vector< vector<float> > vv = vector< vector<float> >(nP); // track vectors
  vector< vector<float> > qq = vector< vector<float> >(nP);// query vectors
  for(int i=0; i< nP ; i++){
    vv[i]=vector<float>(d);  // allocate vector
    qq[i]=vector<float>(d);  // allocate vector
  }
  
  for(int k = 0 ; k < nT ; k ++){
    cout << "[" << k << "]";
    cout.flush();
    for(int i = 0 ; i< nP ; i++)
      for(int j=0; j< d ; j++)
	vv[i][j] =   genrand_real2() / radius; // MT_19937 random numbers
    lsh->insert_point_set(vv, (trackBase+k)<<N_POINT_BITS);
    qq[k] = vv[k]; // One identity query per set of database vectors
  }
  cout << endl;
  cout.flush();

  cout << "Writing serialized LSH tables..." << endl;
  // TEST SERIALIZED LSH RETRIEVAL
  lsh->serialize(FILENAME);

  // TEST LSH RETRIEVAL IN CORE
  printf("\n********** In-core LSH retrieval from %d track%c **********\n", 
	 (lsh->get_maxp()>>N_POINT_BITS)+1,(lsh->get_maxp()>>N_POINT_BITS)>0?'s':' ');
  fflush(stdout);  
  for(int i = 0; i < nT ; i++ ){
    trackSequenceQueryRadNNReporter* pr = new trackSequenceQueryRadNNReporter(nP,nT,(lsh->get_maxp()>>N_POINT_BITS)+1); 
    lsh->retrieve_point(qq[i], i, &add_point, (void*)pr); // LSH point retrieval from core
    printf("query vector %d] t1:%u t2:%0X\n", i, lsh->get_t1(), lsh->get_t2());
    fflush(stdout);
    pr->report(0,0);
    delete pr;
  }
  delete lsh;

  cout << "Loading Serialized LSH functions from disk ..." << endl;
  cout.flush();
  lsh = new LSH(FILENAME);
  assert(lsh);  
  //  lsh->serial_dump_tables(FILENAME);
  printf("\n********** Serialized LSH retrieval from %d track%c **********\n", (lsh->get_maxp()>>N_POINT_BITS)+1,(lsh->get_maxp()>>N_POINT_BITS)>1?'s':' ');
  fflush(stdout);  
  for(int i= 0; i < nT ; i++ ){
    trackSequenceQueryRadNNReporter* pr = new trackSequenceQueryRadNNReporter(nP,nT,(lsh->get_maxp()>>N_POINT_BITS)+1); 
    lsh->serial_retrieve_point(FILENAME, qq[i], i, &add_point, (void*) pr); // LSH serialized point retrieval method  
    printf("query vector %d] t1:%u t2:%0X\n", i, lsh->get_t1(), lsh->get_t2());
    fflush(stdout);
    pr->report(0,0);
    delete pr;
  }  
  delete lsh;

#ifdef LSH_IN_CORE  
  cout << "Loading Serialized LSH functions and tables from disk ..." << endl;
  cout.flush();
  // Unserialize entire lsh tree to core
  lsh = new LSH(FILENAME,1);
  
  // TEST UNSERIALIZED LSH RETRIEVAL IN CORE
  printf("\n********** Unserialized LSH in-core retrieval from %d track%c **********\n", (lsh->get_maxp()>>N_POINT_BITS)+1,(lsh->get_maxp()>>N_POINT_BITS)>1?'s':' ');
  fflush(stdout);
  for(int i = 0; i < nT ; i++ ){
    trackSequenceQueryRadNNReporter* pr = new trackSequenceQueryRadNNReporter(nP,nT,(lsh->get_maxp()>>N_POINT_BITS)+1); 
    lsh->retrieve_point(qq[i], i, &add_point, (void*) pr); // LSH point retrieval from core
    printf("query vector %d] t1:%u t2:%0X\n", i, lsh->get_t1(), lsh->get_t2());
    fflush(stdout);
    pr->report(0,0);
    delete pr;
  }  
  delete lsh;
#endif

}
