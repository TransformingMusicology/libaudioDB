template <class T> class DBAccumulator : public Accumulator {
public:
  DBAccumulator(unsigned int pointNN);
  ~DBAccumulator();
  void add_point(adb_result_t *r);
  adb_query_results_t *get_points();
private:
  unsigned int pointNN;
  std::priority_queue< adb_result_t, std::vector<adb_result_t>, T > *queue;
  std::set< adb_result_t, adb_result_triple_lt > *set;
};

template <class T> DBAccumulator<T>::DBAccumulator(unsigned int pointNN)
  : pointNN(pointNN), queue(0), set(0) {
  queue = new std::priority_queue< adb_result_t, std::vector<adb_result_t>, T>;
  set = new std::set<adb_result_t, adb_result_triple_lt>;
}

template <class T> DBAccumulator<T>::~DBAccumulator() {
  if(queue) {
    delete queue;
  }
  if(set) {
    delete set;
  }
}

template <class T> void DBAccumulator<T>::add_point(adb_result_t *r) {
  if(!isnan(r->dist)) {
    if(set->find(*r) == set->end()) {
      set->insert(*r);
      queue->push(*r);
      if(queue->size() > pointNN) {
        queue->pop();
      }
    }
  }
}

template <class T> adb_query_results_t *DBAccumulator<T>::get_points() {
  unsigned int size = queue->size();
  adb_query_results_t *r = (adb_query_results_t *) malloc(sizeof(adb_query_results_t));
  adb_result_t *rs = (adb_result_t *) calloc(size, sizeof(adb_result_t));
  r->nresults = size;
  r->results = rs;

  for(unsigned int k = 0; k < size; k++) {
    rs[k] = queue->top();
    queue->pop();
  }
  return r;
}
