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
  Function(void *_address, string *_comment);
  void AppendComment(const string *c);
  void *address;
  string *comment;
  int operator<(Function *right);
};



/******************************************************************************
 * forward declarations
 *****************************************************************************/

static void new_function_entry(void *addr, string *comment);



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
    if (!is_possible_fn(f->address)) continue;
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
    add_function_entry(addr, NULL);
  }
}


void 
add_function_entry(void *addr, const string *comment)
{
  FunctionSet::iterator it = function_entries.find(addr); 

  if (it == function_entries.end()) {
    new_function_entry(addr, comment ? new string(*comment) : NULL);
  } else if (comment) {
    Function *f = (*it).second;
    f->AppendComment(comment);
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
new_function_entry(void *addr, string *comment)
{
  Function *f = new Function(addr, comment);
  function_entries.insert(pair<void*, Function*>(addr, f));
}


int 
is_possible_fn(void *addr)
{
  return (cbranges.contains(addr) == NULL);
}


void 
add_branch_range(void *start, void *end)
{
  cbranges.insert(start,end);
}


Function::Function(void *_address, string *_comment) 
{ 
  address = _address; 
  comment = _comment; 
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
