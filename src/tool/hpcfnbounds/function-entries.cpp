// -*-Mode: C++;-*-
// $Id$

/******************************************************************************
 * include files
 *****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string>

#include "code-ranges.h"
#include "function-entries.h"
#include "process-ranges.h"
#include "intervals.h"

#include <map>
#include <set>

using namespace std;


/******************************************************************************
 * types
 *****************************************************************************/

class Function {
public:
  Function(void *_address, string *_comment, bool _isvisible, int _call_count);
  void AppendComment(const string *c);
  void *address;
  string *comment;
  bool isvisible;
  int call_count;
  int operator<(Function *right);
};


/******************************************************************************
 * forward declarations
 *****************************************************************************/

static void new_function_entry(void *addr, string *comment, bool isvisible, 
			       int call_count);
static void dump_function_entry(void *addr, const char *comment);
static void end_of_function_entries(void);


/******************************************************************************
 * local variables 
 *****************************************************************************/

typedef map<void*,Function*> FunctionSet;
typedef set<void*> ExcludedFunctionSet;

static FunctionSet function_entries;
static ExcludedFunctionSet excluded_function_entries;

static intervals cbranges;

#define ADDR_BUF_SIZE  1024

static void *addr_buf[ADDR_BUF_SIZE];
static long num_entries_in_buf = 0;
static long num_entries_total = 0;


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
	if (f->call_count == 0 && 
	    (((unsigned long) nextf->address) - 
	     ((unsigned long) f->address)) < 16)  {
	  long offset = offset_for_fn(f->address);
	  if (!range_contains_control_flow((char *) f->address + offset, 
					   ((char *) nextf->address + 
					    offset)))
	    continue;
	}
      }
      sprintf(buffer,"stripped_%p", f->address);
      name = buffer;
    }
    dump_function_entry(f->address, name);
  }
  end_of_function_entries();
}


void 
add_stripped_function_entry(void *addr, int call_count)
{
  // only add the function if it hasn't been specifically excluded 
  if (excluded_function_entries.find(addr) == 
      excluded_function_entries.end())  {
    add_function_entry(addr, NULL, false, call_count);
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
add_function_entry(void *addr, const string *comment, bool isvisible, 
		   int call_count)
{
  FunctionSet::iterator it = function_entries.find(addr); 

  if (it == function_entries.end()) {
    new_function_entry(addr, comment ? new string(*comment) : NULL, 
		       isvisible, call_count);
  } else {
    Function *f = (*it).second;
    if (comment) {
      f->AppendComment(comment);
    } else if (f->comment) {
      f->isvisible = true;
    }
    f->call_count += call_count;
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


long num_function_entries(void)
{
  return (num_entries_total);
}


/******************************************************************************
 * private operations 
 *****************************************************************************/

//
// Write one function entry, possibly to multiple files.
// The binary format is buffered.
//
static void
dump_function_entry(void *addr, const char *comment)
{
  num_entries_total++;

  if (binary_fmt_fd() >= 0) {
    addr_buf[num_entries_in_buf] = addr;
    num_entries_in_buf++;
    if (num_entries_in_buf == ADDR_BUF_SIZE) {
      write(binary_fmt_fd(), addr_buf, num_entries_in_buf * sizeof(void *));
      num_entries_in_buf = 0;
    }
  }

  if (c_fmt_fp() != NULL) {
    if (num_entries_total > 1)
      fprintf(c_fmt_fp(), ",\n");
    fprintf(c_fmt_fp(), "   %p /* %s */", addr, comment);
  }

  if (text_fmt_fp() != NULL) {
    fprintf(text_fmt_fp(), "%p    %s\n", addr, comment);
  }
}


static void
end_of_function_entries(void)
{
  if (binary_fmt_fd() >= 0 && num_entries_in_buf > 0) {
    write(binary_fmt_fd(), addr_buf, num_entries_in_buf * sizeof(void *));
    num_entries_in_buf = 0;
  }

  if (c_fmt_fp() != NULL && num_entries_total > 0)
    fprintf(c_fmt_fp(), "\n");
}


static void 
new_function_entry(void *addr, string *comment, bool isvisible, int call_count)
{
  Function *f = new Function(addr, comment, isvisible, call_count);
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


Function::Function(void *_address, string *_comment, bool _isvisible, 
		   int _call_count) 
{ 
  address = _address; 
  comment = _comment; 
  isvisible = _isvisible; 
  call_count = _call_count;
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
