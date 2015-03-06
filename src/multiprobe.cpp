/*
 * MultiProbe C++ class
 *
 * Given a vector of LSH boundary distances for a query, 
 * perform lookup by probing nearby hash-function locations
 *
 * Implementation using C++ STL
 *
 * Reference:
 * Qin Lv, William Josephson, Zhe Wang, Moses Charikar and Kai Li,
 * "Multi-Probe LSH: Efficient Indexing for High-Dimensional Similarity
 * Search", Proc. Intl. Conf. VLDB, 2007
 *
 *
 * Copyright (C) 2009 Michael Casey, Dartmouth College, All Rights Reserved
 * License: GNU Public License 2.0
 *
 */

#include "audioDB/multiprobe.h"

//#define _TEST_MP_LSH

bool operator> (const min_heap_element& a, const min_heap_element& b){
  return a.score > b.score;
}

bool operator< (const min_heap_element& a, const min_heap_element& b){
  return a.score < b.score;
}

bool operator>(const sorted_distance_functions& a, const sorted_distance_functions& b){
  return a.first > b.first;
}

bool operator<(const sorted_distance_functions& a, const sorted_distance_functions& b){
  return a.first < b.first;
}

MinHeapElement::MinHeapElement(perturbation_set a, float s):
  perturbs(a),
  score(s)
{

}

MinHeapElement::~MinHeapElement(){;}

MultiProbe::MultiProbe():
  minHeap(0),
  outSets(0),
  distFuns(0),
  numHashBoundaries(0)
{

}

MultiProbe::~MultiProbe(){  
  cleanup();
}

void MultiProbe::initialize(){
  minHeap = new min_heap_of_perturbation_set();
  outSets = new min_heap_of_perturbation_set();
}

void MultiProbe::cleanup(){
  delete minHeap;
  minHeap = 0;
  delete outSets;
  outSets = 0;
  delete distFuns;
  distFuns = 0;
}

size_t MultiProbe::size(){
  return outSets->size();
}

bool MultiProbe::empty(){
  return !outSets->size();
}


void MultiProbe::generatePerturbationSets(vector<float>& x, unsigned T){
  cleanup(); // Make re-entrant
  initialize();
  makeSortedDistFuns(x);
  algorithm1(T);
}

// overloading to support efficient array use without initial copy
void MultiProbe::generatePerturbationSets(float* x, unsigned N, unsigned T){
  cleanup(); // Make re-entrant
  initialize();
  makeSortedDistFuns(x, N);
  algorithm1(T);
}

// Generate the optimal T perturbation sets for current query
// pre-conditions:
//   an LSH structure was initialized and passed to constructor
//   a query vector was passed to lsh->compute_hash_functions()
//   the query-to-boundary distances are stored in x[hashFunIndex]
//
// post-conditions:
//  generates an ordered list of perturbation sets (stored as an array of sets)
//  these are indexes into pi_j=(i,delta) pairs representing x_i(delta) in sort order z_j
//  data structures are cleared and reset to zeros thereby making them re-entrant
//
void MultiProbe::algorithm1(unsigned T){
  perturbation_set ai,as,ae;
  float ai_score;
  ai.insert(0); // Initialize for this query
  minHeap->push(min_heap_element(ai, score(ai))); // unique instance stored in mhe

  min_heap_element mhe = minHeap->top();

  if(T>distFuns->size())
    T = distFuns->size();
  for(unsigned i = 0 ; i != T ; i++ ){
    do{
      mhe = minHeap->top();
      ai = mhe.perturbs;
      ai_score = mhe.score;
      minHeap->pop();
      as=ai;
      shift(as);
      minHeap->push(min_heap_element(as, score(as)));
      ae=ai;
      expand(ae);
      minHeap->push(min_heap_element(ae, score(ae)));
    }while(!valid(ai));
    outSets->push(mhe); // Ordered list of perturbation sets
  }
}

void MultiProbe::dump(perturbation_set a){
  perturbation_set::iterator it = a.begin();
  while(it != a.end()){
    cout << "[" << (*distFuns)[*it].second.first << "," << (*distFuns)[*it].second.second << "]" << " " 
	 << (*distFuns)[*it].first << *it << ", ";
    it++;    
  }
  cout << "(" << score(a) << ")";
  cout << endl;
}

