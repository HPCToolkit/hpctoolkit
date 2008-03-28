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
/// @file xed-examples-util.H
/// @author Mark Charney   <mark.charney@intel.com>


#ifndef _XED_EXAMPLES_UTIL_H_
# define _XED_EXAMPLES_UTIL_H_

#include <stdio.h>
#include "xed-interface.h"

extern int intel_syntax;
extern int att_syntax;
extern int xed_syntax;
extern int client_verbose;

#define CLIENT_VERBOSE (client_verbose > 2)
#define CLIENT_VERBOSE1 (client_verbose > 3)
#define CLIENT_VERBOSE2 (client_verbose > 4)
#define CLIENT_VERBOSE3 (client_verbose > 5)

char* xed_upcase_buf(char* s);

/// Accepts K / M / G (or B) qualifiers ot multiply
int64_t xed_atoi_general(char* buf, int mul);
int64_t xed_atoi_hex(char* buf);

/// Converts "112233" in to 0x112233
uint64_t convert_ascii_hex_to_int(const char* s);


unsigned int xed_convert_ascii_to_hex(const char* src, 
                                      uint8_t* dst, 
                                      unsigned int max_bytes);


void xed_print_hex_lines(char* buf , const uint8_t* array, const int length); // breaks lines at 16 bytes.
void xed_print_hex_line(char* buf, const uint8_t* array, const int length);  // no endl

void xedex_derror(const char* s);
void xedex_dwarn(const char* s);

//////////////////////////////////////////////////////////////////////


typedef struct {
    xed_state_t dstate;
    int ninst;
    int decode_only;
} xed_decode_file_info_t;

void xed_decode_file_info_init(xed_decode_file_info_t* p,
                               const xed_state_t* arg_dstate,
                               int anrg_ninst,
                               int arg_decode_only);

void xed_map_region(const char* path,
                    void** start,
                    unsigned int* length);

void
xed_disas_test(const xed_state_t* dstate,
               unsigned char* s, // start of image
               unsigned char* a, // start of instructions to decode region
               unsigned char* q, // end of region
               int ninst,
               uint64_t runtime_vaddr,
               int decode_only); // where this region would live at runtime


// returns 1 on success, 0 on failure
uint_t disas_decode_binary(const xed_state_t* dstate,
                           const uint8_t* hex_decode_text,
                           const unsigned int bytes,
                           xed_decoded_inst_t* xedd);

// returns encode length on success, 0 on failure
uint_t disas_decode_encode_binary(const xed_state_t* dstate,
                                  const uint8_t* decode_text_binary,
                                  const unsigned int bytes,
                                  xed_decoded_inst_t* xedd);


void xed_print_decode_stats();

int
fn_disassemble_xed(xed_syntax_enum_t syntax,
                   char* buf,
                   int buflen,
                   xed_decoded_inst_t* xedd,
                   uint64_t runtime_instruction_address);

void disassemble(char* buf,
                 int buflen,
                 xed_decoded_inst_t* xedd,
                 uint64_t runtime_instruction_address);

uint64_t  get_time();

// 64b version missing on some MS compilers so I wrap it for portability.
// This function is rather limited and only handles base 10 and base 16.
int64_t xed_strtoll(const char* buf, int base);

#endif // file
//Local Variables:
//pref: "xed-examples-util.c"
//End:
