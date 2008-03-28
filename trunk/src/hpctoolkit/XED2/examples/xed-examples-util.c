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
/// @file xed-examples-util.cpp
/// @author Mark Charney   <mark.charney@intel.com>

#include "xed-interface.h"
#include "xed-examples-util.h"
#include <string.h> //strlen, memcmp, memset
#if defined(__APPLE__) || defined(__linux__) || defined(__linux)
# include <unistd.h>
# include <sys/mman.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#endif
#include <ctype.h>
#include <stdlib.h>
#include "xed-portability.h"
#include "xed-util.h"


void xed_decode_file_info_init(xed_decode_file_info_t* p,
                               const xed_state_t* arg_dstate,
                               int arg_ninst,
                               int arg_decode_only) {
    p->dstate = *arg_dstate;
    p->ninst = arg_ninst;
    p->decode_only = arg_decode_only;
}

typedef struct {
    uint64_t  total_time ;
    uint64_t  total_insts ;
    uint64_t  total_ilen ;
    uint64_t  total_olen ;
    uint64_t  total_shorter ;
    uint64_t  total_longer ;
    uint64_t  bad_times ;
    uint64_t  reset_counter;
} xed_decode_stats_t;

void xed_decode_stats_reset(xed_decode_stats_t* p, uint64_t t1, uint64_t t2) {
    if (t2 > t1) 
        p->total_time += (t2-t1);
    else
        p->bad_times++;
    p->total_insts++;
    p->reset_counter++;
    if (p->reset_counter == 50) {
        if (CLIENT_VERBOSE1) 
            printf("\n\nRESETTING STATS\n\n");
        // to ignore startup transients paging everything in.
        p->total_insts=0;
        p->total_time=0;
    }
}

void xed_decode_stats_zero(xed_decode_stats_t* p)    {
    p->total_time = 0;
    p->total_insts = 0;
    p->total_ilen = 0;
    p->total_olen = 0;
    p->total_shorter = 0;
    p->total_longer = 0;
    p->bad_times = 0;
    p->reset_counter = 0;
}

static xed_decode_stats_t xed_stats;
int xed_syntax = 0;
int intel_syntax = 1;
int att_syntax = 0;
int client_verbose=0; 

////////////////////////////////////////////////////////////////////////////

static char xed_toupper(char c) {
    if (c >= 'a' && c <= 'z')
        return c-'a'+'A';
    return c;
}

char* xed_upcase_buf(char* s) {
    uint_t len = STATIC_CAST(uint_t,strlen(s));
    uint_t i;
    for(i=0 ; i < len ; i++ ) 
        s[i] = STATIC_CAST(char,xed_toupper(s[i]));
    return s;
}

static uint8_t convert_nibble(uint8_t x) {
    // convert ascii nibble to hex
    uint8_t rv = 0;
    if (x >= '0' && x <= '9') 
        rv = x - '0';
    else if (x >= 'A' && x <= 'F') 
        rv = x - 'A' + 10;
    else if (x >= 'a' && x <= 'f') 
        rv = x - 'a' + 10;
    else    {
        printf("Error converting hex digit. Nibble value 0x%x\n", x);
        exit(1);
    }
    return rv;
}


int64_t xed_atoi_hex(char* buf) {
    int64_t o=0;
    uint_t i;
    uint_t len = STATIC_CAST(uint_t,strlen(buf));
    for(i=0; i<len ; i++) 
        o = o*16 + convert_nibble(buf[i]);
    return o;
}

int64_t xed_atoi_general(char* buf, int mul) {
    /*      mul should be 1000 or 1024     */
    char* q;
    int64_t b;

    char* p = buf;
    while(*p && isspace(*p))
    {
        p++;
    }
    // exclude hex; octal works just fine
    q = p;
    if (*q == '-' || *q == '+')
    {
        q++;
    }
    if (*q=='0' && (q[1]=='x' || q[1]=='X'))
    {
        return xed_strtoll(buf,0);
    }

    b = xed_strtoll(buf,0);
    if (p)
    {
        while(*p && (*p == '-' || *p == '+'))
        {
            p++;
        }
        while(*p && isdigit(*p))
        {
            p++;
        }

        if (*p != 0)
        {
            if (*p == 'k' || *p == 'K')
            {
                b = b * mul;
            }
            else if (*p == 'm' || *p == 'M')
            {
                b = b * mul * mul;
            }
            else if (*p == 'g' || *p == 'G' || *p == 'b' || *p == 'B')
            {
                b = b * mul * mul * mul;
            }
        }
    }
    return b;
}

