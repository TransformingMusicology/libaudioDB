#include "audioDB.h"
extern "C" {
#include "audioDB_API.h"
#include "audioDB-internals.h"
}

PointPair::PointPair(Uns32T a, Uns32T b, Uns32T c) :
  trackID(a), qpos(b), spos(c) {
};

bool operator<(const PointPair& a, const PointPair& b) {
  return ((a.trackID < b.trackID) ||
          ((a.trackID == b.trackID) &&
           ((a.spos < b.spos) || ((a.spos == b.spos) && (a.qpos < b.qpos)))));
}

bool operator>(const PointPair& a, const PointPair& b) {
  return ((a.trackID > b.trackID) ||
          ((a.trackID == b.trackID) &&
           ((a.spos > b.spos) || ((a.spos == b.spos) && (a.qpos > b.qpos)))));
}

bool operator==(const PointPair& a, const PointPair& b) {
  return ((a.trackID == b.trackID) &&
          (a.qpos == b.qpos) &&
          (a.spos == b.spos));
}
