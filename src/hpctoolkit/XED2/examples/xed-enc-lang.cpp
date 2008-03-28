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
/// @file xed-enc-lang.cpp
/// @author Mark Charney   <mark.charney@intel.com>

// This is an example of how to use the encoder from scratch in the context
// of parsing a string from the command line.  


#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <cassert>
extern "C" {
#include "xed-interface.h"
#include "xed-portability.h"
#include "xed-examples-util.h"
}
#include "xed-enc-lang.H"




using namespace std;
static char xed_enc_lang_toupper(char c) {
    if (c >= 'a' && c <= 'z')
        return c-'a'+'A';
    return c;
}

static string upcase(string s) {
    string t = "";
    uint_t len = static_cast<uint_t>(s.size());
    uint_t i;
    for(i=0 ; i < len ; i++ ) 
        t = t + xed_enc_lang_toupper(s[i]);
    return t;
}

unsigned int
xed_split_args(const string& sep, 
               const string& input, 
               vector<string>& output_array)
{
    // returns the number of args
    // rip off the separator characters and split the src string based on separators.
    
    // find the string between last_pos and pos. pos is after last_pos
    string::size_type last_pos = input.find_first_not_of(sep, 0);
    string::size_type pos = input.find_first_of(sep, last_pos);  
    if (CLIENT_VERBOSE3)
        printf("input %s\tlast_pos " XED_FMT_U " pos " XED_FMT_U "\n", 
               input.c_str() , STATIC_CAST(uint_t,last_pos), STATIC_CAST(uint_t,pos));
    int i=0;
    while( pos != string::npos && last_pos != string::npos ) 
    {
        string a = input.substr(last_pos, pos-last_pos);
        output_array.push_back(a); 
        if (CLIENT_VERBOSE3)
            printf("\t\tlast_pos " XED_FMT_U " pos " XED_FMT_U " i %d\n", 
                   STATIC_CAST(uint_t,last_pos),
                   STATIC_CAST(uint_t,pos),
                   i);
        last_pos = input.find_first_not_of(sep, pos);
        pos = input.find_first_of(sep, last_pos);  
        i++;
    }
    if (last_pos != string::npos && pos == string::npos)
    {
        if (CLIENT_VERBOSE3)
            printf("\t\tGetting last substring at " XED_FMT_U "\n", STATIC_CAST(uint_t,last_pos));
        string a = input.substr(last_pos); // get the rest of the string
        output_array.push_back(a);
        i++;
    }
    if (CLIENT_VERBOSE3)
        printf("\t returning %d\n",i);
    return i;
}

vector<string> 
tokenize(const string& s,
	 const string& delimiter) {
    vector<string> v;
    (void) xed_split_args(delimiter, s, v);
    return v;
}


void slash_split(const string& src,
                 string& first, // output
                 string&  second) //output
{
  string::size_type p = src.find("/");
  if (p == string::npos) {
    first = src;
    second = "";
  }
  else {
    first = src.substr(0,p);
    second = src.substr(p+1);
  }
}

class immed_parser_t {
  public:
    bool valid;
    string immed;
    unsigned int width_bits;
    uint64_t immed_val;
    string tok0;

    immed_parser_t(const string& s, 
                   const string& arg_tok0) //CONS
        : valid(false),
          tok0(arg_tok0)
    {
        vector<string> vs = tokenize(s,"(),");
        if (vs.size() == 2) {
            if (vs[0] == tok0) {
                string immed_str = vs[1];
                immed_val = convert_ascii_hex_to_int(immed_str.c_str());
                width_bits = immed_str.size()*4; // nibbles to bits
                valid = true;
            }
        }
    }

    void
    print(ostream& o) const {
        o << tok0 
          << "(" ;
        if (valid) 
            o << hex << immed_val << dec;
        else 
            o << "???";
        o << ")";
    }

};

ostream& operator<<(ostream& o, const immed_parser_t& x) 
{
    x.print(o);
    return o;
}


class mem_bis_parser_t 
{
    // parse: MEM[length]([segment:]base,index,scale[,displacement])
    // parse: AGEN(base,index,scale[,displacement])
    // The displacement is optional
    // The length of the memop is usually optional 
    //   but required for x87 ops, for example.
  public:
    bool valid;
    bool mem;

