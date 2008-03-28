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
/// @file xed.cpp
/// @author Mark Charney   <mark.charney@intel.com>


////////////////////////////////////////////////////////////////////////////
extern "C" {
#include "xed-interface.h"
#include "xed-immdis.h"
#include "xed-portability.h"
#include "xed-examples-util.h"
//void xed_decode_traverse_dump_profile();
}

#include "xed-disas-elf.H"
#include "xed-disas-macho.H"
#include "xed-disas-pecoff.H"
#include "xed-disas-raw.H"
#include "xed-enc-lang.H"

#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;


int main(int argc, char** argv);

////////////////////////////////////////////////////////////////////////////

static uint_t disas_decode(const xed_state_t* dstate,
                           const char* decode_text,
                           xed_decoded_inst_t* xedd) {
    uint8_t hex_decode_text[XED_MAX_INSTRUCTION_BYTES];
    uint_t bytes = xed_convert_ascii_to_hex(decode_text, hex_decode_text,XED_MAX_INSTRUCTION_BYTES);
    return  disas_decode_binary(dstate, hex_decode_text, bytes, xedd);
}


static unsigned int disas_decode_encode(const xed_state_t* dstate,
                                        const char* decode_text,
                                        xed_decoded_inst_t* xedd) {
    uint8_t hex_decode_text[XED_MAX_INSTRUCTION_BYTES];
    uint_t bytes = xed_convert_ascii_to_hex(decode_text,
                                            hex_decode_text, 
                                            XED_MAX_INSTRUCTION_BYTES);
    return disas_decode_encode_binary(dstate, hex_decode_text, bytes, xedd);
}

static unsigned int disas_encode(const xed_state_t* dstate,
                                 const char* encode_text) {
    unsigned int olen=0;
    ascii_encode_request_t areq;
    areq.dstate = *dstate;
    areq.command = encode_text;
    xed_encoder_request_t req = parse_encode_request(areq);
    char buf[5000];
    xed_encode_request_print(&req, buf, 5000);
    printf("Request: %s", buf);

    
    //for(uint_t i=0;i< xed_encoder_request_operand_order_entries(&req);i++)
    //   printf("REQUEST OPERAND ORDER ARRAY %d %s\n", 
    //           i, xed_operand_enum_t2str( xed_encoder_request_get_operand_order(&req,i) ) );
    uint8_t array[XED_MAX_INSTRUCTION_BYTES];
    unsigned int ilen = XED_MAX_INSTRUCTION_BYTES;
    xed_error_enum_t r = xed_encode(&req, array, ilen, &olen);
    if (r != XED_ERROR_NONE)     {
        printf("Could not encode: %s\n", encode_text);
        printf("Error code was: %s\n", xed_error_enum_t2str(r));
        xedex_derror("Dieing");
    }
    else if (CLIENT_VERBOSE)   {
        char buf2[100];
        xed_print_hex_line(buf2,array, olen);
        printf("Encodable! %s\n", buf2);
    }
    return olen;
}

static void no_comments(char* buf) {
    size_t len = strlen(buf);
    for(size_t i=0;i<len;i++) {
        if (buf[i] == ';' || buf[i] == '#') {
            buf[i]= 0; // stomp on it
            return;
        }
    }
}


#include <fstream>
static void xed_assemble(const xed_state_t* dstate,
                         const char* encode_file_name) {
    ifstream infile(encode_file_name);
    if (!infile) {
        printf("Could not open %s\n", encode_file_name);
        xedex_derror("Dieing");
    }
    char buf[1024];
    while(infile.getline(buf,sizeof(buf))) {
        printf("; %s\n",buf);
        no_comments(buf);
        if (strlen(buf) == 0)
            continue;
        unsigned int olen=0;
        ascii_encode_request_t areq;
        areq.dstate = *dstate;
        areq.command = buf;
        xed_encoder_request_t req = parse_encode_request(areq);
 
        uint8_t array[XED_MAX_INSTRUCTION_BYTES];
        unsigned int ilen = XED_MAX_INSTRUCTION_BYTES;
        xed_error_enum_t r = xed_encode(&req, array, ilen, &olen);
        if (r != XED_ERROR_NONE)     {
            printf("Could not encode: %s\n", buf);
            printf("Error code was: %s\n", xed_error_enum_t2str(r));
            xedex_derror("Dieing");
        }
        printf("      .byte ");
        for(unsigned int i=0;i<olen;i++) {
            if (i > 0)
                printf(", ");
            printf("0x%02x",array[i]);
        }
        printf("\n");
    }
}

static void usage(char* prog) {
    unsigned int i;
    static const char* usage_msg[] = {
      "One of the following is required:",
      "\t-i input_file             (decode file)",
      "\t-ir raw_input_file        (decode a raw unformatted binary file)",
      "\t-ide input_file           (decode/encode file)",
      "\t-d hex-string             (decode one instruction, must be last)",
      "\t-e instruction            (encode, must be last)",
      "\t-ie file-to-assemble      (assemble the contents of the file)",
      "\t-de hex-string            (decode-then-encode, must be last)",
      " ",
      "Optional arguments:",
      "\t-v verbosity  (0=quiet, 1=errors, 2=useful-info, 3=trace, 5=very verbose)",
      "\t-xv xed-engine-verbosity  (0...99)",
      "\t-s target section for file disassembly (PECOFF and ELF formats only)",

      "\t-n number-of-instructions-to-decode (default 100M, accepts K/M/G qualifiers)",
      "\t-I            (Intel syntax for disassembly)",
      "\t-A            (ATT SYSV syntax for disassembly)",
      "\t-16           (for LEGACY_16 mode)",
      "\t-32           (for LEGACY_32 mode, default)",
      "\t-64           (for LONG_64 mode w/64b addressing)",
      "\t-a32          (32b addressing, default, not in LONG_64 mode)",
      "\t-a16          (16b addressing, not in LONG_64 mode)",
      "\t-s32          (32b stack addressing, default, not in LONG_64 mode)",
      "\t-s16          (16b stack addressing, not in LONG_64 mode)",
//    " ",
//    "\t-nofeedback   (turn off feedback for builds that actually use feedback)",
      " ",
      0
    };      

    cerr << "Usage: " << prog << " [options]" << endl;
    for(i=0; usage_msg[i]  ; i++)
        cerr << "\t" << usage_msg[i] << endl;
}
 
#if 1
void test_immdis() {
    char buf[1000];
    int64_t sv;
    xed_immdis_t i;
    uint64_t v = 0x11223344;
    uint32_t uv =0;

    xed_immdis_init(&i,8);
    xed_immdis_add_shortest_width_signed(&i,v,4);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);

    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_unsigned(&i,uv,5);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);

    xed_immdis_zero(&i);

    sv = 0x90;
    xed_immdis_add_shortest_width_signed(&i,sv,5);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    xed_immdis_zero(&i);

    sv = -128;
    xed_immdis_add_shortest_width_signed(&i,sv,5);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    xed_immdis_zero(&i);

    sv = -127;
    xed_immdis_add_shortest_width_signed(&i,sv,5);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    xed_immdis_zero(&i);
    uv = 0x80000000;
    xed_immdis_add_shortest_width_unsigned(&i,uv,5);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
#if !defined(_MSC_VER) // MSVS6 VC98 chokes on the LL. Just punt on MS compilers
    xed_immdis_zero(&i);

    sv = 0xffffffff81223344LL;
    xed_immdis_add_shortest_width_signed(&i,sv,4);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
        v = 0x1122334455667788ULL;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,sv,4);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    #endif

    v = 0x11223344;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,4);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    v = 0x112233;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,4);

    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    v = 0x1122;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,4);

    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    v = 0x11;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,4);

    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    

    v = 0x1122;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,3);

    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    v = 0xffff;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,2);

    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    v = 0xff00;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,2);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    

    v = 0xff77;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,7);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    v = 0xff7777;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,5);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    v = 0xff8000;
    xed_immdis_zero(&i);
    xed_immdis_add_shortest_width_signed(&i,v,7);
    xed_immdis_print(&i,buf,1000);
    printf("%s\n",buf);
    
    exit(1);
}
#endif


void remove_spaces(string& s) {
    string::size_type i,p=0,len = s.size();
    for(i=0;i<len;i++)
        if (s[i] != ' ')
            s[p++]=s[i];
    s = s.substr(0,p);
}

void test_argc(int i, int argc) {
    if (i+1 >= argc)
        xedex_derror("Need more arguments. Use \"xed -help\" for usage.");
}

int main(int argc,     char** argv) {
    bool sixty_four_bit = false;
    bool decode_only = true;
    char* input_file_name = 0;
    string decode_text("");
    string encode_text("");
    xed_state_t dstate;
    bool encode = false;
    unsigned int ninst = 100*1000*1000; // FIXME: should use maxint...
    bool decode_encode = false;
    int i,j;
    unsigned int loop_decode = 0;
    bool decode_raw = false;
    bool assemble  = false;
    char* target_section = 0;
    xed_state_init(&dstate,
                   XED_MACHINE_MODE_LEGACY_32,
                   XED_ADDRESS_WIDTH_32b, 
                   XED_ADDRESS_WIDTH_32b);


    client_verbose = 3;
    xed_set_verbosity( client_verbose );
    for( i=1;i<argc;i++)    {
        if (strcmp(argv[i],"-d")==0)         {
            test_argc(i,argc);
            for(j=i+1; j< argc;j++) 
                decode_text = decode_text + argv[j];
            break; // leave the i=1...argc loop
        }
        else if (strcmp(argv[i],"-de")==0)        {
            test_argc(i,argc);
            decode_encode = true;
            for(j=i+1; j< argc;j++) 
                decode_text = decode_text + argv[j];
            break; // leave the i=1...argc loop
        }
        else if (strcmp(argv[i],"-i")==0)        {
            test_argc(i,argc);
            input_file_name = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i],"-s")==0)        {
            test_argc(i,argc);
            target_section = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i],"-ir")==0)        {
            test_argc(i,argc);
            input_file_name = argv[i+1];
            decode_raw = true;
            i++;
        }
        else if (strcmp(argv[i],"-ie")==0)        {
            test_argc(i,argc);
            input_file_name = argv[i+1];
            assemble = true;
            i++;
        }
        else if (strcmp(argv[i],"-ide")==0)        {
            test_argc(i,argc);
            input_file_name = argv[i+1];
            decode_only = false;
            i++;
        }
        else if (strcmp(argv[i],"-n") ==0)         {
            test_argc(i,argc);
            ninst = STATIC_CAST(unsigned int,xed_atoi_general(argv[i+1],1000));
            i++;
        }
        else if (strcmp(argv[i],"-loop") ==0)         {
            test_argc(i,argc);
            loop_decode = STATIC_CAST(unsigned int,xed_atoi_general(argv[i+1],1000));
            i++;
        }
        else if (strcmp(argv[i],"-v") ==0)         {
            test_argc(i,argc);
            client_verbose = STATIC_CAST(int,xed_atoi_general(argv[i+1],1000));
            xed_set_verbosity(client_verbose);

            i++;
        }
        else if (strcmp(argv[i],"-xv") ==0)        {
            test_argc(i,argc);
            unsigned int xed_engine_verbose = STATIC_CAST(unsigned int,xed_atoi_general(argv[i+1],1000));
            xed_set_verbosity(xed_engine_verbose);
            i++;
        }
        else if (strcmp(argv[i],"-A")==0)        {
            att_syntax = true;
            xed_syntax = intel_syntax = false;
        }
        else if (strcmp(argv[i],"-I")==0)        {
            intel_syntax = true;
            xed_syntax = att_syntax = false;
        }
        else if (strcmp(argv[i],"-X") == 0)  {  // undocumented
            xed_syntax = true;
            intel_syntax = true;
            att_syntax = true;
        }
        else if (strcmp(argv[i],"-16")==0)         {
            sixty_four_bit = false;
            dstate.mmode = XED_MACHINE_MODE_LEGACY_16;
        }
        else if (strcmp(argv[i],"-32")==0) { // default
            sixty_four_bit = false;
            dstate.mmode = XED_MACHINE_MODE_LEGACY_32;
        }
        else if (strcmp(argv[i],"-64")==0)         {
            sixty_four_bit = true;
            dstate.mmode = XED_MACHINE_MODE_LONG_64;
            dstate.addr_width = XED_ADDRESS_WIDTH_64b;
        }
        else if (strcmp(argv[i],"-a32")==0) 
            dstate.addr_width = XED_ADDRESS_WIDTH_32b;
        else if (strcmp(argv[i],"-a16")==0) 
            dstate.addr_width = XED_ADDRESS_WIDTH_16b;
        else if (strcmp(argv[i],"-s32")==0) 
            dstate.stack_addr_width = XED_ADDRESS_WIDTH_32b;
        else if (strcmp(argv[i],"-s16")==0) 
            dstate.stack_addr_width = XED_ADDRESS_WIDTH_16b;
        else if (strcmp(argv[i],"-ti") ==0)        {
            client_verbose = 5;
            xed_set_verbosity(5);
            test_immdis();
            exit(1);
        }
        else if (strcmp(argv[i],"-e") ==0)         {
            encode = true;
            test_argc(i,argc);
            // merge the rest of the args in to the encode_text string.
            for( j = i+1; j< argc; j++ ) 
                encode_text = encode_text + argv[j] + " ";
            break;  // leave the loop
        }
        else          {
            usage(argv[0]);
            exit(1);
        }
    }
    if (!encode)     {
        if (input_file_name == 0 && decode_text == "")        {
            cerr << "ERROR: required argument(s) were missing" << endl;
            usage(argv[0]);
            exit(1);
        }
    }
    if (CLIENT_VERBOSE2)
        printf("Initializing XED tables...\n");
    xed_tables_init();
    if (CLIENT_VERBOSE2)
        printf("Done initialing XED tables.\n");

    printf("#XED version: [%s]\n", xed_get_version());
    xed_decoded_inst_t xedd;
    xed_decoded_inst_zero_set_mode(&xedd, &dstate);

    uint_t retval_okay = 1;
    unsigned int obytes=0;
    if (assemble) {
        xed_assemble(&dstate, input_file_name);
    }
    else if (decode_encode)     {
        obytes = disas_decode_encode(&dstate, decode_text.c_str(), &xedd);
        retval_okay = (obytes != 0) ? 1 : 0;
    }
    else if (encode) 
        obytes = disas_encode(&dstate, encode_text.c_str());
    else if (decode_text != "") {
        if (loop_decode) {
            unsigned int k;
            for(k=0;k<loop_decode;k++) {
                retval_okay = disas_decode(&dstate, decode_text.c_str(), &xedd);
                xed_decoded_inst_zero_set_mode(&xedd, &dstate);
                //xed_decode_traverse_dump_profile();
            }
        }
        else {
            remove_spaces(decode_text);
            const char* p = decode_text.c_str();
            int remaining = static_cast<int>(decode_text.size() / 2); // 2 bytes per nibble
            do {
                retval_okay = disas_decode(&dstate, p, &xedd);
                unsigned int  len = xed_decoded_inst_get_length(&xedd);
                p+=len*2;
                remaining -= len;
            }
            while(retval_okay && remaining > 0);
        }
    }
    else if (decode_raw) {
        xed_disas_raw(input_file_name, &dstate, ninst, sixty_four_bit, decode_only);
    }
    else     {

#if defined(XED_MAC_OSX_FILE_READER)
        xed_disas_macho(input_file_name, &dstate, ninst, sixty_four_bit, decode_only);
#elif defined(XED_ELF_READER)
        xed_disas_elf(input_file_name, &dstate, ninst, sixty_four_bit, decode_only, 
                      target_section);
#elif defined(XED_PECOFF_FILE_READER)
        xed_disas_pecoff(input_file_name, &dstate, ninst, sixty_four_bit, decode_only, 
                         target_section);
#else
        xedex_derror("No PECOFF, ELF or MACHO support compiled in");
#endif
    }
    //xed_decode_traverse_dump_profile();

    if (retval_okay==0) 
        exit(1);
    return 0;
    (void) obytes;
}
 

////////////////////////////////////////////////////////////////////////////
