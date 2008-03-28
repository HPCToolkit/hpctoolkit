/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2007 Intel Corporation 
All rights reserved. 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/// @file xed-ex1.cpp
/// @author Mark Charney   <mark.charney@intel.com>

extern "C" {
#include "xed-interface.h"
}
#include "xed-examples-ostreams.H"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
using namespace std;

int main(int argc, char** argv);

void print_attributes(xed_decoded_inst_t* xedd) {
    const xed_inst_t* xi = xed_decoded_inst_inst(xedd);
    unsigned int i, nattributes  =  xed_attribute_max();
    uint32_t all_attributes = xed_inst_get_attributes(xi);
    if (all_attributes == 0)
        return;
    cout << "ATTRIBUTES: ";
    for(i=0;i<nattributes;i++) {
        xed_attribute_enum_t attr = xed_attribute(i);
        if (xed_inst_get_attribute(xi,attr))
            cout << xed_attribute_enum_t2str(attr) << " ";
    }
    cout << endl;
}

void print_flags(xed_decoded_inst_t* xedd) {
    unsigned int i, nflags;
    if (xed_decoded_inst_uses_rflags(xedd)) {
        cout << "FLAGS:" << endl;
        const xed_simple_flag_t* rfi = xed_decoded_inst_get_rflags_info(xedd);
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

void print_memops(xed_decoded_inst_t* xedd) {
    unsigned int i, memops = xed_decoded_inst_number_of_memory_operands(xedd);
    cout << "Memory Operands" << endl;
    
    for( i=0;i<memops ; i++)   {
        bool r_or_w = false;
        cout << "  " << i << " ";
        if ( xed_decoded_inst_mem_read(xedd,i)) {
            cout << "read ";
            r_or_w = true;
        }
        if (xed_decoded_inst_mem_written(xedd,i)) {
            cout << "written ";
            r_or_w = true;
        }
        if (!r_or_w) {
            cout << "agen "; // LEA instructions
        }
        xed_reg_enum_t seg = xed_decoded_inst_get_seg_reg(xedd,i);
        if (seg != XED_REG_INVALID) {
            cout << "SEG= " << xed_reg_enum_t2str(seg) << " ";
        }
        xed_reg_enum_t base = xed_decoded_inst_get_base_reg(xedd,i);
        if (base != XED_REG_INVALID) {
            cout << "BASE= " << xed_reg_enum_t2str(base) << "/"
                 <<  xed_reg_class_enum_t2str(xed_reg_class(base)) << " "; 
        }
        xed_reg_enum_t indx = xed_decoded_inst_get_index_reg(xedd,i);
        if (i == 0 && indx != XED_REG_INVALID) {
            cout << "INDEX= " << xed_reg_enum_t2str(indx)
                 << "/" <<  xed_reg_class_enum_t2str(xed_reg_class(indx)) << " ";
            if (xed_decoded_inst_get_scale(xedd,i) != 0) {
                // only have a scale if the index exists.
                cout << "SCALE= " <<  xed_decoded_inst_get_scale(xedd,i) << " ";
            }
        }
        uint_t disp_bits = xed_decoded_inst_get_memory_displacement_width(xedd,i);
        if (disp_bits) {
            cout  << "DISPLACEMENT_BYTES= " << disp_bits << " ";
            int64_t disp = xed_decoded_inst_get_memory_displacement(xedd,i);
            cout << hex << setfill('0') << setw(16) << disp << setfill(' ') << dec;
        }
        cout << endl;
    }
    cout << "  MemopBytes = " << xed_decoded_inst_get_memory_operand_length(xedd,0) << endl;
}

void print_operands(xed_decoded_inst_t* xedd) {
    unsigned int i, noperands;
    cout << "Operands" << endl;
    const xed_inst_t* xi = xed_decoded_inst_inst(xedd);
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
              uint_t disp_bits = xed_decoded_inst_get_branch_displacement_width(xedd);
              if (disp_bits) {
                  cout  << "BRANCH_DISPLACEMENT_BYTES= " << disp_bits << " ";
                  int32_t disp = xed_decoded_inst_get_branch_displacement(xedd);
                  cout << hex << setfill('0') << setw(8) << disp << setfill(' ') << dec;
              }
            }
            break;

          case XED_OPERAND_IMM0: { // immediates
              uint_t width = xed_decoded_inst_get_immediate_width(xedd);
              if (xed_decoded_inst_get_immediate_is_signed(xedd)) {
                  int32_t x =xed_decoded_inst_get_signed_immediate(xedd);
                  cout << hex << setfill('0') << setw(8) << x << setfill(' ') << dec 
                       << '(' << width << ')';
              }
              else {
                  uint64_t x = xed_decoded_inst_get_unsigned_immediate(xedd); 
                  cout << hex << setfill('0') << setw(16) << x << setfill(' ') << dec 
                       << '(' << width << ')';
              }
              break;
          }
          case XED_OPERAND_IMM1: { // immediates
              uint8_t x = xed_decoded_inst_get_second_immediate(xedd);
              cout << hex << setfill('0') << setw(2) << (int)x << setfill(' ') << dec;
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
          case XED_OPERAND_REG12:
          case XED_OPERAND_REG13:
          case XED_OPERAND_REG14:
          case XED_OPERAND_REG15: {
              xed_reg_enum_t r = xed_decoded_inst_get_reg(xedd, op_name);
              cout << xed_operand_enum_t2str(op_name) << "=" << xed_reg_enum_t2str(r);
              break;
          }
          default:
            cout << "[Not currently printing value of field " << xed_operand_enum_t2str(op_name) << ']';
            break;

        }
        cout << " " << xed_operand_visibility_enum_t2str(xed_operand_operand_visibility(op))
             << " / " << xed_operand_action_enum_t2str(xed_operand_rw(op))
             << " / " << xed_operand_width_enum_t2str(xed_operand_width(op));
        cout << " bytes=" << xed_decoded_inst_operand_length(xedd,i);
        cout << endl;
    }
}

