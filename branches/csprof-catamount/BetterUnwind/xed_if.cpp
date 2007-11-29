#include <iostream>
#include <strstream>
using namespace std;
#include "xed-interface.H"
#include "intervals.h"
#include "find.h"

#if 0
#include "mem.h"
// johnmc - we should include mem.h, but it defines a variable
#else
extern "C" {
void* csprof_malloc(size_t size);
};
#endif


using namespace XED;



xed_state_t dstate(XED_MACHINE_MODE_LONG_64, ADDR_WIDTH_64b, ADDR_WIDTH_64b);

// The state of the machine -- required for decoding
// static xed_state_t dstate(XED_MACHINE_MODE_LONG_64,
//			  ADDR_WIDTH_64b,
//			  ADDR_WIDTH_64b);

// create the decoded instruction object

extern "C" {
  void xed_init(void);
  void xed_inst(void *ins);
  int xed_inst2(void *ins);
  void xed_show(void);
  void idump(unwind_interval *u);

#include "pmsg.h"
  // turn off msgs @ compile time
  // comment out code below 
  // define DBG true to turn on all sorts of printing
#if 1
#define DBG false
#else
#define DBG true
#endif
}
  //
  // debug block
static bool idebug  = DBG;
static bool ildebug = DBG;
static bool irdebug = DBG;
static bool jdebug = DBG;
static int dump_to_stdout = 0;

static bool bp_frames_found = false;

static unwind_interval poison = {
  0L,
  0L,
  POISON,
  0,
  0,
  BP_HOSED,
  0,
  0,
  NULL,
  NULL
};

void xed_init(void){
   // initialize the XED tables -- one time.
  xed_tables_init();
}

void
print_flags(xed_decoded_inst_t& xedd)
{
    if (xedd.uses_rflags())
    {
        cerr << "FLAGS:" << endl;
        const xed_rflag_t* rfi = xedd.get_rflags_info();
        if (rfi->reads_rflags()) 
        {
            cerr <<  "   reads-rflags ";
        }
        if (rfi->writes_rflags())
        {
            //XED provides may-write and must-write information
            if (rfi->get_may_write())
            {
                cerr << "  may-write-rflags ";
            }
            if (rfi->get_must_write())
            {
                cerr << "  must-write-rflags ";
            }
        }
        
        for(unsigned int i=0;i<rfi->get_nflags() ;i++) 
        {
            const xed_rflag_action_t& fa = rfi->get_flag_action(i);
            cerr  <<  fa << " ";
        }
        cerr << endl;
        // or as as bit-union
        const xed_rflag_set_t& read_set = rfi->get_read_flag_set();
        const xed_rflag_set_t& written_set = rfi->get_written_flag_set();
        cerr << "  read: " << read_set << endl;
        cerr << "  written: " << written_set << endl;
    }
}

void print_memops(xed_decoded_inst_t& xedd){
  cerr << "Memory Operands" << endl;
  for(unsigned int i=0;i<xedd.number_of_memory_operands() ; i++)
    {
      cerr << "  " << i << " ";
      if ( xedd.mem_read(i))
        {
	  cerr << "read ";
        }
      else if (xedd.mem_written(i))
        {
	  cerr << "written ";
        }
      else 
        {
	  cerr << "agen "; // LEA instructions
        }

      xedregs_t seg = xedd.get_seg_reg(i);
      if (seg != XEDREG_INVALID) 
        {
	  cerr << "SEG= " << seg << " ";
        }

      xedregs_t base = xedd.get_base_reg(i);
      if (base != XEDREG_INVALID) 
        {
	  cerr << "BASE= " << base << "/" <<  xed_reg_class(base) << " ";
        }

      xedregs_t indx = xedd.get_index_reg();
      if (i == 0 && indx != XEDREG_INVALID) 
        {
	  cerr << "INDEX= " << indx
	       << "/" <<  xed_reg_class(indx) << " ";
	  if (xedd.get_scale() != 0) 
            {
	      // only have a scale if the index exists.
	      cerr << "SCALE= " <<  xedd.get_scale() << " ";
            }
        }

      if (i == 0)
        {
	  const unsigned int disp_bytes = xedd.get_disp().get_bytes();
	  if (disp_bytes && xedd.get_displacement_for_memop()) 
            {
	      cerr  << "DISPLACEMENT= " 
		    << disp_bytes
		    << " ";
	      const xed_immdis_t& disp =  xedd.get_disp();
	      disp.print_value_signed(cerr);
            }
        }

      cerr << endl;
    }

  cerr << "  MemopLength = " << xedd.get_memory_operand_length() << endl;

}

void print_operands(xed_decoded_inst_t& xedd){
  cerr << "Operands" << endl;
  for(unsigned int i=0; i < xedd.get_operand_count() ; i++){ 
    const xed_decoded_resource_t& r =  xedd.get_operand_resource(i);
    cerr << "  "
	 << i 
	 << " "
	 << r.get_res() 
	 << " ";
    switch(r.get_res())
      {
      case XED_RESOURCE_AGEN:
      case XED_RESOURCE_MEM0:
      case XED_RESOURCE_MEM1:
	// we print memops in a different function
	break;
      case XED_RESOURCE_DISP: // branch displacements
	{
	  // assert(xedd.has_disp() && ! xedd.get_displacement_for_memop());
	  assert(r.disp());
	  const xed_immdis_t& disp =  xedd.get_disp();
	  disp.print_value_signed(cerr);
	}
	break;

      case XED_RESOURCE_IMM: // immediates
	{

	  const xed_immdis_t& immed =  xedd.get_immed();
	  assert(immed.is_present());
	  immed.print_value_unsigned(cerr);
	}
	break;

      case XED_RESOURCE_IMM8: // Only for the ENTER instruction
	cerr << hex 
	     << "0x"
	     << std::setfill('0')
	     << (unsigned int)r.get_imm8() 
	     << std::setfill(' ')
	     << dec;
	break;

      case XED_RESOURCE_PSEUDO: // pseudo resources
	cerr  << r.get_pseudo_res();
	break;

      case XED_RESOURCE_REG: // registers
	cerr << r.get_reg();
	break;

      case XED_RESOURCE_INVALID:
	cerr  << "INVALID"; 
	break;
            
      default:
	EMSG("reached default, should not!!");
	assert(0);

      }
    cerr << " " 
	 << r.get_opvis() << " / "
	 << r.get_opnd_action() 
	 << endl;
  }
}

unwind_interval *fluke_interval(char *loc,unsigned int pos){
  unwind_interval *u = (unwind_interval *) csprof_malloc(sizeof(unwind_interval)); 
  u->startaddr = (unsigned long) loc;
  u->endaddr = (unsigned long) loc;
  u->ra_status = RA_SP_RELATIVE;
  u->ra_pos = pos;
  u->next = NULL;
  u->prev = NULL;
  return u; 
}

unwind_interval *newinterval(char *start,
			     ra_loc loc, unsigned int pos,
			     int bp_ra_pos,
			     bp_loc loc2, int pos2,
			     int bp_bp_pos,
			     unwind_interval *prev){

  unwind_interval *u = (unwind_interval *) csprof_malloc(sizeof(unwind_interval)); 
  u->startaddr = (unsigned long) start;
  u->endaddr = 0;
  u->ra_status = loc;
  u->ra_pos = pos;
  u->bp_status = loc2;
  u->bp_pos    = pos2;
  // johnmc 11am Nov 13, 2007
  u->bp_bp_pos = bp_bp_pos;
  u->bp_ra_pos = bp_ra_pos;
  u->next = NULL;
  u->prev = prev;
  return u; 
}

void link(unwind_interval *current, unwind_interval *next){
  current->endaddr = next->startaddr;
  current->next= next;
}

#define STR(s) case s: return #s
const char * const status(ra_loc l) 
{
  switch(l) {
   STR(RA_SP_RELATIVE);
   STR(RA_STD_FRAME);
   STR(RA_BP_FRAME);
   STR(RA_REGISTER);
   STR(POISON);

  default:
    assert(0);
  }
  return NULL;
}
const char * bp_status(bp_loc l){
  switch(l){
    STR(BP_UNCHANGED);
    STR(BP_SAVED);
    STR(BP_HOSED);
  default:
    assert(0);
  }
}

void dump(unwind_interval *u){
  char buf[1000];
  strstream cerr_s(buf,sizeof(buf));

  // replace endl at end with a \n and explicit 0;
  cerr_s <<  "start="<< (void *) u->startaddr << " end=" << (void *) u->endaddr <<
    " stat=" << status(u->ra_status) << " pos=" << u->ra_pos <<
    " bp_ra_pos=" << u->bp_ra_pos <<
    " bp_stat=" << bp_status(u->bp_status) << " bp_pos=" << u->bp_pos <<
    " bp_bp_pos=" << u->bp_bp_pos <<
    " next=" << u->next << " prev=" << u->prev << "\n" << '\0';

  PMSG(INTV,buf);
  if (dump_to_stdout) cerr << buf;
}

// wrapper to ensure a C interface
void idump(unwind_interval *u){
  dump(u);
}

bool no_push_bp_save(xed_decoded_inst_t xedd){
  
  if (xedd.get_iclass() == XEDICLASS_MOV){
    const xed_decoded_resource_t& r0 =  xedd.get_operand_resource(0);
    const xed_decoded_resource_t& r1 =  xedd.get_operand_resource(1);

    return ((r0.get_res() == XED_RESOURCE_MEM0) &&
	    (r0.get_opnd_action() == XED_OPND_ACTION_W) &&
	    (xedd.get_base_reg(0) == XEDREG_RSP) &&
	    (r1.get_res() == XED_RESOURCE_REG) &&
	    (r1.get_reg() == XEDREG_RBP) &&
	    (r1.get_opnd_action() == XED_OPND_ACTION_R));
  }
  return false;
}