    bool agen;
    bool disp_valid;
    string segment;
    string base;
    string index;
    string scale;
    string disp; //displacement
    xed_reg_enum_t segment_reg;
    xed_reg_enum_t base_reg;
    xed_reg_enum_t index_reg;
    uint8_t scale_val;


    int64_t disp_val;
    unsigned int disp_width_bits;

    unsigned int mem_len;

    void
    print(ostream& o) const {
        if (agen) 
            o << "AGEN"; 
        if (mem) 
            o << "MEM"; 
        if (mem_len) 
            o << setw(1) << mem_len;
        o << "(";
        if (segment_reg != XED_REG_INVALID)
            o << segment_reg << ":";
        o << base_reg;
        o << "," << index_reg 
          << "," 
          << (unsigned int) scale_val;
        if (disp_valid) 
            o <<  "," << disp;
        o << ")";
     
    }
  
    mem_bis_parser_t(const string& s) //CONS
        : valid(false),
          disp_valid(false),
          base("INVALID"),
          index("INVALID"),
          scale("1"),
          segment_reg(XED_REG_INVALID),
          base_reg(XED_REG_INVALID),
          index_reg(XED_REG_INVALID),
          disp_val(0),
          disp_width_bits(0),
          mem_len(0)
    {

        mem = false;
        agen = false;
        vector<string> vs = tokenize(s,"(),");
        uint_t ntokens = static_cast<uint_t>(vs.size());
        if (ntokens >= 2 && ntokens <= 5) {
            if (vs[0] == "AGEN") {
                agen = true;
            }
            else if (vs[0].substr(0,3) == "MEM") {
                mem = true;
                if (vs[0].size() > 3) {
                    string len = vs[0].substr(3);
                    mem_len = strtol(len.c_str(),0,0);
                    //printf("mem_len  = " XED_FMT_U "\n", mem_len);
                }
            }
            else             {
                return;
            }

            segment = "INVALID";
            string seg_and_base = upcase(vs[1]);
            vector<string> sb = tokenize(seg_and_base,":");
            int seg_and_base_tokens = STATIC_CAST(int,sb.size());
            if (seg_and_base_tokens == 1) {
                segment = "INVALID";
                base = sb[0];
            }
            else if (seg_and_base_tokens == 2) {
                if (agen) {
                    xedex_derror("AGENs cannot have segment overrides");
                }
                segment = sb[0];
                base = sb[1];
            }
            else            {
                printf("seg_and_base_tokens = %d\n",seg_and_base_tokens);
                xedex_derror("Bad segment-and-base specifier.");
            }

            if (base == "-" || base == "NA") {
                base = "INVALID";
            }
            if (ntokens > 2) {
                index = upcase(vs[2]);
                if (index == "-" || index == "NA") {
                    index = "INVALID";
                }
            }

            if (ntokens > 3) {
                scale = vs[3];
                if (scale == "-" || scale == "NA") {
                    scale = "1";
                }
            }
            if (scale == "1" || scale == "2" || scale == "4" || scale == "8") {
                valid=true;
                scale_val = STATIC_CAST(uint8_t,strtol(scale.c_str(), 0, 10));
                segment_reg = str2xed_reg_enum_t(segment.c_str());
                base_reg = str2xed_reg_enum_t(base.c_str());
                index_reg = str2xed_reg_enum_t(index.c_str());

                // look for a displacement
                if (ntokens == 5 && vs[4] != "-") {
                    disp = vs[4];
                    disp_valid = true;
                    unsigned int nibbles = STATIC_CAST(int,disp.size());
                    if (nibbles & 1) {
                        // ensure an even number of nibbles
                        string zero("0");
                        disp = zero + disp;
                        nibbles++;
                    }
                    disp_val = convert_ascii_hex_to_int(disp.c_str());
                    disp_width_bits = nibbles*4; // nibbles to bits
                }
            }

        }
    
    }
};

ostream& operator<<(ostream& o, const mem_bis_parser_t& x) {
  x.print(o);
  return o;
}

