/******************************************************************************
 * include files
 *****************************************************************************/

#include <stdio.h>
#include <string>

#include "code-ranges.h"
#include "function-entries.h"

#include <set>
#include <map>

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
typedef set<void*> CondBranchTargets;

static FunctionSet function_entries;
static CondBranchTargets cbtargets;

/******************************************************************************
 * interface operations 
 *****************************************************************************/


void 
dump_reachable_functions()
{
  char buffer[1024];
  FunctionSet::iterator i = function_entries.begin();
  for (; i != function_entries.end();) {
    Function *f = (*i).second;
    const char *sep = (++i != function_entries.end()) ? "," : " ";

    const char *name;
    if (f->comment) {
      name = f->comment->c_str();
    } else {
      if (is_branch_target(f->address)) continue;
      sprintf(buffer,"stripped_%p", f->address);
      name = buffer;
    }
    printf("   %p%s /* %s */\n", f->address, sep, name);
  }
}


void 
add_stripped_function_entry(void *addr)
{
  add_function_entry(addr, NULL);
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
is_branch_target(void *addr)
{
  return (cbtargets.find(addr) != cbtargets.end());
}


void 
add_branch_target(void *addr)
{
  if (!is_branch_target(addr)) cbtargets.insert(addr);
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