unwind_interval *what(xed_decoded_inst_t& xedd, char *ins, 
		      unwind_interval *current,bool &bp_just_pushed, 
		      unwind_interval *&highwatermark){
  int size;

  bool next_bp_just_pushed = false;
  bool next_bp_popped      = false;
  unwind_interval *next    = NULL;

  // debug block
  bool rdebug = DBG;
  bool idebug = DBG;
  bool fdebug = DBG;
  bool mdebug = DBG;

  // johnmc: don't consider this a save of the caller's RBP if the
  // caller's RBP has already been saved. (change 2)
  if (no_push_bp_save(xedd) && current->bp_status != BP_SAVED){
    next = newinterval(ins + xedd.get_length(),
		       current->ra_status,current->ra_pos,current->bp_ra_pos,
		       BP_SAVED,xedd.get_disp().get_signed64(),current->bp_bp_pos,
		       current);
    highwatermark = next;
    return next;
  }
  // FIXME: look up bp action f enter
  if (xedd.get_iclass() == XEDICLASS_ENTER) {
    long offset = 8;
    int tmp;

    //    int op_count = xedd.get_operand_count();

    for(unsigned int i=0; i < xedd.get_operand_count() ; i++){
      const xed_decoded_resource_t& r =  xedd.get_operand_resource(i);
      switch(r.get_res()){
      case XED_RESOURCE_IMM:
	offset +=  xedd.get_immed().get_signed64();
	break;
      case XED_RESOURCE_IMM8:
	tmp = (int) r.get_imm8();
	offset += 8 * ((tmp >= 1) ? (tmp -1):tmp);
	break;
      default:
	// RSP, RBP ...
	break;
      }
    }
    PMSG(INTV,"new interval from ENTER");
    next = newinterval(ins + xedd.get_length(),
		       RA_STD_FRAME,
		       current->ra_pos + offset,
		       8,
		       BP_SAVED,
		       current->bp_pos + offset -8,
		       0,
		       current);
    highwatermark = next;
    return next;
  }
  for(unsigned int i=0; i < xedd.get_operand_count() ; i++){ 
    const xed_decoded_resource_t& r =  xedd.get_operand_resource(i);
    if (rdebug){
      cerr << "  "
	   << i 
	   << " "
	   << r.get_res() 
	   << " ";
    }
    switch(r.get_res()){
    case XED_RESOURCE_IMM: // immediates
      {
	const xed_immdis_t& immed =  xedd.get_immed();
	if (!immed.is_present()){
	  EMSG("!! something screwy: resource is imm, but immed not present !!");
	}
	assert(immed.is_present());
	if (idebug){
	  cerr << "   --immed value = ";
	  //                immed.print_value_unsigned(cerr);

	  immed.print_value_unsigned(cerr);
	  cerr << endl;
	}
      }
      break;

    case XED_RESOURCE_IMM8: // Only for the ENTER instruction
      EMSG("!! ENTER instruction IMM8 encountered, but not supposed to!!");
      break;
    case XED_RESOURCE_REG: // registers
      if ((r.get_reg() == XEDREG_RSP) && 
	  ((r.get_opnd_action() == XED_OPND_ACTION_RW) ||  
	   (r.get_opnd_action() == XED_OPND_ACTION_W))) {
	if (fdebug){
	  cerr << "\t writes RSP" << endl;
	}
	if ((current->ra_status == RA_SP_RELATIVE) || 1) {
	  if (xedd.get_category() == XED_CATEGORY_PUSH) {
	    switch(xedd.get_iclass()) {
	    case XEDICLASS_PUSH:
	      // hack: assume 64-bit mode
	      size = 8;
	      break;
	    case XEDICLASS_PUSHFQ:
	      size = 8;
	      break;
	    case XEDICLASS_PUSHF:
	      size = 2;
	      break;
	    case XEDICLASS_PUSHFD:
	      size = 4;
	      break;
	    default:
	      EMSG("!! push class, but not specfic push encountered");
	      assert(0);
	    }
	    PMSG(INTV,"new interval from PUSH");
	    next = newinterval(ins + xedd.get_length(), 
			       // RA_SP_RELATIVE, 
			       current->ra_status,
			       current->ra_pos + size,
			       current->bp_ra_pos,
			       current->bp_status,
			       current->bp_pos + size,
			       current->bp_bp_pos,
			       current);
	  }
	  else if (xedd.get_category() == XED_CATEGORY_POP) {
	    switch(xedd.get_iclass()) {
	    case XEDICLASS_POP:
	      // hack: assume 64-bit mode
	      size = -8;
	      break;
	    case XEDICLASS_POPFQ:
	      size = -8;
	      break;
	    case XEDICLASS_POPF:
	      size = -2;
	      break;
	    case XEDICLASS_POPFD:
	      size = -4;
	      break;
	    default:
	      EMSG("!! pop class, but not specific pop");
	      assert(0);
	    }
	    PMSG(INTV,"new interval from POP");
	    next = newinterval(ins + xedd.get_length(), 
			       // RA_SP_RELATIVE,
			       current->ra_status,
			       current->ra_pos + size,
			       current->bp_ra_pos,
			       current->bp_status,
			       current->bp_pos + size,
			       current->bp_bp_pos,
			       current);
	  }
	  else if (xedd.get_category() == XED_CATEGORY_DATAXFER){
	    const xed_decoded_resource_t& op1 = xedd.get_operand_resource(1);

	    if ((op1.get_res() == XED_RESOURCE_REG) &&
	        (op1.get_reg() == XEDREG_RBP)) {
	      PMSG(INTV,"Restore RSP from BP detected @%p",ins);
	      next = newinterval(ins + xedd.get_length(),
				 RA_SP_RELATIVE, current->bp_ra_pos, current->bp_ra_pos,
				 current->bp_status,current->bp_bp_pos, current->bp_bp_pos,
				 current);
	    }
	  }
	  else if ((xedd.get_iclass() == XEDICLASS_SUB) ||
		   (xedd.get_iclass() == XEDICLASS_ADD)) { 
	    const xed_immdis_t& immed =  xedd.get_immed();
	    if (immed.is_present()) {
		    int sign = (xedd.get_iclass() == XEDICLASS_ADD) ? -1 : 1;
		    long immedv = sign * immed.get_signed64();
		    if (mdebug){
		      cerr << "sp immed arith val = "
			   << immedv
			   << endl;
		    }
		    PMSG(INTV,"newinterval from ADD/SUB immediate");
		    next = newinterval(ins + xedd.get_length(), 
				       // RA_SP_RELATIVE, 
				       current->ra_status,
				       current->ra_pos + sign * immed.get_signed64(),
				       current->bp_ra_pos,
				       current->bp_status,
				       current->bp_pos + sign * immed.get_signed64(),
				       current->bp_bp_pos,
				       current);
		    if (xedd.get_iclass() == XEDICLASS_SUB) highwatermark = next;
	    }
	    else {
	      // johnmc - i think this is wrong in my replacement below
	      // I update ra_pos to be bp-relative. I also set bp_pos to zero after the move.
	      if (current->ra_status != RA_BP_FRAME){
		EMSG("!! NO immediate in sp add/sub @ %p, switching to BP_FRAME",ins);
		next = newinterval(ins + xedd.get_length(),
				   RA_BP_FRAME,
				   current->ra_pos,
				   current->bp_ra_pos,
				   current->bp_status,
				   current->bp_pos,
				   current->bp_bp_pos,
				   current);
		bp_frames_found = true;
		// assert(0 && "no immediate in add or sub!");
		// return &poison;
	      }
	    }
	  }
	  else{
	    if (xedd.get_category() != XED_CATEGORY_CALL){
	      // assert(0 && "unexpected mod of RSP!");
	      if (current->ra_status == RA_SP_RELATIVE){
		EMSG("interval: SP_RELATIVE unexpected mod of RSP @%p",ins);
		return &poison;
	      }
	    }
	  }
	}
      }
      else if (r.get_reg() == XEDREG_RBP) {
	if (r.get_opnd_action() == XED_OPND_ACTION_R) {
	  if (xedd.get_category() == XED_CATEGORY_PUSH) {
	    next_bp_just_pushed = true;
	  }
	}
	else if (((r.get_opnd_action() == XED_OPND_ACTION_RW) ||  
		    (r.get_opnd_action() == XED_OPND_ACTION_W))) {
	  if (fdebug){
	    cerr << "\t writes RBP" << endl;
	  }
	  if (xedd.get_category() == XED_CATEGORY_POP){
	    next_bp_popped = true;
	  }
          if( (current->bp_status == BP_SAVED) &&
	      (xedd.get_category() == XED_CATEGORY_DATAXFER)) {
	    const xed_decoded_resource_t& op1 = xedd.get_operand_resource(1);


	    if ((op1.get_res() == XED_RESOURCE_REG) &&
	        (op1.get_reg() == XEDREG_RSP)) {
	      PMSG(INTV,"new interval from PUSH BP");
	      next = newinterval(ins + xedd.get_length(), 
				 RA_STD_FRAME,
				 current->ra_pos,
				 current->ra_pos,
				 BP_SAVED,
				 current->bp_pos,
				 current->bp_pos,
				 current); 
	      highwatermark = next;
	    }
	  }
	  else if ((xedd.get_category() == XED_CATEGORY_POP) &&
		   (current->ra_status == RA_BP_FRAME)){
	    switch(xedd.get_iclass()) {
	    case XEDICLASS_POP:
	      // hack: assume 64-bit mode
	      size = -8;
	      break;
	    case XEDICLASS_POPFQ:
	      size = -8;
	      break;
	    case XEDICLASS_POPF:
	      size = -2;
	      break;
	    case XEDICLASS_POPFD:
	      size = -4;
	      break;
	    default:
	      EMSG("!! pop class, but no pop type !!");
	      assert(0);
	    }
	    // johnmc
	    EMSG("PROBLEM @%p: pop bp in BP_FRAME mode",ins);
	    next = newinterval(ins + xedd.get_length(), 
			       RA_SP_RELATIVE, 0, 0,
			       BP_UNCHANGED, current->bp_pos, 0,
			       current);
	  }
	} 
      }
      break;
    default:
      break;

    }
  }
  bp_just_pushed = next_bp_just_pushed;
  if (bp_just_pushed){
    next->bp_status = BP_SAVED;
    next->bp_pos    = 0;
  }
  if (next_bp_popped){
    next->bp_status = BP_UNCHANGED;
  }
  return (next) ? next : current;
}

