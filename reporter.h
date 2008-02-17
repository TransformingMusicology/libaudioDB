#include <utility>
#include <queue>
#include <set>
#include <functional>

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

class Reporter {
public:
  virtual ~Reporter() {};
  virtual void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist) = 0;
  // FIXME: this interface is a bit wacky: a relic of previous, more
  // confused times.  Really it might make sense to have separate
  // reporter classes for WS and for stdout, rather than passing this
  // adbQueryResponse thing everywhere; the fileTable argument is
  // there solely for convertion trackIDs into names.  -- CSR,
  // 2007-12-10.
  virtual void report(char *fileTable, adb__queryResponse *adbQueryResponse) = 0;
};

template <class T> class pointQueryReporter : public Reporter {
public:
  pointQueryReporter(unsigned int pointNN);
  ~pointQueryReporter();
  void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist);
  void report(char *fileTable, adb__queryResponse *adbQueryResponse);
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

template <class T> void pointQueryReporter<T>::report(char *fileTable, adb__queryResponse *adbQueryResponse) {
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
      std::cout << fileTable + r.trackID*O2_FILETABLESIZE << " ";
      std::cout << r.dist << " " << r.qpos << " " << r.spos << std::endl;
    }
  } else {
    adbQueryResponse->result.__sizeRlist=size;
    adbQueryResponse->result.__sizeDist=size;
    adbQueryResponse->result.__sizeQpos=size;
    adbQueryResponse->result.__sizeSpos=size;
    adbQueryResponse->result.Rlist= new char*[size];
    adbQueryResponse->result.Dist = new double[size];
    adbQueryResponse->result.Qpos = new unsigned int[size];
    adbQueryResponse->result.Spos = new unsigned int[size];
    unsigned int k = 0;
    for(rit = v.rbegin(); rit < v.rend(); rit++, k++) {
      r = *rit;
      adbQueryResponse->result.Rlist[k] = new char[O2_MAXFILESTR];
      adbQueryResponse->result.Dist[k] = r.dist;
      adbQueryResponse->result.Qpos[k] = r.qpos;
      adbQueryResponse->result.Spos[k] = r.spos;
      snprintf(adbQueryResponse->result.Rlist[k], O2_MAXFILESTR, "%s", fileTable+r.trackID*O2_FILETABLESIZE);
    }
  }
}

template <class T> class trackAveragingReporter : public Reporter {
 public:
  trackAveragingReporter(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles);
  ~trackAveragingReporter();
  void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist);
  void report(char *fileTable, adb__queryResponse *adbQueryResponse);
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

template <class T> void trackAveragingReporter<T>::report(char *fileTable, adb__queryResponse *adbQueryResponse) {
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
      std::cout << fileTable + r.trackID*O2_FILETABLESIZE << " ";
      std::cout << r.dist << " " << r.qpos << " " << r.spos << std::endl;
    }
  } else {
    adbQueryResponse->result.__sizeRlist=size;
    adbQueryResponse->result.__sizeDist=size;
    adbQueryResponse->result.__sizeQpos=size;
    adbQueryResponse->result.__sizeSpos=size;
    adbQueryResponse->result.Rlist= new char*[size];
    adbQueryResponse->result.Dist = new double[size];
    adbQueryResponse->result.Qpos = new unsigned int[size];
    adbQueryResponse->result.Spos = new unsigned int[size];
    unsigned int k = 0;
    for(rit = v.rbegin(); rit < v.rend(); rit++, k++) {
      r = *rit;
      adbQueryResponse->result.Rlist[k] = new char[O2_MAXFILESTR];
      adbQueryResponse->result.Dist[k] = r.dist;
      adbQueryResponse->result.Qpos[k] = r.qpos;
      adbQueryResponse->result.Spos[k] = r.spos;
      snprintf(adbQueryResponse->result.Rlist[k], O2_MAXFILESTR, "%s", fileTable+r.trackID*O2_FILETABLESIZE);
    }
  }
}

