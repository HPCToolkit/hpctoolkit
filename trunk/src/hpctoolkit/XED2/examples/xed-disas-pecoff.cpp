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
/// @file xed-disas-pecoff.cpp
/// @author Mark Charney   <mark.charney@intel.com>

//// ONLY COMPILES IF -mno-cygwin is thrown on to GCC compilations

#include "xed-disas-pecoff.H"

#if defined(XED_PECOFF_FILE_READER)
#include <sstream>
#include <iostream>
#include <iomanip>


// windows specific headers
#include <windows.h>
#include <winnt.h>

// xed headers -- THESE MUST BE AFTER THE WINDOWS HEADERS


extern "C" {
#include "xed-interface.h"
#include "xed-examples-util.h"
#include "xed-portability.h" // This really must be after the windows.h include
}

#include "xed-disas-pecoff.h"
#include "xed-examples-ostreams.h"
using namespace std;

// Pronto
static std::string
windows_error(const char* syscall, 
              const char* filename)
{
  std::ostringstream os;
  os << "Mapped file:: " << syscall
     << " for file " << filename << " failed: ";
  switch (GetLastError())
    {
    case 2:
      os << "File not found";
      break;
    case 3:
      os << "Path not found";
      break;
    case 5:
      os <<  "Access denied";
      break;
    case 15:
      os << "Invalid drive";
      break;
    default:
      os << "error code " << STATIC_CAST(uint32_t,GetLastError());
      break;
    }

  return os.str();
}

class pecoff_reader_t
{
  /// NT handle for the open file.
  void* file_handle_;

  /// NT handle for the memory mapping.
  void* map_handle_;

  void* base_;
  bool okay_;

  const IMAGE_SECTION_HEADER* hdr;
  const IMAGE_SECTION_HEADER* orig_hdr;
  unsigned int nsections;
  uint64_t image_base;


public:
  uint32_t section_index;

  pecoff_reader_t()
  {
    init();
  }
  ~pecoff_reader_t()
  {
    close();
  }

  void* base() const { return base_; }
  bool okay() const { return okay_; }

  void
  init()
  {
    file_handle_ = INVALID_HANDLE_VALUE;
    map_handle_ = INVALID_HANDLE_VALUE;
    okay_ = false;
    
    hdr=0;
    orig_hdr=0;
    nsections=0;
    image_base=0;
    section_index=0;
  }

  void
  close()
  {
    if (base_)
      {
        UnmapViewOfFile(base_);
      }
    if (map_handle_ != INVALID_HANDLE_VALUE)
      {
        CloseHandle(map_handle_);
      }
    if (file_handle_ != INVALID_HANDLE_VALUE)
      {
        CloseHandle(file_handle_);
      }
        
    init();
  }


  bool
  map_region(const char* input_file_name, 
             void*& vregion,
             uint32_t& len)
  {
    std::string error_msg;
    okay_ = false;

    file_handle_ = CreateFile(input_file_name,
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              FILE_FLAG_NO_BUFFERING + FILE_ATTRIBUTE_READONLY,
                              NULL);
    if (file_handle_ == INVALID_HANDLE_VALUE)  {
      error_msg = windows_error("CreateFile", input_file_name);
      xedex_derror(error_msg.c_str());
    }

    map_handle_ = CreateFileMapping(file_handle_,
                                    NULL,
                                    PAGE_READONLY,
                                    0,
                                    0,
                                    NULL);

    if (map_handle_ == INVALID_HANDLE_VALUE)   {
      error_msg = windows_error("CreateFileMapping", input_file_name);
      xedex_derror(error_msg.c_str());
    }

    base_ = MapViewOfFile(map_handle_,
                          FILE_MAP_READ, 0, 0, 0);
    if (base_ != NULL)   {
      okay_ = true;
      vregion = base_;
      len = 0; //FIXME
      return true;
    }
    error_msg = windows_error("MapViewOfFile", input_file_name);
    CloseHandle(map_handle_);
    map_handle_ = INVALID_HANDLE_VALUE;
        
    CloseHandle(file_handle_);
    file_handle_ = INVALID_HANDLE_VALUE;
    return false;
  }


