template <class T> class NearestAccumulator : public Accumulator {
public:
  NearestAccumulator();
  ~NearestAccumulator();
  void add_point(adb_result_t *r);
  adb_query_results_t *get_points();
private:
  std::set< adb_result_t, adb_result_triple_lt > *set;
  std::set< adb_result_t, adb_result_qpos_lt > *points;
};

template <class T> NearestAccumulator<T>::NearestAccumulator()
  : set(0), points(0) {
  set = new std::set< adb_result_t, adb_result_triple_lt >;
  points = new std::set< adb_result_t, adb_result_qpos_lt >;
}

template <class T> NearestAccumulator<T>::~NearestAccumulator() {
  if(set) {
    delete set;
  }
  if(points) {
    delete points;
  }
}

template <class T> void NearestAccumulator<T>::add_point(adb_result_t *r) {
  if(!isnan(r->dist)) {
    if(set->find(*r) == set->end()) {
      set->insert(*r);

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

