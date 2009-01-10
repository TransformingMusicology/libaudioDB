#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

class Accumulator {
public:
  virtual ~Accumulator() {};
  virtual void add_point(adb_result_t *r) = 0;
  virtual adb_query_results_t *get_points() = 0;
};

#endif