static char nibble_to_ascii_hex(uint8_t i) {
    if (i<10) return i+'0';
    if (i<16) return i-10+'A';
    return '?';
}
void xed_print_hex_line(char* buf, const uint8_t* array, const int length) {
  int n = length;
  int i=0;
  if (length == 0)
      n = XED_MAX_INSTRUCTION_BYTES;
  for( i=0 ; i< n; i++)     {
      buf[2*i+0] = nibble_to_ascii_hex(array[i]>>4);
      buf[2*i+1] = nibble_to_ascii_hex(array[i]&0xF);
  }
  buf[2*i]=0;
}

void xed_print_hex_lines(char* buf, const uint8_t* array, const int length) {
  int n = length;
  int i=0,j=0;
  char* b = buf;
  for( i=0 ; i< n; i++)     {
      *b++ = nibble_to_ascii_hex(array[i]>>4);
      *b++ = nibble_to_ascii_hex(array[i]&0xF);
      j++;
      if (j == 16) {
	j = 0;
        *b++ = '\n';
      }
  }
  *b++ = '\n';
  *b = '0';
}




void xedex_derror(const char* s) {
    printf("[XED CLIENT ERROR] %s\n",s);
    exit(1);
}

void xedex_dwarn(const char* s) {
    printf("[XED CLIENT WARNING] %s\n",s);
}


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////




void xed_print_decode_stats()
{
    double cpi;
    int64_t growth;
    printf("#Total decode cycles:        " XED_FMT_LU "\n", xed_stats.total_time);
    printf("#Total instructions decoded: " XED_FMT_LU "\n", xed_stats.total_insts);
#if defined(_MSC_VER) 
#  if  _MSC_VER==1200
#    define XCAST(x) STATIC_CAST(int64_t,x)
#  else 
#    define XCAST(x) (x)
#  endif
#else
# define XCAST(x) (x)
#endif
    cpi  =  1.0 * XCAST(xed_stats.total_time) / XCAST(xed_stats.total_insts);
    printf("#Total cycles/instructions decoded: %f\n" , cpi);

    printf("#Bad times: " XED_FMT_LU "\n", xed_stats.bad_times);
    printf("#Total input length bytes: " XED_FMT_LU "\n", xed_stats.total_ilen );
    printf("#Total output length bytes: " XED_FMT_LU "\n", xed_stats.total_olen );
    printf("#Growth bytes: " XED_FMT_LU "\n", xed_stats.total_longer );
    printf("#Shrinkage bytes: " XED_FMT_LU "\n", xed_stats.total_shorter );
    growth = xed_stats.total_olen - xed_stats.total_ilen;
    printf("#Growth/Shrinkage  bytes: " XED_FMT_LD "\n", growth );
    if (xed_stats.total_ilen)    {
        double pct_growth = 100.0 * growth / (double) XCAST(xed_stats.total_ilen);
        printf("#Code size growth percent: %f\n", pct_growth);
    }
}


void
xed_map_region(const char* path,
               void** start,
               unsigned int* length)
{
#if defined(_WIN32) 
    FILE* f;
    size_t t,ilen;
    uint8_t* p;
#if defined(XED_MSVC8)
    errno_t err;
    fprintf(stderr,"#Opening %s\n", path);
    err = fopen_s(&f,path,"rb");
#else
    int err=0;
    fprintf(stderr,"#Opening %s\n", path);
    f = fopen(path,"rb");
    err = (f==0);
#endif
    if (err != 0) {
        fprintf(stderr,"ERROR: Could not open %s\n", path);
        exit(1);
    }
    err =  fseek(f, 0, SEEK_END);
    if (err != 0) {
        fprintf(stderr,"ERROR: Could not fseek %s\n", path);
        exit(1);
    }
    ilen = ftell(f);
    fprintf(stderr,"#Trying to read " XED_FMT_SIZET "\n", ilen);
    p = (uint8_t*)malloc(ilen);
    t=0;
    err = fseek(f,0, SEEK_SET);
    if (err != 0) {
        fprintf(stderr,"ERROR: Could not fseek to start of file %s\n", path);
        exit(1);
    }
    
    while(t < ilen) {
        size_t n;
        if (feof(f)) {
            fprintf(stderr, "#Read EOF. Stopping.\n");
            break;
        }
        n = fread(p+t, 1, ilen-t,f);
        t = t+n;
        fprintf(stderr,"#Read " XED_FMT_SIZET " of %d bytes\n", t, ilen);
        if (ferror(f)) {
            fprintf(stderr, "Error in file read. Stopping.\n");
            break;
        }
    }
    fclose(f);
    *start = p;
    *length = (unsigned int)ilen;
    
#else 
    int ilen,fd;
    fd = open(path, O_RDONLY);
    if (fd == -1)   {
        printf("Could not open file: %s\n" , path);
        exit(1);
    }
    ilen = lseek(fd, 0, SEEK_END); // find the size.
    if (ilen == -1)
        xedex_derror("lseek failed");
    else 
        *length = (unsigned int) ilen;

    lseek(fd, 0, SEEK_SET); // go to the beginning
    *start = mmap(0,
                  *length,
                  PROT_READ|PROT_WRITE,
                  MAP_PRIVATE,
                  fd,
                  0);
    if (*start == (void*) -1)
        xedex_derror("could not map region");
#endif
    if (CLIENT_VERBOSE1)
        printf("Mapped " XED_FMT_U " bytes!\n", *length);
}


////////////////////////////////////////////////////////////////////////////

static int all_zeros(uint8_t* p, unsigned int len) {
    unsigned int i;
    for( i=0;i<len;i++) 
        if (p[i]) 
            return 0;
    return 1;
}

int
fn_disassemble_xed(xed_syntax_enum_t syntax,
                   char* buf,
                   int buflen,
                   xed_decoded_inst_t* xedd, 
                   uint64_t runtime_instruction_address) {   
#define BUFLEN 1000
    char buffer[BUFLEN];
    int blen= buflen;
    //memset(buffer,0,BUFLEN);
    int ok = xed_format(syntax, xedd, buffer, BUFLEN,runtime_instruction_address);
#undef BUFLEN
    if (ok)
        blen = xed_strncpy(buf,buffer,buflen);
    else    {
        blen = buflen;
        blen = xed_strncpy(buf,"Error disassembling ",blen);
        blen = xed_strncat(buf, xed_syntax_enum_t2str(syntax),blen);
        blen = xed_strncat(buf," syntax.",blen);
    }
    return blen;
}


void disassemble(char* buf,
                 int buflen,
                 xed_decoded_inst_t* xedd,
                 uint64_t runtime_instruction_address)
{
    int blen = buflen;
    if (xed_syntax)    {
        blen = fn_disassemble_xed(XED_SYNTAX_XED, buf, blen, xedd, runtime_instruction_address);
        if (att_syntax || intel_syntax)
            blen = xed_strncat(buf, " | ",blen);
    }
    if (att_syntax)    {
        char* xbuf = buf+strlen(buf);
        blen = fn_disassemble_xed(XED_SYNTAX_ATT, xbuf, blen, xedd, runtime_instruction_address);
        if (intel_syntax)
            blen = xed_strncat(buf, " | ",blen);
    }
    if (intel_syntax) {
        char* ybuf = buf+strlen(buf);
        blen = fn_disassemble_xed(XED_SYNTAX_INTEL, ybuf, blen, xedd, runtime_instruction_address);
    }
}