  bool read_header() {
    if (! parse_nt_file_header(&nsections, &image_base, &hdr)) {
      xedex_derror("Could not read nt file header");
      return false;
    }

    orig_hdr=hdr;
    return true;
  }
  void print_section_headers() { 
    const IMAGE_SECTION_HEADER* jhdr = orig_hdr;
    for (unsigned int j = 0; j < nsections; j++, jhdr++)   {
        cout << "# SECNAME  " << j << " "
             << reinterpret_cast<const char*>(jhdr->Name) 
             << endl;
    }
  }

  bool
  module_section_info(
                      const char* secname,
                      uint8_t*& section_start,
                      uint32_t& section_size,
                      uint64_t& virtual_addr)
  {
    unsigned int i,ii;
        
    if (secname == 0)
        secname = ".text";

    // Extract the name into a 0-padded 8 byte string.
    char my_name[IMAGE_SIZEOF_SHORT_NAME];
    memset(my_name,0,IMAGE_SIZEOF_SHORT_NAME);
    for( i=0;i<IMAGE_SIZEOF_SHORT_NAME;i++)   {
      my_name[i] = secname[i];
      if (secname[i] == 0)
	break;
    }

    // There are section names that LOOK like .text$x but they really have
    // a null string embedded in them. So when you strcmp, you hit the
    // null.
    
    // match the substring that starts with the given target_section_name
    unsigned int match_len = static_cast<unsigned int>(strlen(secname));
    if (match_len > IMAGE_SIZEOF_SHORT_NAME)
        match_len = IMAGE_SIZEOF_SHORT_NAME;

    for ( ii = section_index; ii < nsections; ii++, hdr++)   {
        if (strncmp(reinterpret_cast<const char*>(hdr->Name), my_name,
		    match_len) == 0) {
            // Found it.  Extract the info and return.
            virtual_addr  = hdr->VirtualAddress  + image_base;
            section_size = (hdr->Misc.VirtualSize > 0 ? 
                            hdr->Misc.VirtualSize
                            : hdr->SizeOfRawData);
            section_start = (uint8_t*)ptr_add(base_, hdr->PointerToRawData);
            section_index = ii+1;
            hdr++;
            return true;
          }
      }

    return false;
  }

private:
  static inline const void*
  ptr_add(const void* ptr, unsigned int n)  {
    return static_cast<const char*>(ptr)+n;
  }