// #define DEBUG 1

unwind_interval *find_first_bp_frame(unwind_interval *first){
  while (first && (first->ra_status != RA_BP_FRAME)) first = first->next;
  return first;
}

unwind_interval *find_first_non_decr(unwind_interval *first, 
				     unwind_interval *highwatermark){

  while (first && first->next && (first->ra_pos <= first->next->ra_pos) && 
	 (first != highwatermark)) {
    first = first->next;
  }
  return first;
}

void reset_to_canonical_interval(xed_decoded_inst_t xedd, unwind_interval *&current, 
				 unwind_interval *&next, char *&ins, char *end, 
				 bool irdebug, unwind_interval *first, 
				 unwind_interval *highwatermark, 
				 unwind_interval *&canonical_interval){

  // if the return is not the last instruction in the interval, 
  // set up an interval for code after the return 
  if (ins + xedd.get_length() < end){
    if (canonical_interval) {
      if ((highwatermark->bp_status == BP_SAVED) && 
	  (canonical_interval->bp_status != BP_SAVED) &&
	  (canonical_interval->ra_pos == highwatermark->ra_pos))
	canonical_interval = highwatermark;
      first = canonical_interval;
    } else if (bp_frames_found){ 
      // look for first bp frame
      first = find_first_bp_frame(first);
      canonical_interval = first;
    } else { 
      // look for first nondecreasing with no jmp
      first = find_first_non_decr(first,highwatermark);
      canonical_interval = first;
    }
    PMSG(INTV,"new interval from RET");
    ra_loc istatus =  
      (first->ra_status == RA_STD_FRAME) ? RA_BP_FRAME : first->ra_status;
    if ((current->ra_status != istatus) ||
	(current->bp_status != first->bp_status) ||
	(current->ra_pos != first->ra_pos) ||
	(current->bp_ra_pos != first->bp_ra_pos) ||
	(current->bp_bp_pos != first->bp_bp_pos) ||
	(current->bp_pos != first->bp_pos)) {
      next = newinterval(ins + xedd.get_length(),
			 istatus,first->ra_pos,first->bp_ra_pos,
			 first->bp_status,first->bp_pos,first->bp_bp_pos,
			 current);
      return;
    }
  }
  next = current; 
}
void set_flags(bool val)
{
  idebug  = val;
  ildebug = val;
  irdebug = val;
  jdebug = val;
}
void pl_build_intervals(char  *addr, int xed, int pint) 
{
	char *s, *e;
	unwind_interval *u;
	find_enclosing_function_bounds(addr, &s, &e);

	interval_status intervals;
	set_flags(xed);
	intervals = l_build_intervals(s, e - s);
	set_flags(false);
	dump_to_stdout = pint;
	_dbg_pmsg_stderr();
	for(u = intervals.first; u; u = u->next) {
		dump(u);
	}
	dump_to_stdout = 0;
	_dbg_pmsg_restore();
}

