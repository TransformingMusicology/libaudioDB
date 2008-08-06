#ifndef __REPORTER_H
#define __REPORTER_H

#include <utility>
#include <queue>
#include <set>
#include <functional>
#include <iostream>
#include "ReporterBase.h"
#include "audioDB.h"

typedef struct nnresult {
  unsigned int trackID;
  double dist;
  unsigned int qpos;
  unsigned int spos;
} NNresult;

typedef struct radresult {
  unsigned int trackID;
  unsigned int count;
} Radresult;

bool operator< (const NNresult &a, const NNresult &b) {
  return a.dist < b.dist;
}

bool operator> (const NNresult &a, const NNresult &b) {
  return a.dist > b.dist;
}

bool operator< (const Radresult &a, const Radresult &b) {
  return a.count < b.count;
}

bool operator> (const Radresult &a, const Radresult &b) {
  return a.count > b.count;
}

class Reporter : public ReporterBase {
public:
  virtual ~Reporter() {};
  virtual void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist) = 0;
  // FIXME: this interface is a bit wacky: a relic of previous, more
  // confused times.  Really it might make sense to have separate
  // reporter classes for WS and for stdout, rather than passing this
  // adbQueryResponse thing everywhere; the fileTable argument is
  // there solely for convertion trackIDs into names.  -- CSR,
  // 2007-12-10.
  virtual void report(char *fileTable, void* adbQueryResponse) = 0;
};

template <class T> class pointQueryReporter : public Reporter {
public:
  pointQueryReporter(unsigned int pointNN);
  ~pointQueryReporter();
  void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist);
  void report(char *fileTable, void* adbQueryResponse);
private:
  unsigned int pointNN;
  std::priority_queue< NNresult, std::vector< NNresult >, T> *queue;
};

template <class T> pointQueryReporter<T>::pointQueryReporter(unsigned int pointNN)
  : pointNN(pointNN) {
  queue = new std::priority_queue< NNresult, std::vector< NNresult >, T>;
}

template <class T> pointQueryReporter<T>::~pointQueryReporter() {
  delete queue;
}

template <class T> void pointQueryReporter<T>::add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist) {
  if (!isnan(dist)) {
    NNresult r;
    r.trackID = trackID;
    r.qpos = qpos;
    r.spos = spos;
    r.dist = dist;
    queue->push(r);
    if(queue->size() > pointNN) {
      queue->pop();
    }
  }
}

