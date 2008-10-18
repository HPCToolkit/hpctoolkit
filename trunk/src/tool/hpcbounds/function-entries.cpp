/******************************************************************************
 * include files
 *****************************************************************************/

#include <stdio.h>
#include <string>

#include "code-ranges.h"
#include "function-entries.h"
#include "intervals.h"

#include <map>
#include <set>

using namespace std;


/******************************************************************************
 * types
 *****************************************************************************/

class Function {
public:
  Function(void *_address, string *_comment, bool _isvisible);
  void AppendComment(const string *c);
  void *address;
  string *comment;
  bool isvisible;
  int operator<(Function *right);
};



/******************************************************************************
 * forward declarations
 *****************************************************************************/

static void new_function_entry(void *addr, string *comment, bool isvisible);



/******************************************************************************
 * local variables 
 *****************************************************************************/

typedef map<void*,Function*> FunctionSet;
typedef set<void*> ExcludedFunctionSet;

static FunctionSet function_entries;
static ExcludedFunctionSet excluded_function_entries;

static intervals cbranges;

/******************************************************************************
 * interface operations 
 *****************************************************************************/

void
exclude_function_entry(void *addr)
{
  excluded_function_entries.insert(addr);
}


void 
dump_reachable_functions()
{
  char buffer[1024];
  FunctionSet::iterator i = function_entries.begin();
  int first = 1;
  for (; i != function_entries.end();) {
    Function *f = (*i).second;
    ++i;

    const char *name;
    if (!f->isvisible && !is_possible_fn(f->address)) continue;
    if (f->comment) {
      name = f->comment->c_str();
    } else {
      // inferred functions must be at least 16 bytes long
      if (i != function_entries.end()) {
        Function *nextf = (*i).second;
	 if ((((unsigned long) nextf->address) - 
              ((unsigned long) f->address)) < 16) continue;
      }
      sprintf(buffer,"stripped_%p", f->address);
      name = buffer;
    }
    if (first) printf("   %p /* %s */", f->address, name);
    else printf(",\n   %p /* %s */", f->address, name);
    first = 0;
  }
  if (!first) printf("\n");
}


void 
add_stripped_function_entry(void *addr)
{
  // only add the function if it hasn't been specifically excluded 
  if (excluded_function_entries.find(addr) == excluded_function_entries.end())  {
    add_function_entry(addr, NULL, false);
  }
}


bool 
query_function_entry(void *addr)
{
  FunctionSet::iterator it = function_entries.find(addr); 

  if (it == function_entries.end()) return false;
  else return true;
}


void 
add_function_entry(void *addr, const string *comment, bool isvisible)
{
  FunctionSet::iterator it = function_entries.find(addr); 

  if (it == function_entries.end()) {
    new_function_entry(addr, comment ? new string(*comment) : NULL, isvisible);
  } else {
    Function *f = (*it).second;
    if (comment) {
      f->AppendComment(comment);
    } else if (f->comment) {
      f->isvisible = true;
    }
  }
}

void
entries_in_range(void *start, void *end, vector<void *> &result)
{
#ifdef DEBUG_ENTRIES_IN_RANGE
  printf("function entries for range [%p, %p]\n", start, end);
#endif
  FunctionSet::iterator it = function_entries.find(start); 
  for (; it != function_entries.end(); it++) {
    void *addr = (*it).first;
    if (addr > end) return;  
#ifdef DEBUG_ENTRIES_IN_RANGE
    printf("  %p\n", addr);
#endif
    result.push_back(addr);
  }
}



bool contains_function_entry(void *address)
{
  FunctionSet::iterator it = function_entries.find(address); 
  if (it != function_entries.end()) return true;
  else return false;
}



/******************************************************************************
 * private operations 
 *****************************************************************************/

static void 
new_function_entry(void *addr, string *comment, bool isvisible)
{
  Function *f = new Function(addr, comment, isvisible);
  function_entries.insert(pair<void*, Function*>(addr, f));
}


int 
is_possible_fn(void *addr)
{
  return (cbranges.contains(addr) == NULL);
}


// test if an address is within a range rather than at its start
int 
inside_protected_range(void *addr)
{
  std::pair<void *const, void *> *interval = cbranges.contains(addr);
  if (interval != NULL && (addr > interval->first)) return 1;
  return 0;
}


void 
add_protected_range(void *start, void *end)
{
  cbranges.insert(start,end);
}


Function::Function(void *_address, string *_comment, bool _isvisible) 
{ 
  address = _address; 
  comment = _comment; 
  isvisible = _isvisible; 
}


void
Function::AppendComment(const string *c) 
{ 
  *comment = *comment + ", " + *c;
}


int 
Function::operator<(Function *right)
{
  return this->address < right->address;
}