// Given the set a, add 1 to last element of the set
inline perturbation_set& MultiProbe::shift(perturbation_set& a){  
  perturbation_set::iterator it = a.end();
  int val = *(--it) + 1;
  a.erase(it);
  a.insert(it,val);
  return a;
}

// Given the set a, add a new element one greater than the max
inline perturbation_set& MultiProbe::expand(perturbation_set& a){
  perturbation_set::reverse_iterator ri = a.rbegin();
  a.insert(*ri+1);
  return a;
}

// Take the list of distances (x) assuming list len is 2M and
// delta = (-1)^i, i = { 0 .. 2M-1 }
void MultiProbe::makeSortedDistFuns(vector<float>& x){
  numHashBoundaries = x.size(); // x.size() == 2M
  delete distFuns;
  distFuns = new std::vector<sorted_distance_functions>(numHashBoundaries);
  for(unsigned i = 0; i != numHashBoundaries ; i++ )
    (*distFuns)[i] = make_pair(x[i], make_pair(i, i%2?1:-1));
  // SORT
  sort( distFuns->begin(), distFuns->end() );
}

// Float array version of above
void MultiProbe::makeSortedDistFuns(float* x, unsigned N){
  numHashBoundaries = N; // x.size() == 2M
  delete distFuns;
  distFuns = new std::vector<sorted_distance_functions>(numHashBoundaries);
  for(unsigned i = 0; i != numHashBoundaries ; i++ )
    (*distFuns)[i] = make_pair(x[i], make_pair(i, i%2?1:-1));
  // SORT
  sort( distFuns->begin(), distFuns->end() );
}

// For a given perturbation set, the score is the 
// sum of squares of corresponding distances in x
float MultiProbe::score(perturbation_set& a){
  //assert(!a.empty());
  float score = 0.0, tmp;
  perturbation_set::iterator it;
  it = a.begin();
  do{
    tmp = (*distFuns)[*it].first;
    score += tmp*tmp;
  }while( ++it != a.end() );
  return score;
}

// A valid set must have at most one
// of the two elements {j, 2M + 1 - j} for every j
//
// A perturbation set containing an element > 2M is invalid
bool MultiProbe::valid(perturbation_set& a){
  int j;  
  perturbation_set::iterator it = a.begin();  
  while( it != a.end() ){
    j = *it;
    it++;
    if( ( (unsigned)j > numHashBoundaries ) || ( a.find( numHashBoundaries - j - 1 ) != a.end() ) )
      return false;    
  }
  return true;
}

int MultiProbe::getIndex(perturbation_set::iterator it){
  return (*distFuns)[*it].second.first;
}

int MultiProbe::getBoundary(perturbation_set::iterator it){
  return (*distFuns)[*it].second.second;
}

// copy return next perturbation_set
perturbation_set MultiProbe::getNextPerturbationSet(){
  perturbation_set s = outSets->top().perturbs; 
  outSets->pop();
  return s;
}

// Test routine: generate 100 random boundary distance pairs
// call generatePerturbationSets on these distances
// dump output for inspection
#ifdef _TEST_MP_LSH
int main(const int argc, const char* argv[]){
  int N_SAMPS = 100; // Number of random samples
  int W = 4;         // simulated hash-bucket size
  int N_ITER = 100;  // How many re-entrant iterations
  unsigned T = 10; // Number of multi-probe sets to generate

  MultiProbe mp= MultiProbe();
  vector<float> x(N_SAMPS);

  srand((unsigned)time(0)); 

  // Test re-entrance on single instance
  for(int j = 0; j< N_ITER ; j++){
    cout << "********** ITERATION " << j << " **********" << endl;
    cout.flush();
    for (int i = 0 ; i != x.size()/2 ; i++ ){
      x[2*i] = W*(rand()/(RAND_MAX+1.0));
      x[2*i+1] = W - x[2*i];
    }
    // Generate multi-probe sets
    mp.generatePerturbationSets(x, T);
    // Output contents of multi-probe sets
    while(!mp.empty())
      mp.dump(mp.getNextPerturbationSet());
  }
}
#endif
