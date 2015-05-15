template <class T> class NearestAccumulator : public Accumulator {
public:
  NearestAccumulator();
  ~NearestAccumulator();
  void add_point(adb_result_t *r, double *thresh = NULL);
  double threshold(const char *key);
  adb_query_results_t *get_points();
private:
  std::set< adb_result_t, adb_result_qpos_lt > *points;
};

template <class T> NearestAccumulator<T>::NearestAccumulator()
  : points(0) {
  points = new std::set< adb_result_t, adb_result_qpos_lt >;
}

template <class T> NearestAccumulator<T>::~NearestAccumulator() {
  if(points) {
    delete points;
  }
}

template <class T> double NearestAccumulator<T>::threshold(const char *key) {
  return std::numeric_limits<double>::infinity();
}

template <class T> void NearestAccumulator<T>::add_point(adb_result_t *r, double *thresh) {
  if(!isnan(r->dist)) {
    std::set< adb_result_t, adb_result_qpos_lt >::iterator it;
    it = points->find(*r);
    if(it == points->end()) {
      points->insert(*r);
    } else if(T()(*(const adb_result_t *)r,(*it))) {
      points->erase(it);
      points->insert(*r);
    }
  }
}

template <class T> adb_query_results_t *NearestAccumulator<T>::get_points() {
  unsigned int nresults = points->size();
  adb_query_results_t *r = (adb_query_results_t *) malloc(sizeof(adb_query_results_t));
  adb_result_t *rs = (adb_result_t *) calloc(nresults, sizeof(adb_result_t));
  r->nresults = nresults;
  r->results = rs;
  std::set< adb_result_t, adb_result_qpos_lt >::iterator it;
  unsigned int k = 0;
  for(it = points->begin(); it != points->end(); it++) {
    rs[k++] = *it;
  }
  return r;
}

