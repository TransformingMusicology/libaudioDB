
#ifndef __REPORTERBASE_H
#define __REPORTERBASE_H

class ReporterBase {
public:
  virtual ~ReporterBase(){};
  virtual void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist) = 0;
  virtual void report(adb_t *,void *) = 0;
};

#endif
