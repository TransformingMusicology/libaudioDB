extern "C" {
#include "audioDB/audioDB_API.h"
}
#include "audioDB/audioDB-internals.h"

PointPair::PointPair(uint32_t a, uint32_t b, uint32_t c) :
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
