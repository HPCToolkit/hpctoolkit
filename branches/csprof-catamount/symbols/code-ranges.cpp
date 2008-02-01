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
  CodeRange(void *_start, void *_end, long _offset);
  void Process(bool fn_discovery);
  bool Contains(void *addr);
  void *Relocate(void *addr); 
private:
  void *start;
  void *end;
  long offset;
};


typedef map<void*,CodeRange*> CodeRangeSet;


/******************************************************************************
 * local variables 
 *****************************************************************************/

static CodeRangeSet code_ranges;



/******************************************************************************
 * interface operations 
 *****************************************************************************/

bool 
is_valid_code_address(void *addr)
{
  CodeRangeSet::iterator it = code_ranges.lower_bound(addr);

  if (it == code_ranges.begin() || it == code_ranges.end()) return false;
  if (--it == code_ranges.begin()) return false;

  CodeRange *r = (*it).second;
  return r->Contains(addr);
} 


void 
new_code_range(void *start, void *end, long offset)
{
  code_ranges.insert(pair<void*,CodeRange*>(start, 
					    new CodeRange(start, end, offset)));
}


void 
process_code_ranges(bool fn_discovery)
{
  process_range_init();
  CodeRangeSet::iterator it = code_ranges.begin();
  for (; it != code_ranges.end(); it++) {
    CodeRange *r = (*it).second;
    r->Process(fn_discovery);
  }
}

/******************************************************************************
 * private operations 
 *****************************************************************************/

CodeRange::CodeRange(void *_start, void *_end, long _offset) 
{ 
  start = _start;
  end = _end; 
  offset = _offset;
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
CodeRange::Process(bool fn_discovery)
{
  process_range(-offset, Relocate(start), Relocate(end), fn_discovery);
}

