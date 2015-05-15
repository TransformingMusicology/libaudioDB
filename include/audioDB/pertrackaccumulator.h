template <class T> class PerTrackAccumulator : public Accumulator {
public:
  PerTrackAccumulator(unsigned int pointNN, unsigned int trackNN);
  ~PerTrackAccumulator();
  void add_point(adb_result_t *r);
  adb_query_results_t *get_points();
private:
  unsigned int pointNN;
  unsigned int trackNN;
  std::map< const char *, std::priority_queue< adb_result_t, std::vector<adb_result_t>, T > *, adb_strlt> *queues;
};

template <class T> PerTrackAccumulator<T>::PerTrackAccumulator(unsigned int pointNN, unsigned int trackNN)
  : pointNN(pointNN), trackNN(trackNN), queues(0) {
  queues = new std::map< const char *, std::priority_queue< adb_result_t, std::vector<adb_result_t>, T > *, adb_strlt>;
}

template <class T> PerTrackAccumulator<T>::~PerTrackAccumulator() {
  if(queues) {
    typename std::map< const char *, std::priority_queue< adb_result_t, std::vector< adb_result_t >, T > *, adb_strlt>::iterator it;
    for(it = queues->begin(); it != queues->end(); it++) {
      delete (*it).second;
    }
    delete queues;
  }
}

template <class T> void PerTrackAccumulator<T>::add_point(adb_result_t *r) {
  if(!isnan(r->dist)) {
    typename std::map< const char *, std::priority_queue< adb_result_t, std::vector< adb_result_t >, T > *, adb_strlt>::iterator it;
    std::priority_queue< adb_result_t, std::vector< adb_result_t >, T > *queue;
    it = queues->find(r->ikey);
    if(it == queues->end()) {
      queue = new std::priority_queue< adb_result_t, std::vector< adb_result_t >, T >;
      (*queues)[r->ikey] = queue;
    } else {
      queue = (*it).second;
    }
    
    queue->push(*r);
    if(queue->size() > pointNN) {
      queue->pop();
    }
  }
}

template <class T> adb_query_results_t *PerTrackAccumulator<T>::get_points() {
  typename std::map< const char *, std::vector< adb_result_t >, adb_strlt> points;
  typename std::priority_queue< adb_result_t, std::vector< adb_result_t >, T> queue;
  typename std::map< const char *, std::priority_queue< adb_result_t, std::vector< adb_result_t >, T > *, adb_strlt>::iterator it;

  unsigned int size = 0;
  for(it = queues->begin(); it != queues->end(); it++) {
    unsigned int n = ((*it).second)->size();
    std::vector<adb_result_t> v;
    adb_result_t r;
    double dist = 0;
    for(unsigned int k = 0; k < n; k++) {
      r = ((*it).second)->top();
      dist += r.dist;
      v.push_back(r);
      ((*it).second)->pop();
    }
    points[r.ikey] = v;
    dist /= n;
    size += n;
    r.dist = dist;
    /* I will burn in hell */
    r.ipos = n;
    queue.push(r);
    if(queue.size() > trackNN) {
      size -= queue.top().ipos;
      queue.pop();
    }
  }

  adb_query_results_t *r = (adb_query_results_t *) malloc(sizeof(adb_query_results_t));
  adb_result_t *rs = (adb_result_t *) calloc(size, sizeof(adb_result_t));
  r->nresults = size;
  r->results = rs;
  
  unsigned int k = 0;
  while(queue.size() > 0) {
    std::vector<adb_result_t> v = points[queue.top().ikey];
    queue.pop();
    while(v.size() > 0) {
      rs[k++] = v.back();
      v.pop_back();
    }
  }
  return r;
}

