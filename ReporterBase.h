
#ifndef __REPORTERBASE_H
#define __REPORTERBASE_H

class ReporterBase {
public:
  virtual ~ReporterBase(){};
  virtual void add_point(unsigned int trackID, unsigned int qpos, unsigned int spos, double dist, int rot = 0) = 0;
  virtual void report(adb_t *, struct soap *, void *, bool) = 0;
};

#endif
