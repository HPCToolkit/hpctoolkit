#include <iostream>
using namespace std;
#include "xed-interface.H"
#include "intervals.h"

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
}

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

unwind_interval *newinterval(char *start, ra_loc loc, unsigned int pos,
			     unwind_interval *prev)
{
  unwind_interval *u = (unwind_interval *) malloc(sizeof(unwind_interval)); 
  u->startaddr = (unsigned long) start;
  u->endaddr = 0;
  u->ra_status = loc;
  u->ra_pos = pos;
  u->next = NULL;
  u->prev = prev;
  return u; 
}

void link(unwind_interval *current, unwind_interval *next)
{
  current->endaddr = next->startaddr;
  current->next= next;
}

const char * const status(ra_loc l) 
{
  switch(l) {
  case RA_BP_RELATIVE: return "RA_BP_RELATIVE";
  case RA_SP_RELATIVE: return "RA_SP_RELATIVE";
  case RA_REGISTER: return "RA_REGISTER";
  default:
    assert(0);
  }
  return NULL;
}

void dump(unwind_interval *u)
{
  cerr <<  "start="<< (void *) u->startaddr << " end=" << (void *) u->endaddr << 
    " stat=" << status(u->ra_status) << " pos=" << u->ra_pos <<
    " next=" << u->next << " prev=" << u->prev << endl;
}

// wrapper to ensure a C interface
void idump(unwind_interval *u){
  dump(u);
}

unwind_interval *what(xed_decoded_inst_t& xedd, char *ins, 
		      unwind_interval *current,bool &bp_just_pushed){
  int size;

  bool next_bp_just_pushed = false;
  unwind_interval *next = NULL;

  // debug block
  bool rdebug = false;
  bool idebug = false;
  bool fdebug = false;
  bool mdebug = false;

  if (xedd.get_iclass() == XEDICLASS_ENTER) {
    long offset = 8;
    int tmp;
    int op_count = xedd.get_operand_count();
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
    return newinterval(ins + xedd.get_length(),
		       RA_BP_RELATIVE,
		       current->ra_pos + offset,
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
	    next = newinterval(ins + xedd.get_length(), 
			       // RA_SP_RELATIVE, 
			       current->ra_status,
			       current->ra_pos + size, current);
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
	    next = newinterval(ins + xedd.get_length(), 
			       // RA_SP_RELATIVE, 
			       current->ra_status,
			       current->ra_pos + size, current);
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
		    next = newinterval(ins + xedd.get_length(), 
				       // RA_SP_RELATIVE, 
				       current->ra_status,
				       current->ra_pos + sign * immed.get_signed64(),
				       current);
	    }
	    else {
	      EMSG("!! NO immediate in sp add/sub !!");
	      // assert(0 && "no immediate in add or sub!");
	    }
	  }
	  else{
	    if (xedd.get_category() != XED_CATEGORY_CALL)
	      assert(0 && "unexpected mod of RSP!");
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
	  if (bp_just_pushed &&  
	      (xedd.get_category() == XED_CATEGORY_DATAXFER)) {
	    const xed_decoded_resource_t& op1 = xedd.get_operand_resource(1);
	    if (op1.get_reg() == XEDREG_RSP) {
	      next = newinterval(ins + xedd.get_length(), 
				 RA_BP_RELATIVE, current->ra_pos, 
				 current); 
	    }
	  }
	  else if ((xedd.get_category() == XED_CATEGORY_POP) &&
		   (current->ra_status == RA_BP_RELATIVE)){
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
	    next = newinterval(ins + xedd.get_length(), 
			       RA_SP_RELATIVE, 0, current);
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
  return (next) ? next : current;
}

// #define DEBUG 1

interval_status l_build_intervals(char  *ins, unsigned int len)
{
  xed_decoded_inst_t xedd;

  interval_status xed_stat;
  xed_error_enum_t xed_error;
  unwind_interval *prev, *current, *next, *first;
  bool bp_just_pushed = false;
  int ecnt = 0;

  // debug block
  bool idebug  = false;
  bool ildebug = false;
  bool irdebug = false;

  char *end = ins + len;

  xed_state_t dstate(XED_MACHINE_MODE_LONG_64, ADDR_WIDTH_64b, ADDR_WIDTH_64b);

  current = newinterval(ins, RA_SP_RELATIVE, 0, NULL);
  if (ildebug){
    dump(current);
  }

  first = current;
  while (ins < end) {
    
    // fill in the machine mode (dstate)
    xedd.init(dstate);

    xed_error = xed_decode(&xedd, reinterpret_cast<const UINT8*>(ins), 15);
    switch(xed_error) {
    case XED_ERROR_NONE:
      if (idebug){
	cerr << (void *) ins << " (" << xedd.get_length() << " bytes) " 
	     << xedd.get_iclass() << endl;
      }
      break;
    case XED_ERROR_BUFFER_TOO_SHORT:
    case XED_ERROR_GENERAL_ERROR:
    default:
      // Forge ahead, even though we know this is not a valid instruction
      // MWF + JMC
      //
      ins++;
      ecnt++;
      continue;
      // goto done;
    }

    if (xedd.get_iclass() == XEDICLASS_LEAVE) {
      next = newinterval(ins + xedd.get_length(), RA_SP_RELATIVE, 0, current);
    } else if (xedd.get_category() == XED_CATEGORY_RET) {
      // if the return is not the last instruction in the interval, 
      // set up an interval for code after the return 
      if (ins + xedd.get_length() < end) {
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
      }
    } else {
      next = what(xedd, ins, current, bp_just_pushed);
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

 done:
  xed_stat.first_undecoded_ins = ins;
  xed_stat.errcode = ecnt;
  xed_stat.first   = first;
  return xed_stat;
}
