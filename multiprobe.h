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

#ifndef __LSH_MULTIPROBE_
#define __LSH_MULTIPROBE_

#include <functional>
#include <queue>
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>

using namespace std;

typedef set<int > perturbation_set ;

typedef class MinHeapElement{
public:
  perturbation_set perturbs;
  float score;
  MinHeapElement(perturbation_set a, float s);
  virtual ~MinHeapElement();
} min_heap_element;

typedef priority_queue<min_heap_element, 
		       vector<min_heap_element>, 
		       greater<min_heap_element> 
		       > min_heap_of_perturbation_set ;

typedef pair<float, pair<int, int> > sorted_distance_functions ;

class MultiProbe{
protected:
  min_heap_of_perturbation_set* minHeap;
  min_heap_of_perturbation_set* outSets;
  vector<sorted_distance_functions>* distFuns;
  unsigned numHashBoundaries;

  // data structure initialization and cleanup
  void initialize();
  void cleanup();
  
  // perturbation set operations
  perturbation_set& shift(perturbation_set&);
  perturbation_set& expand(perturbation_set&);
  float score(perturbation_set&);
  bool valid(perturbation_set&);
  void makeSortedDistFuns(vector<float> &);
  void makeSortedDistFuns(float* x, unsigned N);

  // perturbation set generation algorithm
  void algorithm1(unsigned T);

public: 
  MultiProbe();
  ~MultiProbe();

  // generation of perturbation sets
  void generatePerturbationSets(vector<float>& vectorOfBounaryDistances, unsigned numSetsToGenerate);  
  void generatePerturbationSets(float* arrayOfBoundaryDistances, unsigned numDistances, unsigned numSetsToGenerate);
  perturbation_set getNextPerturbationSet();
  void dump(perturbation_set);
  size_t size(); // Number of perturbation sets are in the output queue
  bool empty(); // predicate for empty MultiProbe set
  int getIndex(perturbation_set::iterator it); // return index of hash function for given set entry
  int getBoundary(perturbation_set::iterator it); // return boundary {-1,+1} for given set entry
};


/* NOTES:

Reference:
Qin Lv, William Josephson, Zhe Wang, Moses Charikar and Kai Li,
"Multi-Probe LSH: Efficient Indexing for High-Dimensional Similarity
Search", Proc. Intl. Conf. VLDB, 2007

  i = 1..M (number of hash functions used)
  f_i(q) = a_i * q + b_i // projection to real line
  h_i(q) = floor( ( a_i * q + b_i ) / w ) // hash slot  
  delta \in {-1, +1}
  x_i( delta ) = distance of query to left or right boundary
  Delta = [delta_1 delta_2 ... delta_M]
  score(Delta) = sum_{i=1}_M x_i(delta_i).^2
  
  z = sort(x(delta_i), increasing) // i = 1..2M delta={+1,-1}
  p_j = (i, delta) if z_j == x_i(delta) 
  
  A_k is index into p_j

  Multi-probe algorithm (after Lv et al. 2007)
  ---------------------------------------------

  A0 = {1}
  minHeap.insert(A0, score(A0))
  
  for i = 1 to T do   // T = number of perturbation sets
     repeat
        Ai = minHeap.extractMin()
	As = shift(Ai)
	minHeap.insert(As, score(As))
	Ae = expand(Ai)
	minHeap.insert(Ae, score(Ae))
     until valid(Ai)
     output Ai
  end for

 */

#endif // __LSH_MULTIPROBE_