xed_decoded_inst_t _dbg_xedd;

void pl_xedd(void *ins){
  _dbg_xedd.init(dstate);
  xed_decode(&_dbg_xedd, reinterpret_cast<const UINT8*>(ins), 15);
}

void pl_dump_ins(void *ins){
  xed_decoded_inst_t xedd;
  xed_error_enum_t xed_error;

  xedd.init(dstate);
  xed_error = xed_decode(&xedd, reinterpret_cast<const UINT8*>(ins), 15);

  cerr << (void *) ins << " (" << xedd.get_length() << " bytes) " 
       << xedd.get_iclass() << endl;
  print_operands(xedd);
  print_memops(xedd);
}

int is_jump(xed_decoded_inst_t *xedd)
{
  switch(xedd->get_iclass()) {
  case XEDICLASS_JBE: 
  case XEDICLASS_JL:
  case XEDICLASS_JLE:
  case XEDICLASS_JMP:
  case XEDICLASS_JMP_FAR:
  case XEDICLASS_JNB:
  case XEDICLASS_JNBE:
  case XEDICLASS_JNL:
  case XEDICLASS_JNLE:
  case XEDICLASS_JNO:
  case XEDICLASS_JNP:
  case XEDICLASS_JNS:
  case XEDICLASS_JNZ:
  case XEDICLASS_JO:
  case XEDICLASS_JP:
  case XEDICLASS_JrCXZ:
  case XEDICLASS_JS:
  case XEDICLASS_JZ:
     return true;
  default:
     return false;
  }
  return false;
#if 0
    // careless, broken implementation
    return (xedd->get_iclass() >= XEDICLASS_JB) && (xedd->get_iclass() <= XEDICLASS_JZ);
#endif
}

#undef USE_CALL_LOOKAHEAD
#ifdef USE_CALL_LOOKAHEAD
unwind_interval *
call_lookahead(xed_decoded_inst_t *call_xedd, unwind_interval *current, char *ins)
{
  // Assumes: 'ins' is pointing at the instruction from which
  // lookahead is to occur (i.e, the instruction prior to the first
  // lookahead).

  unwind_interval *next;
  xed_error_enum_t xed_err;
  int length = call_xedd->get_length();
  xed_decoded_inst_t xeddobj;
  xed_decoded_inst_t* xedd = &xeddobj;
  char *jmp_ins_addr = ins + length;
  char *jmp_target = NULL;
  char *jmp_succ_addr = NULL;

  if (current->ra_status == RA_BP_FRAME) {
    return current;
  }

  // -------------------------------------------------------
  // requirement 1: unconditional jump with known target within routine
  // -------------------------------------------------------
  xedd->init(dstate);
  xed_err = xed_decode(xedd, reinterpret_cast<const UINT8*>(jmp_ins_addr), 15);
  if (xed_err != XED_ERROR_NONE) {
    return current;
  }

  if ((xedd->get_iclass() == XEDICLASS_JMP) || 
      (xedd->get_iclass() == XEDICLASS_JMP_FAR)) {
    if (xedd->number_of_memory_operands() == 0) {
      const xed_immdis_t& disp = xedd->get_disp();
      if (disp.is_present()) {
	long long offset = disp.get_signed64();
	jmp_succ_addr = jmp_ins_addr + xedd->get_length();
	jmp_target = jmp_succ_addr + offset;
      }
    }
  }
  if (jmp_target == NULL) {
    // jump of proper type not recognized 
    return current;
  }
  // FIXME: possibly test to ensure jmp_target is within routine

  // -------------------------------------------------------
  // requirement 2: jump target affects stack
  // -------------------------------------------------------
  xedd->init(dstate);
  xed_err = xed_decode(xedd, reinterpret_cast<const UINT8*>(jmp_target), 15);
  if (xed_err != XED_ERROR_NONE) {
    return current;
  }
  
  if ((xedd->get_iclass() == XEDICLASS_SUB) ||
      (xedd->get_iclass() == XEDICLASS_ADD)) {
    const xed_decoded_resource_t& r0 = xedd->get_operand_resource(0);
    if ((r0.get_res() == XED_RESOURCE_REG) && (r0.get_reg() == XEDREG_RSP)) {
      const xed_immdis_t& immed = xedd->get_immed();
      if (immed.is_present()) {
	int sign = (xedd->get_iclass() == XEDICLASS_ADD) ? -1 : 1;
	long offset = sign * immed.get_signed64();
	PMSG(INTV,"newinterval from ADD/SUB immediate");
	next = newinterval(jmp_succ_addr,
			   current->ra_status,
			   current->ra_pos + offset,
			   current->bp_ra_pos,
			   current->bp_status,
			   current->bp_pos + offset,
			   current->bp_bp_pos,
			   current);
        return next;
      }
    }
  }
  return current;
}
#endif

interval_status l_build_intervals(char *ins, unsigned int len)
{
  xed_decoded_inst_t xedd;

  interval_status xed_stat;
  xed_error_enum_t xed_error;
  unwind_interval *prev = NULL, *current = NULL, *next = NULL, *first = NULL;
  unwind_interval *highwatermark = 0;
  unwind_interval *canonical_interval = 0;
  bool bp_just_pushed = false;
  int ecnt = 0;


  bp_frames_found = false;  // handle return is different if there are any bp frames

  char *start = ins;
  char *end = ins + len;


  PMSG(INTV,"L_BUILD: start = %p, end = %p",start,end);
  current = newinterval(ins, RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, NULL);

  if (ildebug){
    PMSG(INTV,"L_BUILD:dump first");
    dump(current);
  }

  first = current;
  while (ins < end) {
    
    // fill in the machine mode (dstate)
    xedd.init(dstate);

    PMSG(INTV,"L_BUILD: in loop, ins = %p",ins);
    xed_error = xed_decode(&xedd, reinterpret_cast<const UINT8*>(ins), 15);
    switch(xed_error) {
    case XED_ERROR_NONE:
      if (idebug){
	PMSG(INTV,"trying to dump an instruction!!!");
	cerr << (void *) ins << " (" << xedd.get_length() << " bytes) " 
	     << xedd.get_iclass() << endl;
      }
      PMSG(INTV2,"%p yielded ok decode",ins);
      break;
    case XED_ERROR_BUFFER_TOO_SHORT:
      PMSG(INTV,"Reached buffer too short error");
    case XED_ERROR_GENERAL_ERROR:
      PMSG(INTV,"Reached General error");
    default:
      // Forge ahead, even though we know this is not a valid instruction
      // MWF + JMC
      //
      PMSG(INTV,"taking default incr action");
      ins++;
      ecnt++;
      continue;
      // goto done;
    }
    if (xedd.get_iclass() == XEDICLASS_LEAVE) {
      PMSG(INTV,"new interval from LEAVE");
      next = newinterval(ins + xedd.get_length(), RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, current);
    } 
    else if (xedd.get_category() == XED_CATEGORY_RET) {
      // if the return is not the last instruction in the interval, 
      // set up an interval for code after the return 
      if (ins + xedd.get_length() < end) {
        reset_to_canonical_interval(xedd, current, next, ins, end, irdebug, first, 
				    highwatermark, canonical_interval); 
      }
    } 
    else if ((xedd.get_iclass() == XEDICLASS_JMP) || 
	     (xedd.get_iclass() == XEDICLASS_JMP_FAR)) {

      if (highwatermark == 0)  highwatermark = current; 

#define RESET_FRAME_FOR_ALL_UNCONDITIONAL_JUMPS
#ifdef  RESET_FRAME_FOR_ALL_UNCONDITIONAL_JUMPS
      reset_to_canonical_interval(xedd, current, next, ins, end, irdebug, first, 
				  highwatermark, canonical_interval); 
#else
      if (xedd.number_of_memory_operands() == 0) {
	const xed_immdis_t& disp =  xedd.get_disp();
	if (disp.is_present()) {
	  long long offset = disp.get_signed64();
	  char *target = ins + offset;
	  if (jdebug) {
	    xedd.dump(cout);
	    cerr << "JMP offset = " << offset << ", target = " << (void *) target << ", start = " << (void *) start << ", end = " << (void *) end << endl;
	  } 
	  if (target < start || target > end) {
	    reset_to_canonical_interval(xedd, current, next, ins, end, irdebug, 
					first, highwatermark, canonical_interval); 
	  }
	}
	if (xedd.get_operand_count() >= 1) { 
	  const xed_decoded_resource_t& op0 = xedd.get_operand_resource(0);
	  if ((op0.get_res() == XED_RESOURCE_REG) && 
	      (current->ra_status == RA_SP_RELATIVE) && 
	      (current->ra_pos == 0)) {
	    // jump to a register when the stack is in an SP relative state
	    // with the return address at offset 0 in the stack.
	    // a good assumption is that this is a tail call. -- johnmc
	    // (change 1)
	    reset_to_canonical_interval(xedd, current, next, ins, end, irdebug, 
					first, highwatermark, canonical_interval); 
	  }
	}
      }
#endif
    } else if (xedd.get_category() == XED_CATEGORY_CALL) {

      if (highwatermark == 0) highwatermark = current;

#ifdef USE_CALL_LOOKAHEAD
      next = call_lookahead(&xedd, current, ins);
#endif
    } else if (is_jump(&xedd) && (highwatermark == 0)) {
      highwatermark = current;
    } else {
      next = what(xedd, ins, current, bp_just_pushed, highwatermark);
    }
    
    if (!next) {
      next = current;
    }
   
    if (next == &poison) {
      xed_stat.first_undecoded_ins = ins;
      xed_stat.errcode 		   = -1;
      xed_stat.first   		   = NULL;
      return xed_stat;
      // break;
    }
    if (next != current) {
      link(current, next);
      prev = current;
      current = next;
      if (ildebug){
	dump(current);
      }
    }
    if (idebug){
      print_operands(xedd);
    }
    ins += xedd.get_length();
    current->endaddr = (unsigned long) ins;
  }

  current->endaddr = (long unsigned int) end;

  // done:
  xed_stat.first_undecoded_ins = ins;
  xed_stat.errcode = ecnt;
  xed_stat.first   = first;
  return xed_stat;
}
