// should probably be rewritten
class PointPair {
 public:
  uint32_t trackID;
  uint32_t qpos;
  uint32_t spos;
  PointPair(uint32_t a, uint32_t b, uint32_t c);
};

bool operator<(const PointPair& a, const PointPair& b);
