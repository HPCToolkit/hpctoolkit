#ifndef __MetricDataAccessorInband_hpp__
#define __MetricDataAccessorInband_hpp__

#include "MetricDataAccessor.hpp"


class MetricDataAccessorInband : public MetricDataAccessor {
public:
  MetricDataAccessorInband(Prof::Metric::IData &_mdata) : mdata(_mdata) {}
  virtual double &idx(int mId, int size = 0) {
    return mdata.demandMetric(mId, size);
  }
  virtual double c_idx(int mId) const {
    return mdata.metric(mId);
  }
private:
  Prof::Metric::IData &mdata;
};

#endif