xed_encoder_request_t parse_encode_request(ascii_encode_request_t& areq) {
    unsigned int i;
    xed_encoder_request_t req;
    xed_encoder_request_zero_set_mode(&req,&(areq.dstate)); // calls xed_encoder_request_zero()

    /* This is the important function here. This encodes an instruction from scratch.
       
    You must set:
    the machine mode (machine width, addressing widths)
    the effective operand width
    the iclass
    for some instructions you need to specify prefixes (like REP or LOCK).
    the operands:
           operand kind (XED_OPERAND_{AGEN,MEM0,MEM1,IMM0,IMM1,RELBR,PTR,REG0...REG15}
           operand order 
                    xed_encoder_request_set_operand_order(&req,operand_index, XED_OPERAND_*);
                    where the operand_index is a sequential index starting at zero.

           operand details 
                     FOR MEMOPS: base,segment,index,scale,displacement for memops, 
                  FOR REGISTERS: register name
                 FOR IMMEDIATES: immediate values
       
     */
    

    switch(xed_state_get_machine_mode(&(areq.dstate))) {
        // set the default width.
      case XED_MACHINE_MODE_LONG_64:
        xed_encoder_request_set_effective_operand_width(&req, 32);
        xed_encoder_request_set_effective_address_size(&req, 64);
        break;

      case XED_MACHINE_MODE_LEGACY_32:
      case XED_MACHINE_MODE_LONG_COMPAT_32:
        xed_encoder_request_set_effective_operand_width(&req, 32);
        xed_encoder_request_set_effective_address_size(&req, 32);
        break;

      case XED_MACHINE_MODE_LEGACY_16:
      case XED_MACHINE_MODE_LONG_COMPAT_16:
        xed_encoder_request_set_effective_operand_width(&req, 16);
        xed_encoder_request_set_effective_address_size(&req, 16);
        break;

      default:
        assert(0);
    }

    //FIXME: allow changing the effective address size from the above defaults.

    vector<string> tokens = tokenize(areq.command," ");
    // first token has the operand and our temporary hack for the immediate

    string first, second;
    unsigned int token_index = 0;

    while(token_index < tokens.size()) {
        slash_split(tokens[token_index], first, second);
        if (CLIENT_VERBOSE3)
            printf( "[%s][%s][%s]\n", tokens[0].c_str(), first.c_str(), second.c_str());

        if (token_index == 0 && first == "REP") {
            xed_encoder_request_set_rep(&req);
            token_index++;
            continue;
        }
        else if (token_index == 0 && first == "REPNE") {
            xed_encoder_request_set_repne(&req);
            token_index++;
            continue;
        }
  
        token_index++;
        break;
    }

    // we can attempt to override the mode 
    if (second == "8") 
        xed_encoder_request_set_effective_operand_width(&req, 8);
    else if (second == "16") 
        xed_encoder_request_set_effective_operand_width(&req, 16);
    else if (second == "32") 
        xed_encoder_request_set_effective_operand_width(&req, 32);
    else if (second == "64") 
        xed_encoder_request_set_effective_operand_width(&req, 64);

    first = upcase(first);
    xed_iclass_enum_t iclass =  str2xed_iclass_enum_t(first.c_str());
    if (iclass == XED_ICLASS_INVALID) {
        ostringstream os;
        os << "Bad instruction name: " << first;
        xedex_derror(os.str().c_str());
    }
    xed_encoder_request_set_iclass(&req, iclass );

    uint_t memop = 0;
    uint_t regnum = 0;
    // put the operands in the request. Loop through tokens 
    // (skip the opcode iclass, handled above)
    uint_t operand_index = 0;
    for( i=token_index; i < tokens.size(); i++, operand_index++ ) {
        string str_res_reg, second_x;
        slash_split(tokens[i], str_res_reg, second_x);
        str_res_reg = upcase(str_res_reg);
        // prune the AGEN or MEM(base,index,scale[,displacement]) text from str_res_reg
        // FIXME: add MEM(immed) for the OC1_A and OC1_O types????
        mem_bis_parser_t mem_bis(str_res_reg);
        if (mem_bis.valid) {
            if (mem_bis.mem) {
                if (memop == 0) {
                    // Tell XED that we have a memory operand
                    xed_encoder_request_set_mem0(&req);
                    // Tell XED that the mem0 operand is the next operand:
                    xed_encoder_request_set_operand_order(&req,operand_index, XED_OPERAND_MEM0);
                }
                else {
                    xed_encoder_request_set_mem1(&req);
                    // Tell XED that the mem1 operand is the next operand:
                    xed_encoder_request_set_operand_order(&req,operand_index, XED_OPERAND_MEM1);
                }
                memop++;
            }
            else if (mem_bis.agen) {
                // Tell XED we have an AGEN
                xed_encoder_request_set_agen(&req);
                // The AGEN is the next operand
                xed_encoder_request_set_operand_order(&req,operand_index, XED_OPERAND_AGEN);
            }
            else 
                assert(mem_bis.agen || mem_bis.mem);

            // fill in the memory fields
            xed_encoder_request_set_base0(&req, mem_bis.base_reg);
            xed_encoder_request_set_index(&req, mem_bis.index_reg);
            xed_encoder_request_set_scale(&req, mem_bis.scale_val);
            xed_encoder_request_set_seg0(&req, mem_bis.segment_reg);

            if (mem_bis.mem_len) 
                xed_encoder_request_set_memory_operand_length(&req, mem_bis.mem_len ); // BYTES
            if (mem_bis.disp_valid)
                xed_encoder_request_set_memory_displacement(&req,
                                                            mem_bis.disp_val,
                                                            mem_bis.disp_width_bits/8);
            continue;
        }

        immed_parser_t imm(str_res_reg, "IMM");
        if (imm.valid) {
            if (CLIENT_VERBOSE3) 
                printf("Setting immediate value to " XED_FMT_LX "\n", imm.immed_val);
            xed_encoder_request_set_uimm0_bits(&req, 
                                               imm.immed_val,
                                               imm.width_bits);
            xed_encoder_request_set_operand_order(&req,operand_index, XED_OPERAND_IMM0);
            continue;
        }
        immed_parser_t simm(str_res_reg, "SIMM");
        if (simm.valid) {
            if (CLIENT_VERBOSE3) 
                printf("Setting immediate value to " XED_FMT_LX "\n", simm.immed_val);
            xed_encoder_request_set_simm(&req, 
                                         STATIC_CAST(int32_t,simm.immed_val),
                                         simm.width_bits/8); //FIXME
            xed_encoder_request_set_operand_order(&req,operand_index, XED_OPERAND_IMM0);
            continue;
        }
        immed_parser_t imm2(str_res_reg, "IMM2");
        if (imm2.valid) {
            if (imm2.width_bits != 8)
                xedex_derror("2nd immediate must be just 1 byte long");
            xed_encoder_request_set_uimm1(&req, imm2.immed_val);
            xed_encoder_request_set_operand_order(&req,operand_index, XED_OPERAND_IMM1);
            continue;
        }

        immed_parser_t disp(str_res_reg, "BRDISP");
        if (disp.valid) {
            if (CLIENT_VERBOSE3) 
                printf("Setting  displacement value to " XED_FMT_LX "\n", disp.immed_val);
            xed_encoder_request_set_branch_displacement(&req,
                                                        STATIC_CAST(uint32_t,disp.immed_val),
                                                        disp.width_bits/8); //FIXME
            xed_encoder_request_set_operand_order(&req,operand_index, XED_OPERAND_RELBR);
            xed_encoder_request_set_relbr(&req);
            continue;
        }

        immed_parser_t ptr_disp(str_res_reg, "PTR");
        if (ptr_disp.valid) {
            if (CLIENT_VERBOSE3) 
                printf("Setting pointer displacement value to " XED_FMT_LX "\n", ptr_disp.immed_val);
            xed_encoder_request_set_branch_displacement(&req,
                                                        STATIC_CAST(uint32_t,ptr_disp.immed_val),
                                                        ptr_disp.width_bits/8); //FIXME
            xed_encoder_request_set_operand_order(&req,operand_index, XED_OPERAND_PTR);
            xed_encoder_request_set_ptr(&req);
            continue;
        }

        xed_reg_enum_t reg = str2xed_reg_enum_t(str_res_reg.c_str());
        if (reg == XED_REG_INVALID) {
            ostringstream os;
            os << "Bad register name: " << str_res_reg << " on operand " << i;
            xedex_derror(os.str().c_str()); // dies
        }
        // The registers operands aer numbered starting from the first one
        // as XED_OPERAND_REG0. We incremenet regnum (below) every time we add a
        // register operands.
        xed_operand_enum_t r = STATIC_CAST(xed_operand_enum_t,XED_OPERAND_REG0 + regnum);
        // store the register identifer in the operand storage field
        xed_encoder_request_set_reg(&req, r, reg);
        // store the operand storage field name in the encode-order array
        xed_encoder_request_set_operand_order(&req, operand_index, r);
        regnum++;
    } // for loop

    return req;
}
