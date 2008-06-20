//*****************************************************************************
// system includes
//*****************************************************************************

#include <iostream>



//*****************************************************************************
// local includes
//*****************************************************************************
#include "intervals.h"

using namespace std;


//*****************************************************************************
// interface operations
//*****************************************************************************


//-----------------------------------------------------------------------------
// Method insert: 
//
//   insert [start,end) into the interval set. merge with overlapping in the
//   set to maintain the invariant that no point is contained in more
//   than one interval.
//-----------------------------------------------------------------------------
void 
intervals::insert(void * start, void * end) 
{
  map<void *, void *>::iterator lb;
  map<void *, void *>::iterator ub;

  lb = mymap.upper_bound(start);
  ub = mymap.upper_bound(end);

  // inserted interval spans or overlaps one or more intervals if 
  // iterators are not equal
  bool overlaps = (lb != ub); 

  if (lb != mymap.begin()) {
    lb--; // move back one interval to see if it contains start
    if (start <= (*lb).second) {
      // start is within existing interval; adjust lower bound of union interval
      start = (*lb).first;
      overlaps = true;
    } else lb++; // lb doesn't contain start; thus, erasure shouldn't include lb
  }

  if (ub != mymap.begin()) {
    ub--; // move back one interval to see if it contains end
    if (end <= (*ub).second) {
      // end is within existing interval; adjust upper bound of union interval 
      end = (*ub).second;
      overlaps = true;
    } 
    ub++; // increment ub because erase will only remove up to but not ub
  }
    
  if (overlaps) {
    // remove any intervals that overlap the range being inserted. erase will
    // remove intervals starting at lb up to but not including ub.
    mymap.erase(lb,ub);
  }

  // insert the refined interval
  mymap[start] = end;

  return;
}


//-----------------------------------------------------------------------------
// Method contains:
//    if any interval contains the query point i, return it. 
//    otherwise, return NULL
//-----------------------------------------------------------------------------
pair<void *const, void *> *
intervals::contains(void * i) 
{
  map<void *, void *>::iterator up;
  up = mymap.upper_bound(i);
  if (up != mymap.begin()) {
    --up;
    if (i >= (*up).first && i < (*up).second)  return &(*up);
  }
  return NULL;
}


//-----------------------------------------------------------------------------
// Method dump:
//    print all of the intervals in the set to stdout
//-----------------------------------------------------------------------------
void 
intervals::dump()
{
  map<void *, void *>::iterator it;
  cout << "intervals: ";
  for (it = mymap.begin(); it != mymap.end(); it++) 
    cout << "[" << (*it).first << "," << (*it).second << ") "; 
  cout << endl;
}



//*****************************************************************************
// private operations 
//*****************************************************************************
// #define UNIT_TEST
#ifdef UNIT_TEST
intervals ranges;

void 
test_interval_set_insert(unsigned long start, unsigned long end) 
{
  void *vstart = (void *) start;
  void *vend = (void *) end;
  cout << "inserting [" << vstart << "," << vend << ")" << endl; 
  ranges.insert(vstart, vend);
  ranges.dump();
}


void 
test_interval_set_contains(unsigned long i) 
{
  pair<void *const, void *> *result = ranges.contains((void *) i); 
  cout << "search for " << i << ": "; 
  if (result)
    cout << "found interval [" << result->first << ","<< result->second << ")";
  else cout << "no interval found";   
  cout << endl;   
}


main()
{
  test_interval_set_insert(10,20);

  // disjoint insertion
  test_interval_set_insert(5,7);

  // disjoint insertion
  test_interval_set_insert(30,32);

  // endpoints equal
  test_interval_set_insert(20,28);

  // disjoint insertion
  test_interval_set_insert(60,89);

  // start within interval
  test_interval_set_insert(27,45);

  // end within interval
  test_interval_set_insert(57,61);

  // joining intervals
  test_interval_set_insert(41,58);

  // subsuming intervals
  test_interval_set_insert(1,90);

  // disjoint insertion
  test_interval_set_insert(94,99);

  // disjoint insertion
  test_interval_set_insert(100,110);

  // disjoint insertion
  test_interval_set_insert(115,120);

  // disjoint insertion
  test_interval_set_insert(140,145);

  // disjoint insertion
  test_interval_set_insert(155,165);

  // linking and subsumption
  test_interval_set_insert(95,142);

  // linking and subsumption
  test_interval_set_insert(93,186);

  // subsumption
  test_interval_set_insert(92,187);

  test_interval_set_contains(72);
  test_interval_set_contains(21);
  test_interval_set_contains(33);
  test_interval_set_contains(12);
  test_interval_set_contains(96);
  test_interval_set_contains(8);
  test_interval_set_contains(0);
  test_interval_set_contains(200);
  test_interval_set_contains(91);
}
#endif // UNIT_TEST
