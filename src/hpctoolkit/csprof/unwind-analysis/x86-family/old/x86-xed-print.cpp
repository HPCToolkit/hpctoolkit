#include <iostream>
#include <iomanip>
using namespace std;

#include "x86-xed-print.h"


void print_flags(xed_decoded_inst_t* xptr) {
    unsigned int i, nflags;
    if (xed_decoded_inst_uses_rflags(xptr)) {
        cout << "FLAGS:" << endl;
        const xed_simple_flag_t* rfi = xed_decoded_inst_get_rflags_info(xptr);
        if (xed_simple_flag_reads_flags(rfi)) {
            cout <<  "   reads-rflags ";
        }
        else if (xed_simple_flag_writes_flags(rfi)) {
            //XED provides may-write and must-write information
            if (xed_simple_flag_get_may_write(rfi)) {
                cout << "  may-write-rflags ";
            }
            if (xed_simple_flag_get_must_write(rfi)) {
                cout << "  must-write-rflags ";
            }
        }
        nflags = xed_simple_flag_get_nflags(rfi);
        for( i=0;i<nflags ;i++) {
            const xed_flag_action_t* fa = xed_simple_flag_get_flag_action(rfi,i);
            char buf[500];
            xed_flag_action_print(fa,buf,500);
            cout  <<  buf << " ";
        }
        cout << endl;
        // or as as bit-union
        const xed_flag_set_t* read_set    = xed_simple_flag_get_read_flag_set(rfi);
        const xed_flag_set_t* written_set = xed_simple_flag_get_written_flag_set(rfi);
        char buf[500];
        xed_flag_set_print(read_set,buf,500);
        cout << "  read: " << buf << endl;
        xed_flag_set_print(written_set,buf,500);
        cout << "  written: " << buf << endl;
    }
}

void print_memops(xed_decoded_inst_t* xptr) {
    unsigned int i, memops = xed_decoded_inst_number_of_memory_operands(xptr);
    cout << "Memory Operands" << endl;
    
    for( i=0;i<memops ; i++)   {
        bool r_or_w = false;
        cout << "  " << i << " ";
        if ( xed_decoded_inst_mem_read(xptr,i)) {
            cout << "read ";
            r_or_w = true;
        }
        if (xed_decoded_inst_mem_written(xptr,i)) {
            cout << "written ";
            r_or_w = true;
        }
        if (!r_or_w) {
            cout << "agen "; // LEA instructions
        }
        xed_reg_enum_t seg = xed_decoded_inst_get_seg_reg(xptr,i);
        if (seg != XED_REG_INVALID) {
            cout << "SEG= " << xed_reg_enum_t2str(seg) << " ";
        }
        xed_reg_enum_t base = xed_decoded_inst_get_base_reg(xptr,i);
        if (base != XED_REG_INVALID) {
            cout << "BASE= " << xed_reg_enum_t2str(base) << "/"
                 <<  xed_reg_class_enum_t2str(xed_reg_class(base)) << " "; 
        }
        xed_reg_enum_t indx = xed_decoded_inst_get_index_reg(xptr,i);
        if (i == 0 && indx != XED_REG_INVALID) {
            cout << "INDEX= " << xed_reg_enum_t2str(indx)
                 << "/" <<  xed_reg_class_enum_t2str(xed_reg_class(indx)) << " ";
            if (xed_decoded_inst_get_scale(xptr,i) != 0) {
                // only have a scale if the index exists.
                cout << "SCALE= " <<  xed_decoded_inst_get_scale(xptr,i) << " ";
            }
        }
        uint_t disp_bits = xed_decoded_inst_get_memory_displacement_width(xptr,i);
        if (disp_bits) {
            cout  << "DISPLACEMENT_BYTES= " << disp_bits << " ";
            int64_t disp = xed_decoded_inst_get_memory_displacement(xptr,i);
            cout << hex << setfill('0') << setw(16) << disp << setfill(' ') << dec;
        }
        cout << endl;
    }
    cout << "  MemopBytes = " << xed_decoded_inst_get_memory_operand_length(xptr,0) << endl;
}

void print_operands(xed_decoded_inst_t* xptr) {
    unsigned int i, noperands;
    cout << "Operands" << endl;
    const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
    noperands = xed_inst_noperands(xi);
    for( i=0; i < noperands ; i++) { 
        const xed_operand_t* op = xed_inst_operand(xi,i);
        xed_operand_enum_t op_name = xed_operand_name(op);
        cout << i << " " << xed_operand_enum_t2str(op_name) << " ";
        switch(op_name) {
          case XED_OPERAND_AGEN:
          case XED_OPERAND_MEM0:
          case XED_OPERAND_MEM1:
            // we print memops in a different function
            break;
          case XED_OPERAND_PTR:  // pointer (always in conjunction with a IMM0)
          case XED_OPERAND_RELBR: { // branch displacements
              uint_t disp_bits = xed_decoded_inst_get_branch_displacement_width(xptr);
              if (disp_bits) {
                  cout  << "BRANCH_DISPLACEMENT_BYTES= " << disp_bits << " ";
                  int32_t disp = xed_decoded_inst_get_branch_displacement(xptr);
                  cout << hex << setfill('0') << setw(8) << disp << setfill(' ') << dec;
              }
            }
            break;

          case XED_OPERAND_IMM0: { // immediates
              uint_t width = xed_decoded_inst_get_immediate_width(xptr);
              if (xed_decoded_inst_get_immediate_is_signed(xptr)) {
                  int32_t x =xed_decoded_inst_get_signed_immediate(xptr);
                  cout << hex << setfill('0') << setw(8) << x << setfill(' ') << dec 
                       << '(' << width << ')';
              }
              else {
                  uint64_t x = xed_decoded_inst_get_unsigned_immediate(xptr); 
                  cout << hex << setfill('0') << setw(16) << x << setfill(' ') << dec 
                       << '(' << width << ')';
              }
              break;
          }
          case XED_OPERAND_IMM1: { // immediates
              uint8_t x = xed_decoded_inst_get_second_immediate(xptr);
              cout << hex << setfill('0') << setw(2) << x << setfill(' ') << dec;
              break;
          }

          case XED_OPERAND_REG0:
          case XED_OPERAND_REG1:
          case XED_OPERAND_REG2:
          case XED_OPERAND_REG3:
          case XED_OPERAND_REG4:
          case XED_OPERAND_REG5:
          case XED_OPERAND_REG6:
          case XED_OPERAND_REG7:
          case XED_OPERAND_REG8:
          case XED_OPERAND_REG9:
          case XED_OPERAND_REG10:
          case XED_OPERAND_REG11:
          case XED_OPERAND_REG12: {
              xed_reg_enum_t r = xed_decoded_inst_get_reg(xptr, op_name);
              cout << xed_operand_enum_t2str(op_name) << "=" << xed_reg_enum_t2str(r);
              break;
          }
          default:
            cout << "[Not currently printing value of field " << xed_operand_enum_t2str(op_name) << ']';
            break;

        }
        cout << " " << xed_operand_visibility_enum_t2str(xed_operand_operand_visibility(op))
             << " / " << xed_operand_action_enum_t2str(xed_operand_rw(op))
             << " / " << xed_operand_width_enum_t2str(xed_operand_width(op))
             << endl;
    }
}
