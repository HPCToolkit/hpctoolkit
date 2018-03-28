#ifndef __MetricAccessorInband_hpp__
#define __MetricAccessorInband_hpp__

#include "MetricAccessor.hpp"


class MetricAccessorInband : public MetricAccessor {
public:
  MetricAccessorInband(Prof::Metric::IData &_mdata) : mdata(_mdata) {}
  virtual double &idx(int mId, int size = 0) {
    return mdata.demandMetric(mId, size);
  }
  virtual double c_idx(int mId) const {
    return mdata.metric(mId);
  }
  virtual int idx_ge(int mId) const {
    return mId;
  }
private:
  Prof::Metric::IData &mdata;
};

#endif
