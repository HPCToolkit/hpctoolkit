/******************************************************************************
 * include files
 *****************************************************************************/

#include <stdio.h>
#include <string>

extern "C" {
#include "xed-interface.h"
};

#include "code-ranges.h"
#include "process-ranges.h"

#include <set>
#include <map>

using namespace std;



/******************************************************************************
 * types
 *****************************************************************************/

class Function {
public:
  Function(void *_address, string *_comment);
  void *address;
  string *comment;
  int operator<(Function *right);
};



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void process_call(char *ins, xed_decoded_inst_t *xptr, long offset, 
			 void *start, void *end);

static void process_branch(char *ins, xed_decoded_inst_t *xptr);

static void add_function_entry(void *function_entry);

static int is_branch_target(void *addr);
static void add_branch_target(void *bt);

static void after_unconditional(long offset, char *ins, xed_decoded_inst_t *xptr);
static void process_branch(char *ins, xed_decoded_inst_t *xptr);



/******************************************************************************
 * local variables 
 *****************************************************************************/

static xed_state_t xed_machine_state_x86_64 = { XED_MACHINE_MODE_LONG_64, 
						XED_ADDRESS_WIDTH_64b,
						XED_ADDRESS_WIDTH_64b };


typedef map<void*,Function*> FunctionSet;
typedef set<void*> CondBranchTargets;

static FunctionSet function_entries;
static CondBranchTargets cbtargets;
static int stripped_count;

/******************************************************************************
 * interface operations 
 *****************************************************************************/

void 
new_function_entry(void *addr, string *comment)
{
  Function *f = new Function(addr, comment);
  function_entries.insert(pair<void*, Function*>(addr, f));
}


void
process_range_init()
{
  xed_tables_init();
}


void 
dump_reachable_functions()
{
  char buffer[1024];
  FunctionSet::iterator i = function_entries.begin();
  for (; i != function_entries.end();) {
    const char *sep;
    Function *f = (*i).second;
    i++;
    if (i != function_entries.end())  sep = ",";
    else sep = " ";

    const char *name;
    if (f->comment) {
      name = f->comment->c_str();
    } else {
      if (is_branch_target(f->address)) continue;
      sprintf(buffer,"/* stripped_%p */", f->address);
      name = buffer;
    }
    printf("   %p%s %s\n", f->address, sep, name);
  }
}


void 
process_range(long offset, void *vstart, void *vend, bool fn_discovery)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  int error_count = 0;
  char *ins = (char *) vstart;
  char *end = (char *) vend;

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state_x86_64);

  while (ins < end) {
    
    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

    if (xed_error != XED_ERROR_NONE) {
      error_count++; /* note the error      */
      ins++;         /* skip this byte      */
      continue;      /* continue onward ... */
    }

    xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
    switch(xiclass) {
    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR:
      process_call(ins, xptr, offset, vstart, vend);
      break;

    case XED_ICLASS_JMP: 
    case XED_ICLASS_JMP_FAR:
      // process_branch(ins + offset, xptr);
      if (fn_discovery) after_unconditional(offset, ins ,xptr);
      break;

    case XED_ICLASS_RET_FAR:
    case XED_ICLASS_RET_NEAR:
      if (fn_discovery) after_unconditional(offset, ins ,xptr);
      break;

    case XED_ICLASS_JBE: 
    case XED_ICLASS_JL: 
    case XED_ICLASS_JLE: 
    case XED_ICLASS_JNB:
    case XED_ICLASS_JNBE: 
    case XED_ICLASS_JNL: 
    case XED_ICLASS_JNLE: 
    case XED_ICLASS_JNO:
    case XED_ICLASS_JNP:
    case XED_ICLASS_JNS:
    case XED_ICLASS_JNZ:
    case XED_ICLASS_JO:
    case XED_ICLASS_JP:
    case XED_ICLASS_JRCXZ:
    case XED_ICLASS_JS:
    case XED_ICLASS_JZ:
      if (fn_discovery) process_branch(ins + offset, xptr);
      break;
      
    default:
      break;
    }

    ins += xed_decoded_inst_get_length(xptr);
  }
}



/******************************************************************************
 * private operations 
 *****************************************************************************/

static int 
is_padding(int c)
{
  return (c == 0x66) || (c == 0x90);
}


static void 
after_unconditional(long offset, char *ins, xed_decoded_inst_t *xptr)
{
  ins += xed_decoded_inst_get_length(xptr);
  unsigned char *uins = (unsigned char *) ins;
  if (0 || is_padding(*uins++)) { // try always adding
    for (; is_padding(*uins); uins++); // skip remaining padding 
    add_function_entry(uins + offset); // potential function entry point
  }
}

static void 
add_function_entry(void *addr)
{
  if (function_entries.find(addr) == function_entries.end()) {
    new_function_entry(addr, NULL);
  }
}


static int 
is_branch_target(void *addr)
{
  return (cbtargets.find(addr) != cbtargets.end());
}


static void 
add_branch_target(void *addr)
{
  if (!is_branch_target(addr)) cbtargets.insert(addr);
}


static void *
get_branch_target(char *ins, xed_decoded_inst_t *xptr, xed_operand_values_t *vals)
{
  int bytes = xed_operand_values_get_branch_displacement_length(vals);
  int offset = 0;
  switch(bytes) {
  case 1:
    offset = xed_operand_values_get_branch_displacement_byte(vals,0);
    break;
  case 4:
    offset = xed_operand_values_get_branch_displacement_int32(vals);
    break;
  default:
    assert(0);
  }
  char *end_of_call_inst = ins + xed_decoded_inst_get_length(xptr);
  char *target = end_of_call_inst + offset;
  return target;
}


static void 
process_call(char *ins, xed_decoded_inst_t *xptr, long offset, void *start, void *end)
{ 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      void *target = get_branch_target(ins,xptr,vals);
      void *vaddr = (char *)target + offset;
      if (is_valid_code_address(vaddr)) add_function_entry(vaddr);
    }

  }
}


static void 
process_branch(char *ins, xed_decoded_inst_t *xptr)
{ 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      add_branch_target(get_branch_target(ins,xptr,vals));
    }
  }
}


Function::Function(void *_address, string *_comment) 
{ 
  address = _address; 
  comment = _comment; 
}

int 
Function::operator<(Function *right)
{
  return this->address < right->address;
}


