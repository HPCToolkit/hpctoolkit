#ifndef __MetricDataAccessor_hpp__
#define __MetricDataAccessor_hpp__

class MetricDataAccessor {
public:
  virtual double &idx(int mId, int size = 0) = 0;
  virtual double c_idx(int mId) const = 0;
};

#endif