template <class T> void pointQueryReporter<T>::report(char *fileTable, void *adbQueryResponse) {
  NNresult r;
  std::vector<NNresult> v;
  unsigned int size = queue->size();
  for(unsigned int k = 0; k < size; k++) {
    r = queue->top();
    v.push_back(r);
    queue->pop();
  }
  std::vector<NNresult>::reverse_iterator rit;
      
  if(adbQueryResponse==0) {
    for(rit = v.rbegin(); rit < v.rend(); rit++) {
      r = *rit;
      if(fileTable)
	std::cout << fileTable + r.trackID*O2_FILETABLE_ENTRY_SIZE << " ";
      else
	std::cout << r.trackID << " ";
      std::cout << r.dist << " " << r.qpos << " " << r.spos << std::endl;
    }
  } else {
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeRlist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeDist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeQpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeSpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.Rlist= new char*[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Dist = new double[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Qpos = new unsigned int[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Spos = new unsigned int[size];
    unsigned int k = 0;
    for(rit = v.rbegin(); rit < v.rend(); rit++, k++) {
      r = *rit;
      ((adb__queryResponse*)adbQueryResponse)->result.Rlist[k] = new char[O2_MAXFILESTR];
      ((adb__queryResponse*)adbQueryResponse)->result.Dist[k] = r.dist;
      ((adb__queryResponse*)adbQueryResponse)->result.Qpos[k] = r.qpos;
      ((adb__queryResponse*)adbQueryResponse)->result.Spos[k] = r.spos;
      if(fileTable)
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%s", fileTable+r.trackID*O2_FILETABLE_ENTRY_SIZE);
      else
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%d", r.trackID);
    }
  }
}

template <class T> class trackAveragingReporter : public Reporter {
 public:
  trackAveragingReporter(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles);
  ~trackAveragingReporter();
  void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist);
  void report(char *fileTable, void *adbQueryResponse);
 protected:
  unsigned int pointNN;
  unsigned int trackNN;
  unsigned int numFiles;
  std::priority_queue< NNresult, std::vector< NNresult>, T > *queues;  
};

template <class T> trackAveragingReporter<T>::trackAveragingReporter(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles) 
  : pointNN(pointNN), trackNN(trackNN), numFiles(numFiles) {
  queues = new std::priority_queue< NNresult, std::vector< NNresult>, T >[numFiles];
}

template <class T> trackAveragingReporter<T>::~trackAveragingReporter() {
  delete [] queues;
}

template <class T> void trackAveragingReporter<T>::add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist) {
  if (!isnan(dist)) {
    NNresult r;
    r.trackID = trackID;
    r.qpos = qpos;
    r.spos = spos;
    r.dist = dist;
    queues[trackID].push(r);
    if(queues[trackID].size() > pointNN) {
      queues[trackID].pop();
    }
  }
}

template <class T> void trackAveragingReporter<T>::report(char *fileTable, void *adbQueryResponse) {
  std::priority_queue < NNresult, std::vector< NNresult>, T> result;
  for (int i = numFiles-1; i >= 0; i--) {
    unsigned int size = queues[i].size();
    if (size > 0) {
      NNresult r;
      double dist = 0;
      NNresult oldr = queues[i].top();
      for (unsigned int j = 0; j < size; j++) {
        r = queues[i].top();
        dist += r.dist;
        queues[i].pop();
        if (r.dist == oldr.dist) {
          r.qpos = oldr.qpos;
          r.spos = oldr.spos;
        } else {
          oldr = r;
        }
      }
      dist /= size;
      r.dist = dist; // trackID, qpos and spos are magically right already.
      result.push(r);
      if (result.size() > trackNN) {
        result.pop();
      }
    }
  }

  NNresult r;
  std::vector<NNresult> v;
  unsigned int size = result.size();
  for(unsigned int k = 0; k < size; k++) {
    r = result.top();
    v.push_back(r);
    result.pop();
  }
  std::vector<NNresult>::reverse_iterator rit;
      
  if(adbQueryResponse==0) {
    for(rit = v.rbegin(); rit < v.rend(); rit++) {
      r = *rit;
      if(fileTable)
	std::cout << fileTable + r.trackID*O2_FILETABLE_ENTRY_SIZE << " ";
      else
	std::cout << r.trackID << " ";
      std::cout << r.dist << " " << r.qpos << " " << r.spos << std::endl;
    }
  } else {
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeRlist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeDist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeQpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeSpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.Rlist= new char*[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Dist = new double[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Qpos = new unsigned int[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Spos = new unsigned int[size];
    unsigned int k = 0;
    for(rit = v.rbegin(); rit < v.rend(); rit++, k++) {
      r = *rit;
      ((adb__queryResponse*)adbQueryResponse)->result.Rlist[k] = new char[O2_MAXFILESTR];
      ((adb__queryResponse*)adbQueryResponse)->result.Dist[k] = r.dist;
      ((adb__queryResponse*)adbQueryResponse)->result.Qpos[k] = r.qpos;
      ((adb__queryResponse*)adbQueryResponse)->result.Spos[k] = r.spos;
      if(fileTable)
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%s", fileTable+r.trackID*O2_FILETABLE_ENTRY_SIZE);
      else
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%d", r.trackID);
    }
  }
}

// Another type of trackAveragingReporter that reports all pointNN nearest neighbours
template <class T> class trackSequenceQueryNNReporter : public trackAveragingReporter<T> {
 protected:
  using trackAveragingReporter<T>::numFiles;
  using trackAveragingReporter<T>::queues;
  using trackAveragingReporter<T>::trackNN;
  using trackAveragingReporter<T>::pointNN;
 public:
  trackSequenceQueryNNReporter(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles);
  void report(char *fileTable, void *adbQueryResponse);
};

template <class T> trackSequenceQueryNNReporter<T>::trackSequenceQueryNNReporter(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles)
:trackAveragingReporter<T>(pointNN, trackNN, numFiles){}

template <class T> void trackSequenceQueryNNReporter<T>::report(char *fileTable, void *adbQueryResponse) {
  std::priority_queue < NNresult, std::vector< NNresult>, T> result;
  std::priority_queue< NNresult, std::vector< NNresult>, std::less<NNresult> > *point_queues 
    = new std::priority_queue< NNresult, std::vector< NNresult>, std::less<NNresult> >[numFiles];
  
  for (int i = numFiles-1; i >= 0; i--) {
    unsigned int size = queues[i].size();
    if (size > 0) {
      NNresult r;
      double dist = 0;
      NNresult oldr = queues[i].top();
      for (unsigned int j = 0; j < size; j++) {
        r = queues[i].top();
        dist += r.dist;
	point_queues[i].push(r);
	queues[i].pop();
        if (r.dist == oldr.dist) {
          r.qpos = oldr.qpos;
          r.spos = oldr.spos;
        } else {
          oldr = r;
        }
      }
      dist /= size;
      r.dist = dist; // trackID, qpos and spos are magically right already.
      result.push(r);
      if (result.size() > trackNN) {
        result.pop();
      }
    }
  }

  NNresult r;
  std::vector<NNresult> v;
  unsigned int size = result.size();
  for(unsigned int k = 0; k < size; k++) {
    r = result.top();
    v.push_back(r);
    result.pop();
  }
  std::vector<NNresult>::reverse_iterator rit;
  std::priority_queue< NNresult, std::vector< NNresult>, std::greater<NNresult> > point_queue;      

  if(adbQueryResponse==0) {
    for(rit = v.rbegin(); rit < v.rend(); rit++) {
      r = *rit;
      if(fileTable)
	std::cout << fileTable + r.trackID*O2_FILETABLE_ENTRY_SIZE << " ";
      else
	std::cout << r.trackID << " ";
	std::cout << r.dist << std::endl;
      unsigned int qsize = point_queues[r.trackID].size();
      // Reverse the order of the points stored in point_queues
      for(unsigned int k=0; k < qsize; k++){
	point_queue.push( point_queues[r.trackID].top() );
	point_queues[r.trackID].pop();
      }
      
      for(unsigned int k = 0; k < qsize; k++) {
	NNresult rk = point_queue.top();
	std::cout << rk.dist << " " << rk.qpos << " " << rk.spos << std::endl;
	point_queue.pop();
      }
    }
  } else {
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeRlist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeDist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeQpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeSpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.Rlist= new char*[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Dist = new double[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Qpos = new unsigned int[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Spos = new unsigned int[size];
    unsigned int k = 0;
    for(rit = v.rbegin(); rit < v.rend(); rit++, k++) {
      r = *rit;
      ((adb__queryResponse*)adbQueryResponse)->result.Rlist[k] = new char[O2_MAXFILESTR];
      ((adb__queryResponse*)adbQueryResponse)->result.Dist[k] = r.dist;
      ((adb__queryResponse*)adbQueryResponse)->result.Qpos[k] = r.qpos;
      ((adb__queryResponse*)adbQueryResponse)->result.Spos[k] = r.spos;
      if(fileTable)
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%s", fileTable+r.trackID*O2_FILETABLE_ENTRY_SIZE);
      else
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%d", r.trackID);
    }
  }
  // clean up
  delete[] point_queues;
}

/********************** Radius Reporters **************************/

class triple{
 public:
  unsigned int a;
  unsigned int b;
  unsigned int c;

  triple(void);
  triple(unsigned int, unsigned int, unsigned int);
  unsigned int first();
  unsigned int second();
  unsigned int third();
};

triple& make_triple(unsigned int, unsigned int, unsigned int);

triple::triple(unsigned int a, unsigned int b, unsigned int c):a(a),b(b),c(c){}

unsigned int triple::first(){return a;}
unsigned int triple::second(){return b;}
unsigned int triple::third(){return c;}

triple::triple():a(0),b(0),c(0){}

bool operator< (const triple &t1, const triple &t2) {
  return ((t1.a < t2.a) ||
	  ((t1.a == t2.a) && ((t1.b < t2.b) ||
			      ((t1.b == t2.b) && (t1.c < t2.c)))));
}
bool operator== (const triple &t1, const triple &t2) {
  return ((t1.a == t2.a) && (t1.b == t2.b) && (t1.c == t2.c));
}

triple& make_triple(unsigned int a, unsigned int b, unsigned int c){
  triple* t = new triple(a,b,c);
  return *t;
}

// track Sequence Query Radius Reporter
// only return tracks and retrieved point counts
class trackSequenceQueryRadReporter : public Reporter { 
public:
  trackSequenceQueryRadReporter(unsigned int trackNN, unsigned int numFiles);
  ~trackSequenceQueryRadReporter();
  void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist);
  void report(char *fileTable, void *adbQueryResponse);
 protected:
  unsigned int trackNN;
  unsigned int numFiles;
  std::set<std::pair<unsigned int, unsigned int> > *set;
  std::set< triple > *set_triple;
  unsigned int *count;
};

trackSequenceQueryRadReporter::trackSequenceQueryRadReporter(unsigned int trackNN, unsigned int numFiles):
  trackNN(trackNN), numFiles(numFiles) {
  set = new std::set<std::pair<unsigned int, unsigned int> >;
  set_triple = new std::set<triple, std::less<triple> >;
  count = new unsigned int[numFiles];
  for (unsigned i = 0; i < numFiles; i++) {
    count[i] = 0;
  }
}

trackSequenceQueryRadReporter::~trackSequenceQueryRadReporter() {
  delete set;
  delete set_triple;
  delete [] count;
}

void trackSequenceQueryRadReporter::add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist) {
  std::set<std::pair<unsigned int, unsigned int> >::iterator it;
  std::pair<unsigned int, unsigned int> pair = std::make_pair(trackID, qpos); // only count this once
  std::set<triple>::iterator it2;
  triple triple;

  triple = make_triple(trackID, qpos, spos); // only count this once
  
  // Record unique <trackID,qpos,spos> triples (record one collision from all hash tables)
  it2 = set_triple->find(triple);

  if(it2 == set_triple->end()){
    set_triple->insert(triple);    

    it = set->find(pair);
    if (it == set->end()) {
      set->insert(pair);
      count[trackID]++; // only count if <tackID,qpos> pair is unique
    }
  }
}

void trackSequenceQueryRadReporter::report(char *fileTable, void *adbQueryResponse) {
  std::priority_queue < Radresult, std::vector<Radresult>, std::greater<Radresult> > result;
  // KLUDGE: doing this backwards in an attempt to get the same
  // tiebreak behaviour as before.
  for (int i = numFiles-1; i >= 0; i--) {
    Radresult r;
    r.trackID = i;
    r.count = count[i];
    if(r.count > 0) {
      result.push(r);
      if (result.size() > trackNN) {
        result.pop();
      }
    }
  }

  Radresult r;
  std::vector<Radresult> v;
  unsigned int size = result.size();
  for(unsigned int k = 0; k < size; k++) {
    r = result.top();
    v.push_back(r);
    result.pop();
  }
  std::vector<Radresult>::reverse_iterator rit;
      
  if(adbQueryResponse==0) {
    for(rit = v.rbegin(); rit < v.rend(); rit++) {
      r = *rit;
      if(fileTable)
	std::cout << fileTable + r.trackID*O2_FILETABLE_ENTRY_SIZE << " ";
      else
	std::cout << r.trackID << " ";
      std::cout << r.count << std::endl;
    }
  } 
 else {
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeRlist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeDist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeQpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeSpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.Rlist= new char*[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Dist = new double[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Qpos = new unsigned int[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Spos = new unsigned int[size];
    unsigned int k = 0;
    for(rit = v.rbegin(); rit < v.rend(); rit++, k++) {
      r = *rit;
      ((adb__queryResponse*)adbQueryResponse)->result.Rlist[k] = new char[O2_MAXFILESTR];
      ((adb__queryResponse*)adbQueryResponse)->result.Dist[k] = 0;
      ((adb__queryResponse*)adbQueryResponse)->result.Qpos[k] = 0;
      ((adb__queryResponse*)adbQueryResponse)->result.Spos[k] = 0;
      if(fileTable)
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%s", fileTable+r.trackID*O2_FILETABLE_ENTRY_SIZE);
      else
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%d", r.trackID);
    }
  }
}

// track Sequence Query Radius NN Reporter
// retrieve tracks ordered by query-point matches (one per track per query point)
//
// as well as sorted n-NN points per retrieved track
class trackSequenceQueryRadNNReporter : public Reporter { 
public:
  trackSequenceQueryRadNNReporter(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles);
  ~trackSequenceQueryRadNNReporter();
  void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist);
  void report(char *fileTable, void *adbQueryResponse);
 protected:
  unsigned int pointNN;
  unsigned int trackNN;
  unsigned int numFiles;
  std::set<std::pair<unsigned int, unsigned int> > *set;
  std::set< triple > *set_triple;
  std::priority_queue< NNresult, std::vector< NNresult>, std::less<NNresult> > *point_queues;
  unsigned int *count;
};

trackSequenceQueryRadNNReporter::trackSequenceQueryRadNNReporter(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles):
pointNN(pointNN), trackNN(trackNN), numFiles(numFiles) {
  // Where to count Radius track matches (one-to-one)
  set = new std::set<std::pair<unsigned int, unsigned int> >;
  set_triple = new std::set<triple, std::less<triple> >;
  // Where to insert individual point matches (one-to-many)
  point_queues = new std::priority_queue< NNresult, std::vector< NNresult>, std::less<NNresult> >[numFiles];
  
  count = new unsigned int[numFiles];
  for (unsigned i = 0; i < numFiles; i++) {
    count[i] = 0;
  }
}

trackSequenceQueryRadNNReporter::~trackSequenceQueryRadNNReporter() {
  delete set;
  delete set_triple;
  delete [] count;
}

void trackSequenceQueryRadNNReporter::add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist) {
  std::set<std::pair<unsigned int, unsigned int> >::iterator it;
  std::set<triple>::iterator it2;
  std::pair<unsigned int, unsigned int> pair;
  triple triple;
  NNresult r;

  pair = std::make_pair(trackID, qpos); // only count this once  
  triple = make_triple(trackID, qpos, spos); // only count this once
  // Record unique <trackID,qpos,spos> triples (record one collision from all hash tables)
  it2 = set_triple->find(triple);  
  if(it2 == set_triple->end()){    
    set_triple->insert(triple);    
    // Record all matching points (within radius)
    // Record counts of <trackID,qpos> pairs
    it = set->find(pair);
    if (it == set->end()) {
      set->insert(pair);
      count[trackID]++;
    }
    if (!isnan(dist)) {
      r.trackID = trackID;
      r.qpos = qpos;
      r.dist = dist;
      r.spos = spos;
      point_queues[trackID].push(r);
      if(point_queues[trackID].size() > pointNN)
	point_queues[trackID].pop();
    }
  }
}

void trackSequenceQueryRadNNReporter::report(char *fileTable, void *adbQueryResponse) {
  std::priority_queue < Radresult, std::vector<Radresult>, std::greater<Radresult> > result;
  // KLUDGE: doing this backwards in an attempt to get the same
  // tiebreak behaviour as before.
  Radresult r;
  NNresult rk;
  std::vector<Radresult> v;
  unsigned int size;

  if(pointNN>1){
    for (int i = numFiles-1; i >= 0; i--) {
      r.trackID = i;
      r.count = count[i];
      if(r.count > 0) {
	cout.flush();
	result.push(r);
	if (result.size() > trackNN) {
	  result.pop();
	}
      }
    }
    
    size = result.size();
    for(unsigned int k = 0; k < size; k++) {
      r = result.top();
      v.push_back(r);
      result.pop();
    }
  }
  else{
    // Instantiate a 1-NN trackAveragingNN reporter
    trackSequenceQueryNNReporter<std::less <NNresult> >* rep = new trackSequenceQueryNNReporter<std::less <NNresult> >(1, trackNN, numFiles);
    // Add all the points we've got to the reporter
    for(unsigned int i=0; i<numFiles; i++){
      int qsize = point_queues[i].size();
      while(qsize--){
	rk = point_queues[i].top();
	rep->add_point(i, rk.qpos, rk.spos, rk.dist);
	point_queues[i].pop();
      }	
    }
    // Report
    rep->report(fileTable, adbQueryResponse);
    // Exit
    delete[] point_queues;
    return;
  }


  // Traverse tracks in descending order of count cardinality
  std::vector<Radresult>::reverse_iterator rit;
  std::priority_queue< NNresult, std::vector< NNresult>, std::greater<NNresult> > point_queue;

  if(adbQueryResponse==0) {
    for(rit = v.rbegin(); rit < v.rend(); rit++) {
      r = *rit;
      if(fileTable)
	std::cout << fileTable + r.trackID*O2_FILETABLE_ENTRY_SIZE << " ";
      else
	std::cout << r.trackID << " ";
      std::cout << r.count << std::endl;

      // Reverse the order of the points stored in point_queues
      unsigned int qsize=point_queues[r.trackID].size();
      for(unsigned int k=0; k < qsize; k++){
	point_queue.push(point_queues[r.trackID].top());
	point_queues[r.trackID].pop();
      }
      for(unsigned int k=0; k < qsize; k++){
	rk = point_queue.top();
	std::cout << rk.dist << " " << rk.qpos << " " << rk.spos << std::endl;
	point_queue.pop();
      }
    }
  }
 else {
   ((adb__queryResponse*)adbQueryResponse)->result.__sizeRlist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeDist=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeQpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.__sizeSpos=size;
    ((adb__queryResponse*)adbQueryResponse)->result.Rlist= new char*[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Dist = new double[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Qpos = new unsigned int[size];
    ((adb__queryResponse*)adbQueryResponse)->result.Spos = new unsigned int[size];
    unsigned int k = 0;
    // Loop over returned tracks
    for(rit = v.rbegin(); rit < v.rend(); rit++, k++) {
      r = *rit;
      // Reverse the order of the points stored in point_queues
      unsigned int qsize=point_queues[r.trackID].size();
      while(qsize--){
	point_queue.push(point_queues[r.trackID].top());
	point_queues[r.trackID].pop();
      }
      qsize=point_queue.size();
      rk = point_queue.top(); // Take one point from the top of the queue
      ((adb__queryResponse*)adbQueryResponse)->result.Rlist[k] = new char[O2_MAXFILESTR];
      ((adb__queryResponse*)adbQueryResponse)->result.Dist[k] = rk.dist;
      ((adb__queryResponse*)adbQueryResponse)->result.Qpos[k] = rk.qpos;
      ((adb__queryResponse*)adbQueryResponse)->result.Spos[k] = rk.spos;
      if(fileTable)
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%s", fileTable+r.trackID*O2_FILETABLE_ENTRY_SIZE);
      else
	snprintf(((adb__queryResponse*)adbQueryResponse)->result.Rlist[k], O2_MAXFILESTR, "%d", r.trackID);
      while(qsize--) // pop the rest of the points
	point_queue.pop();
    }
 }
  delete[] point_queues;
}

/********** ONE-TO-ONE REPORTERS *****************/

// track Sequence Query Radius NN Reporter One-to-One
// for each query point find the single best matching target point in all database
// report qpos, spos and trackID
class trackSequenceQueryRadNNReporterOneToOne : public Reporter { 
public:
  trackSequenceQueryRadNNReporterOneToOne(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles);
  ~trackSequenceQueryRadNNReporterOneToOne();
  void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist);
  void report(char *fileTable, void *adbQueryResponse);
 protected:
  unsigned int pointNN;
  unsigned int trackNN;
  unsigned int numFiles;
  std::set< NNresult > *set;
  std::vector< NNresult> *point_queue;
  unsigned int *count;

};

trackSequenceQueryRadNNReporterOneToOne::trackSequenceQueryRadNNReporterOneToOne(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles):
pointNN(pointNN), trackNN(trackNN), numFiles(numFiles) {
  // Where to count Radius track matches (one-to-one)
  set = new std::set< NNresult >; 
  // Where to insert individual point matches (one-to-many)
  point_queue = new std::vector< NNresult >;
  
  count = new unsigned int[numFiles];
  for (unsigned i = 0; i < numFiles; i++) {
    count[i] = 0;
  }
}

trackSequenceQueryRadNNReporterOneToOne::~trackSequenceQueryRadNNReporterOneToOne() {
  delete set;
  delete [] count;
}

void trackSequenceQueryRadNNReporterOneToOne::add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist) {
  std::set< NNresult >::iterator it;
  NNresult r;

  r.qpos = qpos;
  r.trackID = trackID;
  r.spos = spos;
  r.dist = dist;

  if(point_queue->size() < r.qpos + 1){
    point_queue->resize( r.qpos + 1 );
    (*point_queue)[r.qpos].dist = 1e6;
  }

  if (r.dist < (*point_queue)[r.qpos].dist)
    (*point_queue)[r.qpos] = r;

}

void trackSequenceQueryRadNNReporterOneToOne::report(char *fileTable, void *adbQueryResponse) {
  if(adbQueryResponse==0) {
    std::vector< NNresult >::iterator vit;
    NNresult rk;
    for( vit = point_queue->begin() ; vit < point_queue->end() ; vit++ ){
      rk = *vit;
      std::cout << rk.dist << " " 
		<< rk.qpos << " " 
		<< rk.spos << " ";
      if(fileTable)
	std::cout << fileTable + rk.trackID*O2_FILETABLE_ENTRY_SIZE << " ";
      else
	std::cout << rk.trackID << " "; 
      std::cout << std::endl;
      }
  } else {
    // FIXME
  }
}

#endif
