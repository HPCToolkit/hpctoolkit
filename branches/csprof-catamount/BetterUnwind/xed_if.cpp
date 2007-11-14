#include <iostream>
#include <strstream>
using namespace std;
#include "xed-interface.H"
#include "intervals.h"
#include "find.h"


using namespace XED;




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
#if 1
  0,
#endif  
  BP_HOSED,
  0,
#if 1
  0,
#endif
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

void
print_memops(xed_decoded_inst_t& xedd)
{

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

void
print_operands(xed_decoded_inst_t& xedd)
{
    cerr << "Operands" << endl;
    for(unsigned int i=0; i < xedd.get_operand_count() ; i++)
    { 
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
  unwind_interval *u = (unwind_interval *) malloc(sizeof(unwind_interval)); 
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
#if 1
			     int bp_ra_pos,
#endif
			     bp_loc loc2, int pos2,
#if 1
			     int bp_bp_pos,
#endif
			     unwind_interval *prev){

  unwind_interval *u = (unwind_interval *) malloc(sizeof(unwind_interval)); 
  u->startaddr = (unsigned long) start;
  u->endaddr = 0;
  u->ra_status = loc;
  u->ra_pos = pos;
  u->bp_status = loc2;
  u->bp_pos    = pos2;
  #if 1
  // johnmc 11am Nov 13, 2007
  u->bp_bp_pos = bp_bp_pos;
  u->bp_ra_pos = bp_ra_pos;
  #endif
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
#if 1
    " bp_ra_pos=" << u->bp_ra_pos <<
#endif
    " bp_stat=" << bp_status(u->bp_status) << " bp_pos=" << u->bp_pos <<
#if 1
    " bp_bp_pos=" << u->bp_bp_pos <<
#endif
    " next=" << u->next << " prev=" << u->prev << "\n" << '\0';

  PMSG(INTV,buf);
  if (dump_to_stdout) cerr << buf;
}

// wrapper to ensure a C interface
void idump(unwind_interval *u){
  dump(u);
}

unwind_interval *what(xed_decoded_inst_t& xedd, char *ins, 
		      unwind_interval *current,bool &bp_just_pushed){
  int size;

  bool next_bp_just_pushed = false;
  bool next_bp_popped      = false;
  unwind_interval *next    = NULL;

  // debug block
  bool rdebug = DBG;
  bool idebug = DBG;
  bool fdebug = DBG;
  bool mdebug = DBG;

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
    return newinterval(ins + xedd.get_length(),
		       RA_STD_FRAME,
		       current->ra_pos + offset,
#if 1
		       8,
#endif
		       BP_SAVED,
		       current->bp_pos + offset -8,
#if 1
		       0,
#endif
		       current);
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
#if 1
			       current->bp_ra_pos,
#endif
			       current->bp_status,
			       current->bp_pos + size,
#if 1
			       current->bp_bp_pos,
#endif
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
#if 1
			       current->bp_ra_pos,
#endif
			       current->bp_status,
			       current->bp_pos + size,
#if 1
			       current->bp_bp_pos,
#endif
			       current);
	  }
	  else if (xedd.get_category() == XED_CATEGORY_DATAXFER){
	    const xed_decoded_resource_t& op1 = xedd.get_operand_resource(1);

	    if ((op1.get_res() == XED_RESOURCE_REG) &&
	        (op1.get_reg() == XEDREG_RBP)) {
	      PMSG(INTV,"Restore RSP from BP detected @%p",ins);
#if 1
	      next = newinterval(ins + xedd.get_length(),
				 RA_SP_RELATIVE, current->bp_ra_pos, current->bp_ra_pos,
				 current->bp_status,current->bp_bp_pos, current->bp_bp_pos,
				 current);
#else
	      next = newinterval(ins + xedd.get_length(),
				 RA_SP_RELATIVE, current->ra_pos,
				 current->bp_status,current->bp_pos,
				 current);
#endif
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
#if 1
				       current->bp_ra_pos,
#endif
				       current->bp_status,
				       current->bp_pos + sign * immed.get_signed64(),
#if 1
				       current->bp_bp_pos,
#endif
				       current);
	    }
	    else {
#if 1 
// johnmc - i think this is wrong in my replacement below, I update ra_pos to be bp-relative. I also set bp_pos to zero after the move.
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
#else
		next = newinterval(ins + xedd.get_length(),
				   RA_BP_FRAME,
				   current->ra_pos - current->bp_pos,
				   current->bp_status,
				   0,
				   current);
#endif
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
#if 0 // just pushed replaced by BP_SAVED
	  if (bp_just_pushed &&
#endif
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
#if 1
				 current->ra_pos,
#endif
				 BP_SAVED,
				 current->bp_pos,
#if 1
				 current->bp_pos,
#endif
				 current); 
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
#if 1
	    // johnmc
	    EMSG("PROBLEM @%p: pop bp in BP_FRAME mode",ins);
	    next = newinterval(ins + xedd.get_length(), 
			       RA_SP_RELATIVE, 0, 0,
			       BP_UNCHANGED, current->bp_pos, 0,
			       current);
#else
	    next = newinterval(ins + xedd.get_length(), 
			       RA_SP_RELATIVE, 0,
			       BP_UNCHANGED, 0,
			       current);
#endif
	  }
	} 
      }
      break;
#if 0
    case XED_RESOURCE_INVALID:
      cerr  << "INVALID"; 
      break;
#endif
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
  while (first && (first->ra_status != RA_BP_FRAME)){
    first = first->next;
  }
  return first;
}

