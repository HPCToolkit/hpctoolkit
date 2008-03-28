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
/// @file xed-disas-macho.cpp
/// @author Mark Charney   <mark.charney@intel.com>

#include "xed-disas-macho.H"

#if defined(XED_MAC_OSX_FILE_READER)

// mac specific headers
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/stab.h>
#include <mach-o/nlist.h>

extern "C" {
#include "xed-interface.h"
#include "xed-examples-util.h"
}
#include <string.h>
#include <iostream>
using namespace std;

////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
uint32_t 
swap_endian(uint32_t x)
{
    uint32_t r = 0;
    uint32_t t = x;
    uint_t i; 
    for(i=0;i<4;i++)
    {
        uint8_t b = t;
        r =(r << 8)  | b;
        t = t >> 8;
    }
    return r;
}

bool
read_fat_header(uint8_t*&current_position, uint32_t& offset, uint32_t& size)
{
    struct fat_header* fh =
        REINTERPRET_CAST(struct fat_header*,current_position);
    
    // we are little endian looking at big endian data
    if (fh->magic == FAT_CIGAM)
    {
        uint32_t narch = swap_endian(fh->nfat_arch);
        unsigned int i;
        for( i=0 ;i< narch; i++)
        {
            struct fat_arch* fa = 
                REINTERPRET_CAST(struct fat_arch*,current_position + 
                                              sizeof(struct fat_header) + 
                                              i*sizeof(struct fat_arch) );
            const cpu_type_t cpu_type = swap_endian(fa->cputype);

            if (cpu_type == CPU_TYPE_I386)
            {
                offset  = swap_endian(fa->offset);   
                size   = swap_endian(fa->size);   
                return true;
            }
        }
    }
    return false;
}


static bool 
executable(uint32_t flags)
{
    return ( (flags & S_ATTR_PURE_INSTRUCTIONS) !=0  || 
             (flags & S_ATTR_SOME_INSTRUCTIONS) !=0  );
}

void
process_segment32( xed_decode_file_info_t& decode_info,
                   uint8_t* start,
                   uint8_t* segment_position,
                   unsigned int bytes)
{
    struct segment_command* sc = REINTERPRET_CAST(struct segment_command*,segment_position);
    uint8_t* start_of_section_data = segment_position + sizeof(struct segment_command);
    unsigned int i;
    cout << sc->nsects << " sections" << endl;
    // look through the array of section headers for this segment.
    for( i=0; i< sc->nsects;i++)
    {
        struct section* sp = 
            REINTERPRET_CAST(struct section*,start_of_section_data + i *sizeof(struct section));
        if (executable(sp->flags))
        {
            // this section is executable. Go get it and process it.
            uint8_t* section_text = start + sp->offset;
            uint32_t runtime_vaddr = sp->addr;

            cout << "\tProcessing executable section "
                 << i 
                 << " addr in mem: " 
                 << hex
                 << REINTERPRET_CAST(uint32_t,section_text)
                 << dec
                 << " len= " <<  sp->size 
                 << " at offset " << sp->offset
                 << " runtime addr " << hex << runtime_vaddr << dec
                 << endl;


            xed_disas_test(&decode_info.dstate, 
                           start,
                           section_text, 
                           section_text + sp->size,
                           decode_info.ninst,
                           runtime_vaddr,
                           decode_info.decode_only);
        }

    }
}

////////////////////////////////////////////////////////////////////////////

void
process_macho64(uint8_t* start,
                unsigned int length,
                xed_decode_file_info_t& decode_info)
{
    xedex_derror("process_macho64 not done yet");
}



void
process_macho32(uint8_t* start,
                unsigned int length,
                xed_decode_file_info_t& decode_info)
{
    uint8_t* current_position = start;
    //current_position is updated when each section is read

    // the fat header reader bumps current_position to the value for the
    // correct architecture.
    uint32_t offset=0; // offset to  of load commands for this architecture
    uint32_t size;
    uint_t i;
    bool okay = read_fat_header(current_position, offset, size);
    if (!okay)
    {
        xedex_dwarn("Could not find x86 section of fat binary -- checking for mach header");
    }
    if (CLIENT_VERBOSE2)
        printf("Offset of load sections = %x\n", offset);

    // skip to the correct architecture
    current_position += offset;

    struct mach_header* mh = REINTERPRET_CAST(struct mach_header*,current_position);
    if (mh->magic != MH_MAGIC)
    {
         xedex_derror("Could not find mach header");
    }

    current_position += sizeof(struct mach_header);

    if (CLIENT_VERBOSE2)
        printf("Number of load command sections = %d\n", mh->ncmds);
    // load commands point to segments which contain sections.
    //uint8_t* segment_position = current_position + mh->sizeofcmds;
    for( i=0;i< mh->ncmds; i++)
    {
        struct load_command* lc = 
            REINTERPRET_CAST(struct load_command*,current_position);
        //current_position += sizeof(struct load_command);
    
        if (CLIENT_VERBOSE2)
            printf("load command %d\n",i );
        if (lc->cmd == LC_SEGMENT)
        {
            if (CLIENT_VERBOSE2)
                printf("\tload command %d is a LC_SEGMENT\n", i);
            // we add the FAT offset to the start pointer to get to the relative start point.
            process_segment32( decode_info, start + offset, current_position, lc->cmdsize );
        }
        current_position += lc->cmdsize;
        //segment_position = segment_position + lc->cmdsize;
    }
}

void
xed_disas_macho(const char* input_file_name,
                const xed_state_t* dstate,
                int ninst,
                bool sixty_four_bit,
                bool decode_only)
{
    uint8_t* region = 0;
    void* vregion = 0;
    unsigned int len = 0;
    xed_map_region(input_file_name, &vregion, &len);
    region = REINTERPRET_CAST(uint8_t*,vregion);
    
    xed_decode_file_info_t decode_info;
    xed_decode_file_info_init(&decode_info,dstate, ninst, decode_only);    

    if (sixty_four_bit) 
    {
        process_macho64(region, len, decode_info);
    }
    else 
    {
        process_macho32(region, len, decode_info);
    }
    xed_print_decode_stats();
}
 


#endif