void xed_decode_error(uint64_t offset, const uint8_t* ptr, xed_error_enum_t xed_error) {
    char buf[200];
    printf("ERROR: %s Could not decode at offset 0x" XED_FMT_LX ": [", 
           xed_error_enum_t2str(xed_error),
           offset);
    xed_print_hex_line(buf, ptr, 15);
    printf("%s]\n",buf);
}

///////////////////////////////////////////////////////////////////////////
// 2007-07-02

static void print_hex_line(const uint8_t* p, unsigned int length) {
        char buf[128];
        unsigned int lim = 128;
        if (length < lim)
            lim = length;
        xed_print_hex_line(buf,p, lim); 
        printf("%s\n", buf);
}

uint_t disas_decode_binary(const xed_state_t* dstate,
                           const uint8_t* hex_decode_text,
                           const unsigned int bytes,
                           xed_decoded_inst_t* xedd) {
    uint64_t t1,t2;
    xed_error_enum_t xed_error;
    bool okay;

    if (CLIENT_VERBOSE) {
        print_hex_line(hex_decode_text, bytes);
    }
    t1 = get_time();
    xed_error = xed_decode(xedd, hex_decode_text, bytes);
    t2 = get_time();
    okay = (xed_error == XED_ERROR_NONE);
    if (CLIENT_VERBOSE3) {
        uint64_t delta = t2-t1;
        printf("Decode time = " XED_FMT_LU "\n", delta);
    }
    if (okay)     {
#define TBUF_LEN (1024*3)
        if (CLIENT_VERBOSE1) {
            char tbuf[TBUF_LEN];
            xed_decoded_inst_dump(xedd,tbuf,TBUF_LEN);
            printf("%s\n",tbuf);
        }
        if (CLIENT_VERBOSE) {
            char buf[TBUF_LEN];
            if (xed_decoded_inst_valid(xedd)) {
                printf( "ICLASS: %s   CATEGORY: %s   EXTENSION: %s\n", 
                        xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(xedd)),
                        xed_category_enum_t2str(xed_decoded_inst_get_category(xedd)),
                        xed_extension_enum_t2str(xed_decoded_inst_get_extension(xedd)));
            }
            memset(buf,0,TBUF_LEN);
            disassemble(buf,TBUF_LEN, xedd,0);
            printf("SHORT: %s\n", buf);
        }
        return 1;
    }
    else {
        xed_decode_error(0, hex_decode_text, xed_error);
        return 0;
    }
    (void) dstate; // pacify compiler
}

uint_t disas_decode_encode_binary(const xed_state_t* dstate,
                                  const uint8_t* decode_text_binary,
                                  const unsigned int bytes,
                                  xed_decoded_inst_t* xedd)   {
    // decode then encode
    unsigned int retval_olen = 0;
    // decode it...
    bool decode_okay =  disas_decode_binary(dstate, decode_text_binary, bytes, xedd);
    if (decode_okay)     {
        xed_error_enum_t encode_okay;
        unsigned int enc_olen, ilen = XED_MAX_INSTRUCTION_BYTES;
        uint8_t array[XED_MAX_INSTRUCTION_BYTES];
        xed_encoder_request_t* enc_req = xedd;  // they are basically the same now
        // convert decode structure to proper encode structure
        xed_encoder_request_init_from_decode(xedd);
        
        // encode it again...
        encode_okay =  xed_encode(enc_req, array, ilen, &enc_olen);
        if (encode_okay != XED_ERROR_NONE) {
            if (CLIENT_VERBOSE) {
                char buf[5000];
                char buf2[5000];
                int blen=5000;
                xed_encode_request_print(enc_req, buf, 5000);
                blen = xed_strncpy(buf2,"Could not re-encode: ", blen);
                blen = xed_strncat(buf2, buf, blen);
                blen = xed_strncat(buf2,"\nError code was: ",blen);
                blen = xed_strncat(buf2,xed_error_enum_t2str(encode_okay),blen);
                blen = xed_strncat(buf2, "\n",blen);
                xedex_dwarn(buf2);
            }
        }
        else         {
            retval_olen = enc_olen;
            // See if it matched the original...
            if (CLIENT_VERBOSE) {
                char buf[100];
                uint_t dec_length; 
                xed_print_hex_line(buf,array, enc_olen);
                printf("Encodable! %s\n",buf);
                dec_length = xed_decoded_inst_get_length(xedd);
                if ((enc_olen != dec_length || memcmp(decode_text_binary, array, enc_olen)  )) {
                    char buf2[5000];
                    char buf3[5000];
                    printf("Discrepenacy after re-encoding. dec_len= " XED_FMT_U " ", dec_length);
                    xed_print_hex_line(buf, decode_text_binary, dec_length);
                    printf("[%s] ", buf);
                    printf("enc_olen= " XED_FMT_U "", enc_olen);
                    xed_print_hex_line(buf, array, enc_olen);
                    printf(" [%s] ", buf);
                    printf("for instruction: ");
                    xed_decoded_inst_dump(xedd, buf3,5000);
                    printf("%s\n", buf3);
                    printf("vs Encode  request: ");
                    xed_encode_request_print(enc_req, buf2, 5000);
                    printf("%s\n", buf2);
                }
                else 
                    printf("Identical re-encoding\n");
            }
        }
    }
    return retval_olen;
}



///////////////////////////////////////////////////////////////////////////

void
xed_disas_test(const xed_state_t* dstate,
               unsigned char* s, // start of image
               unsigned char* a, // start of instructions to decode region
               unsigned char* q, // end of region
               int ninst,
               uint64_t runtime_vaddr,
               int decode_only) // where this region would live at runtime
{
    static int first = 1;
    uint64_t errors = 0;
    unsigned int m;
    unsigned char* z;
    unsigned int len;
    
    int skipping;
    int last_all_zeros;
    unsigned int i;

    int okay;
    xed_decoded_inst_t xedd;
    unsigned int length;

    uint64_t runtime_instruction_address;

    if (first) {
        xed_decode_stats_zero(&xed_stats);
        first = 0;
    }

    // print some stuff in hex from the text segment.
    // unsigned char* p = a;
    //uint64_t tlen = q-p;
    //if (tlen > 1024 ) 
    //    tlen = 1024;
    //xed_print_hex_line(p,tlen);
  
    m = ninst; // number of things to decode
    z = a;
    len = 15; //FIXME
  
    // for skipping long strings of zeros
    skipping = 0;
    last_all_zeros = 0;
    for( i=0; i<m;i++) 
    {
        if (z >= q) {
            printf("# end of text section.\n");
            break;
        }
        if (CLIENT_VERBOSE3) {
            printf("\n==============================================\n");
            printf("Decoding instruction " XED_FMT_U "\n", i);
            printf("==============================================\n");
        }
    
        // if we get two full things of 0's in a row, start skipping.
        if (all_zeros((uint8_t*) z, 15)) 
        {
            if (skipping) {
                z = z + 15;
                continue;
            }
            else if (last_all_zeros) { 
                printf("...\n");
                z = z + 15;
                skipping = 1;
                continue;
            }
            else
                last_all_zeros = 1;
        }
        else
        {
            skipping = 0;
            last_all_zeros = 0;
        }

        runtime_instruction_address =  ((uint64_t)(z-a)) + runtime_vaddr;
         
        if (CLIENT_VERBOSE3) {
            char tbuf[200];
            printf("Runtime Address " XED_FMT_LX ,runtime_instruction_address);
            xed_print_hex_line(tbuf, (uint8_t*) z, 15);
            printf(" [%s]\n", tbuf);
        }
        okay = 0;
        xed_decoded_inst_zero_set_mode(&xedd, dstate);
        length = 0;
        if ( decode_only )
        {
            uint64_t t1 = get_time();
            uint64_t t2;
            
            xed_error_enum_t xed_error = xed_decode(&xedd, 
                                                    REINTERPRET_CAST(const uint8_t*,z),
                                                    len);
            t2 = get_time();
            okay = (xed_error == XED_ERROR_NONE);
            xed_decode_stats_reset(&xed_stats, t1, t2);
           
            length = xed_decoded_inst_get_length(&xedd);
            if (okay && length == 0) {
                printf("Zero length on decoded instruction!\n");
                xed_decode_error( z-a, z, xed_error);
                xedex_derror("Dieing");
            }
            xed_stats.total_ilen += length;

            if (okay)  {
                if (CLIENT_VERBOSE1) {
                    char tbuf[1024*3];
                    xed_decoded_inst_dump(&xedd,tbuf, 1024*3);
                    printf("%s\n",tbuf);
                }
                if (CLIENT_VERBOSE)  {
                    char buffer[200];
                    unsigned int dec_len;
                    unsigned int sp;
                    printf("XDIS " XED_FMT_LX ": ", runtime_instruction_address);
                    printf("%-8s ", xed_category_enum_t2str(xed_decoded_inst_get_category(&xedd)));
                    printf("%-4s ", xed_extension_enum_t2str(xed_decoded_inst_get_extension(&xedd)));
		    dec_len = xed_decoded_inst_get_length(&xedd);
                    xed_print_hex_line(buffer, (uint8_t*) z, dec_len);
                    printf("%s",buffer);
                    // pad out the instruction bytes
                    for ( sp=dec_len; sp < 12; sp++) {
                        printf("  ");
                    }
                    printf(" ");
		    memset(buffer,0,200);
                    disassemble(buffer,200, &xedd, runtime_instruction_address);
                    printf( "%s\n",buffer);
                }
            }
            else {
                errors++;
                xed_decode_error( z-a, z, xed_error);
                // just give a length of 1B to see if we can restart decode...
                length = 1;
            }
        }
        else
        {
            uint64_t t1 = get_time();
            uint64_t t2;
            unsigned int olen  = 0;
            olen  = disas_decode_encode_binary(dstate, 
                                                REINTERPRET_CAST(const uint8_t*,z),
                                                len,
                                                &xedd);
            t2=get_time();
            okay = (olen != 0);
            xed_decode_stats_reset(&xed_stats, t1, t2);
            if (!okay)  {
                errors++;
                printf("-- Could not decode/encode at offset: %d\n" ,(int)(z-a));
                // just give a length of 1B to see if we can restart decode...
                length = 1;
                //exit(1);
            }        
            else {
                length = xed_decoded_inst_get_length(&xedd);
                xed_stats.total_ilen += length;
                xed_stats.total_olen += olen;
                if (length > olen)
                    xed_stats.total_shorter += (length - olen);
                else
                    xed_stats.total_longer += (olen - length);
            }

        }

        z = z + length;
    }
    
    printf( "# Errors: " XED_FMT_LU "\n", errors);
    (void) s;
}
       
#if defined(__INTEL_COMPILER) && __INTEL_COMPILER >= 810 && !defined(_M_IA64)
#  include <ia32intrin.h>
#  pragma intrinsic(__rdtsc)
#endif
#if defined(_MSC_VER) && _MSC_VER >= 1400 && !defined(_M_IA64) /* MSVS8 and later */
#  include <intrin.h>
#  pragma intrinsic(__rdtsc)
#endif

uint64_t  get_time()
{
   uint64_t ticks;
   uint32_t lo,hi;
#if defined(__GNUC__)
# if defined(__i386__) || defined(i386) || defined(i686) || defined(__x86_64__)
   //asm volatile("rdtsc" : "=A" (ticks) );
   //asm volatile("rdtsc" : "=A" (ticks) :: "edx");
   asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
   ticks = hi;
   ticks <<=32;
   ticks |=lo;
#  define FOUND_RDTSC
# endif
#endif
#if defined(__INTEL_COMPILER) &&  __INTEL_COMPILER>=810 && !defined(_M_IA64)
   ticks = __rdtsc();
#  define FOUND_RDTSC
#endif
#if !defined(FOUND_RDTSC) && defined(_MSC_VER) && _MSC_VER >= 1400 && !defined(_M_IA64) /* MSVS7, 8 */
   ticks = __rdtsc();
#  define FOUND_RDTSC
#endif
#if !defined(FOUND_RDTSC)
   ticks = 0;
#endif
   return ticks;
   (void)hi; (void)lo;
}


uint8_t
convert_ascii_nibble(char c)
{
  if (c >= '0' && c <= '9') {
    return c-'0';
  }
  else if (c >= 'a' && c <= 'f') {
    return c-'a' + 10;
  }
  else if (c >= 'A' && c <= 'F') {
    return c-'A' + 10;
  }
  else {
      char buffer[200];
      char* x;
      xed_strcpy(buffer,"Invalid character in hex string: ");
      x= buffer+strlen(buffer);
      *x++ = c;
      *x++ = 0;
      xedex_derror(buffer);
      return 0;
  }
}



uint64_t convert_ascii_hex_to_int(const char* s) {
    uint64_t retval = 0;
    const char* p = s;
    while (*p) {
        retval  =  (retval << 4) + convert_ascii_nibble(*p);
        p++;
    }
    return retval;
}


uint8_t convert_ascii_nibbles(char c1, char c2) {
    uint8_t a = convert_ascii_nibble(c1) * 16 + convert_ascii_nibble(c2);
    return a;
}

unsigned int
xed_convert_ascii_to_hex(const char* src, uint8_t* dst, unsigned int max_bytes)
{
    unsigned int j;
    unsigned int p = 0;
    unsigned int i = 0;

    const unsigned int len = STATIC_CAST(unsigned int,strlen(src));
    if ((len & 1) != 0) 
        xedex_derror("test string was not an even number of nibbles");
    
    if (len > (max_bytes * 2) ) 
        xedex_derror("test string was too long");

    for( j=0;j<max_bytes;j++) 
        dst[j] = 0;

    for(;i<len/2;i++) {
        if (CLIENT_VERBOSE3) 
            printf("Converting %c & %c\n", src[p], src[p+1]);
        dst[i] = convert_ascii_nibbles(src[p], src[p+1]);
        p=p+2;
    }
    return i;
}

#if defined(_WIN32) && !defined(__GNUC__)
static int64_t
convert_base10(const char* buf)
{
    int64_t v = 0;
    int64_t sign = 1;
    int len = STATIC_CAST(int,strlen(buf));
    int i; 
    for(i=0;i<len;i++)
    {
        char c = buf[i];
        if (i == 0 && c == '-')
        {
            sign = -1;
        }
        else if (c >= '0' && c <= '9')
        {
            unsigned int digit = c - '0';
            v = v*10 + digit;
        }
        else
        {
            break;
        }
    }
    return v*sign;
}

static int64_t
convert_base16(const char* buf)
{
    int64_t v = 0;
    int len = STATIC_CAST(int,strlen(buf));
    int start =0 ;
    int i;
    if (len > 2 && buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X'))
    {
        start = 2;
    }
    for(i=start;i<len;i++)
    {
        char c = buf[i];
        if (c >= '0' && c <= '9')
        {
            unsigned int digit = c - '0';
            v = v*16 + digit;
        }
        else if (c >= 'A' && c <= 'F')
        {
            unsigned int digit = c - 'A' + 10;
            v = v*16 + digit;
        }
        else if (c >= 'a' && c <= 'f')
        {
            unsigned int digit = c - 'a' + 10;
            v = v*16 + digit;
        }
        else
        {
            break;
        }
    }
    return v;
}

static int64_t
xed_internal_strtoll(const char* buf, int base)
{
    switch(base)
    {
      case 0:
        if (strlen(buf) > 2 && buf[0] == '0' && 
            (buf[1] == 'x' || buf[1] == 'X'))
        {
            return convert_base16(buf);
        }
        return convert_base10(buf);
      case 10:
        return convert_base10(buf);
      case 16:
        return convert_base16(buf);
      default:
        xed_assert(0);
    }
    return 0;
}

#endif

int64_t xed_strtoll(const char* buf, int base)
{
#if defined(_WIN32) && !defined(__GNUC__)
    // 64b version missing on some MS compilers
    return xed_internal_strtoll(buf,base);
#else
    return strtoll(buf,0,base);
#endif
}



////////////////////////////////////////////////////////////////////////////
//Local Variables:
//pref: "xed-examples-util.H"
//End:
