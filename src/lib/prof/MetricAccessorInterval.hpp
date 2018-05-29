#ifndef __MetricAccessorInterval_hpp__
#define __MetricAccessorInterval_hpp__

#include "MetricAccessor.hpp"
#include "Metric-IData.hpp"
#include <set>
#include <utility>
#include <vector>
using std::make_pair;
using std::pair;
using std::set;
using std::vector;

typedef std::pair<int, int> MetricInterval;
typedef pair<MetricInterval, vector<double> > MI_Vec;

class MI_Vec_Compare {
public:
  bool operator()(const MI_Vec &lhs, const MI_Vec &rhs) const {
    return lhs.first.second <= rhs.first.first;
  }
};

class MetricAccessorInterval : public MetricAccessor {
private:
  set<MI_Vec, MI_Vec_Compare> table;
  set<MI_Vec>::iterator cacheIter;
  double cacheVal;
  int cacheItem;

  void flush(void) {
    if (cacheIter != table.end()) {
      MI_Vec copy(*cacheIter);
      copy.second[cacheItem - cacheIter->first.first] = cacheVal;
      table.erase(cacheIter++);
      cacheIter = table.insert(cacheIter, copy);
      return;
    }
    if (cacheVal == 0)
      return;
    MetricInterval ival;
    vector<double> val;
    ival = make_pair(cacheItem, cacheItem+1);
    val = vector<double>(1, cacheVal);
    cacheIter = table.insert(cacheIter, make_pair(ival, val));
    set<MI_Vec>::iterator nextIter = next(cacheIter);
    if (table.end() != nextIter &&
	nextIter->first.first == cacheItem+1) {
      ival.second = nextIter->first.second;
      val.insert(val.end(),
		 nextIter->second.begin(), nextIter->second.end());
      table.erase(nextIter);
    }
    set<MI_Vec>::iterator prevIter;
    if (table.begin() != cacheIter &&
	(prevIter = prev(cacheIter))->first.second == cacheItem) {
      ival.first = prevIter->first.first;
      val.insert(val.begin(),
		 prevIter->second.begin(), prevIter->second.end());
      table.erase(prevIter);
    }
    if (ival.first + 1 != ival.second) {
      table.erase(cacheIter++);
      cacheIter = table.insert(cacheIter, make_pair(ival, val));
    }
  }

  double lookup(int mId) {
    vector<double> dummy;
    MI_Vec key = make_pair(make_pair(mId, mId+1), dummy);
    set<MI_Vec>::iterator it = table.find(key);
    cacheIter = it;
    cacheItem = mId;
    if (it == table.end())
      return 0;
    int lo = it->first.first;
    int hi = it->first.second;
    if (lo <= mId && mId < hi)
      return it->second[mId - lo];
    return 0;
  }
 
public:
  MetricAccessorInterval(void):
    table(), cacheIter(table.end()), cacheVal(0.), cacheItem(-1)
  {
  }

  MetricAccessorInterval(const MetricAccessorInterval &src):
    table(src.table), cacheIter(table.end()), cacheVal(src.cacheVal),
    cacheItem(src.cacheItem)
  {
  }

  MetricAccessorInterval(Prof::Metric::IData &_mdata):
    table(), cacheIter(table.end()), cacheVal(0.), cacheItem(-1)
  {
    for (unsigned i = 0; i < _mdata.numMetrics(); ++i)
      idx(i) = _mdata.metric(i);
  }

  virtual double &idx(int mId, int size = 0) {
    if (cacheItem == mId)
       return cacheVal;
    flush();
    cacheVal = lookup(mId);
    return cacheVal;
  }

  virtual double c_idx(int mId) const {
    if (cacheItem == mId)
      return cacheVal;
    vector<double> dummy;
    MI_Vec key = make_pair(make_pair(mId, mId+1), dummy);
    set<MI_Vec>::iterator it = table.find(key);
    if (it == table.end())
      return 0;
    int lo = it->first.first;
    int hi = it->first.second;
    if (lo <= mId && mId < hi)
      return it->second[mId - lo];
    return 0;
  }

  virtual int idx_ge(int mId) const {
    vector<double> dummy;
    MI_Vec key = make_pair(make_pair(mId, mId+1), dummy);
    set<MI_Vec>::iterator it = table.lower_bound(key);
    if (it == table.end()) {
      if (mId < cacheItem)
	return cacheItem;
      return INT_MAX;
    }
    int lo = it->first.first;
    if (mId < cacheItem && cacheItem < lo)
      return cacheItem;
    if (mId < lo)
      return lo;
    return mId;
  }

#include <iostream>
  void dump(void)
  {
    flush();
    for (set<MI_Vec>::iterator it = table.begin(); it != table.end(); it++) {
      std::cout << "[" << it->first.first << ", " << it->first.second << "):";
      for (vector<double>::const_iterator i = it->second.begin(); i != it->second.end(); i++)
	std::cout << " " << *i;
      std::cout << "\n";
    }
  }

};

#endif
