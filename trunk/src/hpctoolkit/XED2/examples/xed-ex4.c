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
/// @file xed-ex4.c
/// @author Mark Charney   <mark.charney@intel.com>

// decoder example. This is the "C" version of xed-ex2.cpp (which is C++).

#include "xed-interface.h"
#include "xed-examples-util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strcmp
#include <assert.h> 

int main(int argc, char** argv);

int 
main(int argc, char** argv)
{
    xed_error_enum_t xed_error;

    bool long_mode = false;
    xed_state_t dstate;
    unsigned int first_argv;
    unsigned int bytes = 0;
    unsigned char itext[XED_MAX_INSTRUCTION_BYTES];
    int i;
    unsigned int u;
    xed_decoded_inst_t xedd;
#define BUFLEN  1000
    char buffer[BUFLEN];
    bool ok;
    xed_syntax_enum_t syntax;

    xed_tables_init();
    xed_state_zero(&dstate);
    xed_set_verbosity( 99 );

    if (argc > 2 && strcmp(argv[1], "-64") == 0) 
        long_mode = true;

    if (long_mode) {
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
    for( i=first_argv ;i < argc; i++)    {
        uint8_t x = (uint8_t)(xed_atoi_hex(argv[i]));
        assert(bytes < XED_MAX_INSTRUCTION_BYTES);
        itext[bytes++] = x;
    }
    if (bytes == 0)    {
        fprintf(stderr, "Must supply some hex bytes\n");
        exit(1);
    }

    printf("PARSING BYTES: ");
    for( u=0;u<bytes; u++) 
        printf("%02x ", STATIC_CAST(unsigned int,itext[u]));
    printf("\n");

    xed_error = xed_decode(&xedd, 
                           REINTERPRET_CAST(const uint8_t*,itext),
                           bytes);
    switch(xed_error)
    {
      case XED_ERROR_NONE:
        break;
      case XED_ERROR_BUFFER_TOO_SHORT:
        fprintf(stderr,"Not enough bytes provided\n");
        exit(1);
      case XED_ERROR_GENERAL_ERROR:
        fprintf(stderr,"Could not decode given input.\n");
        exit(1);
      default:
        fprintf(stderr,"Unhandled error code %s\n", xed_error_enum_t2str(xed_error));
        exit(1);
    }
        
    //memset(buffer,0,BUFLEN);
    xed_decoded_inst_dump(&xedd,buffer, BUFLEN);
    printf("%s\n",buffer);


    for(syntax=  XED_SYNTAX_XED; syntax < XED_SYNTAX_LAST; syntax++)    {
        ok = xed_format(syntax, &xedd, buffer, BUFLEN, 0);
        if (ok)
            printf("%s syntax: %s\n", xed_syntax_enum_t2str(syntax), buffer);
        else
            printf("Error disassembling %s syntax\n", xed_syntax_enum_t2str(syntax));
    }
    return 0;
}