  bool
  is_valid_module()  {
    // Point to the DOS header and check it.
    const IMAGE_DOS_HEADER* dh = static_cast<const IMAGE_DOS_HEADER*>(base_);
    if (dh->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
        
    // Point to the PE signature word and check it.
    const DWORD* sig = static_cast<const DWORD*>(ptr_add(base_, dh->e_lfanew));
        
    // This must be a valid PE file with a valid DOS header.
    if (*sig != IMAGE_NT_SIGNATURE)
        return false;

    return true;
  }


  bool
  parse_nt_file_header(unsigned int* pnsections,
                       uint64_t* pimage_base,
                       const IMAGE_SECTION_HEADER** phdr)
  {
    // Oh joy - the format of a .obj file on Windows is *different*
    // from the format of a .exe file.  Deal with that.
    const IMAGE_FILE_HEADER* ifh;
        
    // Check the header to see if this is a valid .exe file
    if (is_valid_module())
      {
        // Point to the DOS header.
        const IMAGE_DOS_HEADER* dh = static_cast<const IMAGE_DOS_HEADER*>(base_);
            
        // Point to the COFF File Header (just after the signature)
        ifh = static_cast<const IMAGE_FILE_HEADER*>(ptr_add(base_, dh->e_lfanew + 4));
      }
    else
      {
        // Maybe this is a .obj file, which starts with the image file header
        ifh = static_cast<const IMAGE_FILE_HEADER*>(base_);
      }
        
#if !defined(IMAGE_FILE_MACHINE_AMD64)
# define IMAGE_FILE_MACHINE_AMD64 0x8664
#endif

    if (ifh->Machine != IMAGE_FILE_MACHINE_I386
        && ifh->Machine != IMAGE_FILE_MACHINE_AMD64)
      {
        // We only support Windows formats on IA32 and Intel64
        return false;
      }
        
    if (ifh->Machine == IMAGE_FILE_MACHINE_AMD64)
      cout << "# Intel64 format" << endl;
    if (ifh->Machine == IMAGE_FILE_MACHINE_I386)
      cout << "# IA32 format" << endl;
        
    *pimage_base = 0;
        
    // Very important to use the 32b header here because the
    // unqualified IMAGE_OPTIONAL_HEADER gets the wrong version on
    // win64!
    const IMAGE_OPTIONAL_HEADER32* opthdr32
      = static_cast<const IMAGE_OPTIONAL_HEADER32*>(ptr_add(ifh, sizeof(*ifh)));

    // Cygwin's w32api winnt.h header doesn't distinguish 32 and 64b.
#if !defined(IMAGE_NT_OPTIONAL_HDR32_MAGIC)
# define IMAGE_NT_OPTIONAL_HDR32_MAGIC IMAGE_NT_OPTIONAL_HDR_MAGIC
#endif
    // And it lacks the definition for 64b headers
#if !defined(IMAGE_NT_OPTIONAL_HDR64_MAGIC)
# define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b
#endif

    if (ifh->SizeOfOptionalHeader > 0)
      {
        if (opthdr32->Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
          {
            *pimage_base = opthdr32->ImageBase;
            //cerr << hex << "IMAGE BASE 32b: " << *pimage_base << dec << endl;
          }
        else if (opthdr32->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
          {
#if defined(_MSC_VER)
# if _MSC_VER >= 1400
            const IMAGE_OPTIONAL_HEADER64* opthdr64 =
              static_cast<const IMAGE_OPTIONAL_HEADER64*>(ptr_add(ifh, sizeof(*ifh)));
            *pimage_base = opthdr64->ImageBase;
            //cerr << hex << "IMAGE BASE 64b: " << *pimage_base << dec << endl;
# else
            xedex_derror("No support for 64b optional headers because older MS compilers do not have the type yet");
# endif
#else
            xedex_derror("No support for 64b optional headers because cygwin does nt have the type yet");
            return false;
#endif
          }
        else 
          {
            // Optional header is not a form we recognize, so punt.
            return false;
          }
      }
        
    // Point to the first of the Section Headers
    *phdr = static_cast<const IMAGE_SECTION_HEADER*>(ptr_add(opthdr32,
                                                             ifh->SizeOfOptionalHeader));
    *pnsections = ifh->NumberOfSections;
    return true;
  }



};

////////////////////////////////////////////////////////////////////////////



void
process_pecoff(uint8_t* start,
               unsigned int length,
               xed_decode_file_info_t& decode_info,
               pecoff_reader_t& reader,
               const char* target_section)
{
  uint8_t* section_start = 0;
  uint32_t section_size = 0;
  uint64_t runtime_vaddr  = 0;
    
  bool okay = true;
  bool found = false;
  while(okay) {
      okay = reader.module_section_info(target_section,
                                        section_start,
                                        section_size,
                                        runtime_vaddr);
      if (okay) { 
          printf ("# SECTION %d\n", reader.section_index-1);
          found = true;
          xed_disas_test(&decode_info.dstate, 
                         REINTERPRET_CAST(unsigned char*,start),
                         REINTERPRET_CAST(unsigned char*,section_start), 
                         REINTERPRET_CAST(unsigned char*,section_start + section_size),
                         decode_info.ninst,
                         runtime_vaddr,
                         decode_info.decode_only);
      }
  }
  if (!found)
      xedex_derror("text section not found");
  (void) length;
}

void
xed_disas_pecoff(const char* input_file_name,
                 const xed_state_t* dstate,
                 int ninst,
                 bool sixty_four_bit,
                 bool decode_only,
                 const char* target_section)
{
  uint8_t* region = 0;
  void* vregion = 0;
  uint32_t len = 0;
  pecoff_reader_t image_reader;
  bool okay = image_reader.map_region(input_file_name, vregion, len);
  if (!okay)
    xedex_derror("image read failed");
  if (CLIENT_VERBOSE1)
    printf("Mapped image\n");
  image_reader.read_header();
  region = REINTERPRET_CAST(uint8_t*,vregion);
    
  xed_decode_file_info_t decode_info;
  xed_decode_file_info_init(&decode_info, dstate, ninst, decode_only);    

  process_pecoff(region, len,  decode_info, image_reader, target_section);
  xed_print_decode_stats();
  (void)sixty_four_bit;
}
 


#endif
//Local Variables:
//pref: "xed-disas-pecoff.H"
//End:
