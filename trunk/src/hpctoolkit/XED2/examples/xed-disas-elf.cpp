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
/// @file disas-elf.cpp
/// @author Mark Charney   <mark.charney@intel.com>

#include "xed-disas-elf.H"

#if defined(XED_ELF_READER)

////////////////////////////////////////////////////////////////////////////
#include <elf.h>

extern "C" {
#include "xed-interface.h"
#include "xed-portability.h"
#include "xed-examples-util.h"
}

#include <string.h>
#include <stdlib.h>
#include <iostream>
using namespace std;
////////////////////////////////////////////////////////////////////////////


char* 
lookup32(Elf32_Word stoffset,
       void* start,
       Elf32_Off offset)
{
    char* p = (char*)start + offset;
    char* q = p + stoffset;
    // int i;
    //cout << "p = " << (unsigned int) p <<  endl;
    //cout << "q = " << (unsigned int) q <<  endl;
    //for(i=0;i<20;i++)
    //{
    //    cout << q[i];
    //}
    //cout << endl;
    return q;
}

char* 
lookup64(Elf64_Word stoffset,
	 void* start,
	 Elf64_Off offset)
{
  char* p = (char*)start + offset;
  char* q = p + stoffset;
  //int i;
  //cout << "p = " << (unsigned int) p <<  endl;
  //cout << "q = " << (unsigned int) q <<  endl;
  //for( i=0;i<20;i++)
  //{
  //    cout << q[i];
  //}
  //cout << endl;
  return q;
}

void
disas_test32(const xed_state_t* dstate,
	     void* start, 
	     Elf32_Off offset,
	     Elf32_Word size, 
	     int ninst,
             Elf32_Addr runtime_vaddr,
             bool decode_only)
{
  unsigned char* s = (unsigned char*)start;
  unsigned char* a = (unsigned char*)start + offset;
  unsigned char* q = a + size; // end of region
  xed_disas_test(dstate, s,a,q, ninst, runtime_vaddr, decode_only);
}

void
disas_test64(const xed_state_t* dstate,
	     void* start, 
	     Elf64_Off offset,
	     Elf64_Word size,
	     int ninst,
             Elf64_Addr runtime_vaddr,
             bool decode_only)
{
  unsigned char* s = (unsigned char*)start;
  unsigned char* a = (unsigned char*)start + offset;
  unsigned char* q = a + size; // end of region
  xed_disas_test(dstate, s,a,q, ninst, runtime_vaddr, decode_only);
}


void
process_elf32(void* start,
	      unsigned int length,
	      const char* tgt_section,
	      const xed_state_t* dstate,
	      int ninst,
              bool decode_only)
{
    Elf32_Ehdr* elf_hdr = (Elf32_Ehdr*) start;
    if (elf_hdr->e_machine != EM_386) {
        cerr << "Not an IA32 binary. Consider using the -64 switch" << endl;
        exit(1);
    }

    Elf32_Off shoff = elf_hdr->e_shoff;  // section hdr table offset
    Elf32_Shdr* shp = (Elf32_Shdr*) ((char*)start + shoff);
    int sect_strings  = elf_hdr->e_shstrndx;
    //cout << "String section " << sect_strings << endl;
    int nsect = elf_hdr->e_shnum;
    int i;
    for(i=0;i<nsect;i++) {
        char* name = lookup32(shp[i].sh_name, start, shp[sect_strings].sh_offset);
        bool text = false;
        if (shp[i].sh_type == SHT_PROGBITS) {
            if (tgt_section) {
                if (strcmp(tgt_section, name)==0) 
                    text = true;
            }
            else if (shp[i].sh_flags & SHF_EXECINSTR)
                text = true;
        }

        if (text) {
            printf("# SECTION " XED_FMT_U " ", i);
            printf("%25s ", name);
            printf("addr " XED_FMT_LX " ",static_cast<uint64_t>(shp[i].sh_addr)); 
            printf("offset " XED_FMT_LX " ",static_cast<uint64_t>(shp[i].sh_offset));
            printf("size " XED_FMT_LU " ", static_cast<uint64_t>(shp[i].sh_size));
            printf("type " XED_FMT_LU "\n", static_cast<uint64_t>(shp[i].sh_type));

            disas_test32(dstate,
                         start, shp[i].sh_offset, shp[i].sh_size,
                         ninst,
                         shp[i].sh_addr,
                         decode_only);
	}

    }

    (void) length;// pacify compiler
}

void
process_elf64(void* start,
	      unsigned int length,
	      const char* tgt_section,
	      const xed_state_t* dstate,
	      int ninst,
              bool decode_only)

{
    Elf64_Ehdr* elf_hdr = (Elf64_Ehdr*) start;
    if (elf_hdr->e_machine != EM_X86_64) {
        cerr << "Not an x86-64  binary. Consider not using the -64 switch." << endl;
        exit(1);
    }

    Elf64_Off shoff = elf_hdr->e_shoff;  // section hdr table offset
    Elf64_Shdr* shp = (Elf64_Shdr*) ((char*)start + shoff);
    Elf64_Half sect_strings  = elf_hdr->e_shstrndx;
    //cout << "String section " << sect_strings << endl;
    Elf64_Half nsect = elf_hdr->e_shnum;
    if (CLIENT_VERBOSE1) 
        printf("# sections %d\n" , nsect);
    unsigned int i;
    bool text = false;
    for( i=0;i<nsect;i++)  {
        char* name = lookup64(shp[i].sh_name, start, shp[sect_strings].sh_offset);
        
        text = false;
        if (shp[i].sh_type == SHT_PROGBITS) {
            if (tgt_section) {
                if (strcmp(tgt_section, name)==0) 
                    text = true;
            }
            else if (shp[i].sh_flags & SHF_EXECINSTR)
                text = true;
        }

        if (text) {
            printf("# SECTION " XED_FMT_U " ", i);
            printf("%25s ", name);
            printf("addr " XED_FMT_LX " ",static_cast<uint64_t>(shp[i].sh_addr)); 
            printf("offset " XED_FMT_LX " ",static_cast<uint64_t>(shp[i].sh_offset));
            printf("size " XED_FMT_LU "\n", static_cast<uint64_t>(shp[i].sh_size));
            disas_test64(dstate,
                         start, shp[i].sh_offset, shp[i].sh_size, 
                         ninst,
                         shp[i].sh_addr, decode_only);

        }
    }
    (void) length;// pacify compiler
}


void
xed_disas_elf(const char* input_file_name,
              const xed_state_t* dstate,
              int ninst,
              bool sixty_four_bit,
              bool decode_only,
              const char* target_section)
{
    void* region = 0;
    unsigned int len = 0;
    xed_map_region(input_file_name, &region, &len);
    
    
    if (sixty_four_bit) 
        process_elf64(region, len, target_section, dstate, ninst, decode_only);
    else 
        process_elf32(region, len, target_section, dstate, ninst, decode_only);
    xed_print_decode_stats();

}
 


#endif
////////////////////////////////////////////////////////////////////////////