unwind_interval *find_first_non_decr(unwind_interval *first, unwind_interval *firstjmpi){

  while (first && first->next && (first->ra_pos <= first->next->ra_pos) && (first != firstjmpi)) {
    first = first->next;
  }
  return first;
}
void handle_return(xed_decoded_inst_t xedd, unwind_interval *&current, unwind_interval *&next, char *&ins, char *end, 
		   bool irdebug, unwind_interval *first, unwind_interval *firstjmpi){

  // if the return is not the last instruction in the interval, 
  // set up an interval for code after the return 
  if (ins + xedd.get_length() < end){
    if (bp_frames_found){ // look for first bp frame
      first = find_first_bp_frame(first);
    }
    else { // look for first nondecreasing with no jmp
      first = find_first_non_decr(first,firstjmpi);
    }
#if 1
    PMSG(INTV,"new interval from RET");
    next = newinterval(ins + xedd.get_length(),
		       first->ra_status,first->ra_pos,first->bp_ra_pos,
		       first->bp_status,first->bp_pos,first->bp_bp_pos,
		       current);
#else
    next = newinterval(ins + xedd.get_length(),
		       first->ra_status,first->ra_pos,
		       first->bp_status,first->bp_pos,
		       current);
#endif
  }
}
#ifdef NO_IS_OLD
#if 1
	    next = newinterval(ins + xedd.get_length(),
			       RA_BP_FRAME,current->prev->ra_pos,current->prev->bp_ra_pos,
			       current->bp_status,current->bp_pos,current->bp_bp_pos,
			       current);
#else
	    next = newinterval(ins + xedd.get_length(),
			       RA_BP_FRAME,current->prev->ra_pos,
			       current->bp_status,current->bp_pos,
			       current);
#endif
      if (ins + xedd.get_length() < end) {
	if (current->prev){ 
	  if (current->prev->ra_status == RA_BP_FRAME) {
	  } else {
	    if (irdebug){
	      cerr << "--In RET interval backup" << endl;
	    }
	    next = current->prev;
	    if (irdebug){
	      cerr << "--looping thru prev" << endl;
	    }
	    while (first && first->next && (first->ra_pos <= first->next->ra_pos) && (first != firstjmpi)) {
	      if (first->next->ra_status == RA_BP_FRAME){
		first = first->next;
		break;
	      }
	      first = first->next;
	    }
	  }
	}
      }
}
#endif
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

interval_status l_build_intervals(char  *ins, unsigned int len)
{
  xed_decoded_inst_t xedd;

  interval_status xed_stat;
  xed_error_enum_t xed_error;
  unwind_interval *prev, *current, *next, *first;
  unwind_interval *firstjmpi = 0;
  bool bp_just_pushed = false;
  int ecnt = 0;


  bp_frames_found = false;  // handle return is different if there are any bp frames

  char *start = ins;
  char *end = ins + len;

  xed_state_t dstate(XED_MACHINE_MODE_LONG_64, ADDR_WIDTH_64b, ADDR_WIDTH_64b);

  PMSG(INTV,"L_BUILD: start = %p, end = %p",ins,end);
#if 1
  current = newinterval(ins, RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, NULL);
#else
  current = newinterval(ins, RA_SP_RELATIVE, 0, BP_UNCHANGED, 0, NULL);
#endif
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
#if 1
      PMSG(INTV,"new interval from LEAVE");
      next = newinterval(ins + xedd.get_length(), RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, current);
#else
      next = newinterval(ins + xedd.get_length(), RA_SP_RELATIVE, 0, BP_UNCHANGED, 0, current);
#endif
    } else if (xedd.get_category() == XED_CATEGORY_RET) {
      // if the return is not the last instruction in the interval, 
      // set up an interval for code after the return 
      if (ins + xedd.get_length() < end) {
#if 0
	if (current->prev){ 
	  if (current->prev->ra_status == RA_BP_RELATIVE) {
	    next = newinterval(ins + xedd.get_length(), RA_BP_RELATIVE, 
			       current->prev->ra_pos, current);
	  } else {
	    if (irdebug){
	      cerr << "--In RET interval backup" << endl;
	    }
	    next = current->prev;
	    if (irdebug){
	      cerr << "--looping thru prev" << endl;
	    }
	    while ((next->prev != NULL) && (next->ra_pos < next->prev->ra_pos)) {
	      next = next->prev;
	    }
	    next = newinterval(ins + xedd.get_length(), next->ra_status,
			       next->ra_pos, current);
	  }
	}
#else
        handle_return(xedd, current, next, ins, end, irdebug, first, firstjmpi); 
#endif
      }
    } else  if ((xedd.get_iclass() == XEDICLASS_JMP) || (xedd.get_iclass() == XEDICLASS_JMP_FAR)) { 	
            if (firstjmpi == 0) firstjmpi = current;
	    if (xedd.number_of_memory_operands() == 0) {
		    const xed_immdis_t& disp =  xedd.get_disp();
		    if (disp.is_present()) {
			    long long offset = disp.get_signed64();
			    char *target = ins + offset;
			    if (jdebug) {
	                            xedd.dump(cout);
				    cerr << "JMP offset = " << offset << ", target = " << (void *) target << ", start = " << (void *) start << ", end = " << (void *) end << endl;
			    } 
			    if (target < start || target > end)  handle_return(xedd, current, next, ins, end, irdebug, first, firstjmpi); 
		    }
	    }
    }else {
      if ((xedd.get_category() == XED_CATEGORY_CALL) && (firstjmpi == 0)) firstjmpi = current;
      next = what(xedd, ins, current, bp_just_pushed);
    }
    if (next == &poison){
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
