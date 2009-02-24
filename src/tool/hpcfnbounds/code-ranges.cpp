#include <map>
using namespace std;

#include "code-ranges.h"
#include "process-ranges.h"

/******************************************************************************
 * forward declarations 
 *****************************************************************************/


/******************************************************************************
 * types
 *****************************************************************************/

class CodeRange {
public:
  CodeRange(void *_start, void *_end, long _offset, bool discover);
  void Process();
  bool Contains(void *addr);
  bool Discover() { return discover; }
  void *Relocate(void *addr); 
  long Offset() { return offset; }
private:
  void *start;
  void *end;
  long offset;
  bool discover;
};


typedef map<void*,CodeRange*> CodeRangeSet;


/******************************************************************************
 * local variables 
 *****************************************************************************/

static CodeRangeSet code_ranges;



/******************************************************************************
 * interface operations 
 *****************************************************************************/


long
offset_for_fn(void *addr)
{
  CodeRangeSet::iterator it = code_ranges.lower_bound(addr);

  if (it != code_ranges.begin() && it != code_ranges.end()) {
    if (--it != code_ranges.begin()) {
      CodeRange *r = (*it).second;
      if (r->Contains(addr)) return r->Offset();
    }
  }
  return 0L; //should never get here
} 


bool 
consider_possible_fn_address(void *addr)
{
  CodeRangeSet::iterator it = code_ranges.lower_bound(addr);

  if (it == code_ranges.begin() || it == code_ranges.end()) return false;
  if (--it == code_ranges.begin()) return false;

  CodeRange *r = (*it).second;
  return r->Contains(addr) & r->Discover();
} 


void 
new_code_range(void *start, void *end, long offset, bool discover)
{
  code_ranges.insert(pair<void*,CodeRange*>(start, 
					    new CodeRange(start, end, offset, discover)));
}


void 
process_code_ranges()
{
  process_range_init();
  CodeRangeSet::iterator it = code_ranges.begin();
  for (; it != code_ranges.end(); it++) {
    CodeRange *r = (*it).second;
    r->Process();
  }
}

/******************************************************************************
 * private operations 
 *****************************************************************************/

CodeRange::CodeRange(void *_start, void *_end, long _offset, bool _discover) 
{ 
  start = _start;
  end = _end; 
  offset = _offset;
  discover = _discover;
}

void * 
CodeRange::Relocate(void *addr)
{ 
  return (void *)(offset + (char *) addr); 
}

bool 
CodeRange::Contains(void *addr)
{
  return (addr >= start) && (addr < end);
}

void 
CodeRange::Process()
{
  process_range(-offset, Relocate(start), Relocate(end), discover);
}