class trackSequenceQueryRadReporter : public Reporter { 
public:
  trackSequenceQueryRadReporter(unsigned int trackNN, unsigned int numFiles);
  ~trackSequenceQueryRadReporter();
  void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist);
  void report(char *fileTable, adb__queryResponse *adbQueryResponse);
 protected:
  unsigned int trackNN;
  unsigned int numFiles;
  std::set<std::pair<unsigned int, unsigned int> > *set;
  unsigned int *count;
};

trackSequenceQueryRadReporter::trackSequenceQueryRadReporter(unsigned int trackNN, unsigned int numFiles):
  trackNN(trackNN), numFiles(numFiles) {
  set = new std::set<std::pair<unsigned int, unsigned int> >;
  count = new unsigned int[numFiles];
  for (unsigned i = 0; i < numFiles; i++) {
    count[i] = 0;
  }
}

trackSequenceQueryRadReporter::~trackSequenceQueryRadReporter() {
  delete set;
  delete [] count;
}

void trackSequenceQueryRadReporter::add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist) {
  std::set<std::pair<unsigned int, unsigned int> >::iterator it;
  std::pair<unsigned int, unsigned int> pair = std::make_pair(trackID, qpos);
  it = set->find(pair);
  if (it == set->end()) {
    set->insert(pair);
    count[trackID]++;
  }
}

void trackSequenceQueryRadReporter::report(char *fileTable, adb__queryResponse *adbQueryResponse) {
  std::priority_queue < Radresult > result;
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
      std::cout << fileTable + r.trackID*O2_FILETABLESIZE << " " << r.count << std::endl;
    }
  } else {
    // FIXME
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
  void report(char *fileTable, adb__queryResponse *adbQueryResponse);
};

template <class T> trackSequenceQueryNNReporter<T>::trackSequenceQueryNNReporter(unsigned int pointNN, unsigned int trackNN, unsigned int numFiles)
:trackAveragingReporter<T>(pointNN, trackNN, numFiles){}

template <class T> void trackSequenceQueryNNReporter<T>::report(char *fileTable, adb__queryResponse *adbQueryResponse) {
  std::priority_queue < NNresult, std::vector< NNresult>, T> result;
  std::priority_queue< NNresult, std::vector< NNresult>, std::greater<NNresult> > *point_queues = new std::priority_queue< NNresult, std::vector< NNresult>, std::greater<NNresult> >[numFiles];
  
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
      
  if(adbQueryResponse==0) {
    for(rit = v.rbegin(); rit < v.rend(); rit++) {
      r = *rit;
      std::cout << fileTable + r.trackID*O2_FILETABLESIZE << std::endl;
      for(int k=0; k < (int)pointNN; k++){
	NNresult rk = point_queues[r.trackID].top();
	std::cout << rk.dist << " " << rk.qpos << " " << rk.spos << std::endl;
	point_queues[r.trackID].pop();
      }
    }
  } else {
    adbQueryResponse->result.__sizeRlist=size;
    adbQueryResponse->result.__sizeDist=size;
    adbQueryResponse->result.__sizeQpos=size;
    adbQueryResponse->result.__sizeSpos=size;
    adbQueryResponse->result.Rlist= new char*[size];
    adbQueryResponse->result.Dist = new double[size];
    adbQueryResponse->result.Qpos = new unsigned int[size];
    adbQueryResponse->result.Spos = new unsigned int[size];
    unsigned int k = 0;
    for(rit = v.rbegin(); rit < v.rend(); rit++, k++) {
      r = *rit;
      adbQueryResponse->result.Rlist[k] = new char[O2_MAXFILESTR];
      adbQueryResponse->result.Dist[k] = r.dist;
      adbQueryResponse->result.Qpos[k] = r.qpos;
      adbQueryResponse->result.Spos[k] = r.spos;
      snprintf(adbQueryResponse->result.Rlist[k], O2_MAXFILESTR, "%s", fileTable+r.trackID*O2_FILETABLESIZE);
    }
  }
  // clean up
  delete[] point_queues;
}