int main(int argc, char** argv) {
    xed_state_t dstate;
    xed_decoded_inst_t xedd;
    int i, bytes = 0;
    unsigned char itext[XED_MAX_INSTRUCTION_BYTES];
    bool long_mode = false;
    unsigned int first_argv;

    xed_tables_init();
    xed_state_zero(&dstate);
    if (argc > 2 && strcmp(argv[1], "-64") == 0) 
        long_mode = true;

    if (long_mode)  {
        first_argv = 2;
        dstate.mmode=XED_MACHINE_MODE_LONG_64;
    }
    else {
        first_argv=1;
        xed_state_init(&dstate,
                       XED_MACHINE_MODE_LEGACY_32, 
                       XED_ADDRESS_WIDTH_32b, 
                       XED_ADDRESS_WIDTH_32b);
    }

    xed_decoded_inst_zero_set_mode(&xedd, &dstate);

    for(  i=first_argv ;i < argc; i++) {
        unsigned int x;
        // sscanf is deprecated for MSVS8, so I'm using istringstreams
        //sscanf(argv[i],"%x", &x);
        istringstream s(argv[i]);
        s >> hex >> x;
        assert(bytes < XED_MAX_INSTRUCTION_BYTES);
        itext[bytes++] = STATIC_CAST(uint8_t,x);
    }
    if (bytes == 0) {
        cout << "Must supply some hex bytes" << endl;
        exit(1);
    }

    cout << "Attempting to decode: " << hex << setfill('0') ;
    for(i=0;i<bytes;i++)
        cout <<  setw(2) << static_cast<uint_t>(itext[i]) << " ";
    cout << endl << setfill(' ') << dec;

    xed_error_enum_t xed_error = xed_decode(&xedd, 
                                            REINTERPRET_CAST(const uint8_t*,itext), 
                                            bytes);
    switch(xed_error)    {
      case XED_ERROR_NONE:
        break;
      case XED_ERROR_BUFFER_TOO_SHORT:
        cout << "Not enough bytes provided" << endl;
        exit(1);
      case XED_ERROR_GENERAL_ERROR:
        cout << "Could not decode given input." << endl;
        exit(1);
      default:
        cout << "Unhandled error code " << xed_error_enum_t2str(xed_error) << endl;
        exit(1);
    }
        

    cout << "iclass " 
         << xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(&xedd))  << "\t";
    cout << "category " 
         << xed_category_enum_t2str(xed_decoded_inst_get_category(&xedd))  << "\t";
    cout << "ISA-extension " 
         << xed_extension_enum_t2str(xed_decoded_inst_get_extension(&xedd))  << endl;
    cout << "instruction-length " 
         << xed_decoded_inst_get_length(&xedd) << endl;
    cout << "effective-operand-width " 
         << xed_operand_values_get_effective_operand_width(xed_decoded_inst_operands_const(&xedd))  << endl;   
    cout << "effective-address-width "
         << xed_operand_values_get_effective_address_width(xed_decoded_inst_operands_const(&xedd))  << endl; 
    cout << "iform-enum-name " 
         << xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&xedd)) << endl;
    cout << "iform-enum-name-dispatch (zero based) " 
         << xed_decoded_inst_get_iform_enum_dispatch(&xedd) << endl;
    cout << "iclass-max-iform-dispatch "
         << xed_iform_max_per_iclass(xed_decoded_inst_get_iclass(&xedd))  << endl;

    // operands
    print_operands(&xedd);
    
    // memops
    print_memops(&xedd);
    
    // flags
    print_flags(&xedd);

    // attributes
    print_attributes(&xedd);
    return 0;
}
