// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *


//***************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>

#include <igc_binary_decoder.h>
#include <gen_symbols_decoder.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/binutils/ElfHelper.hpp>
#include <lib/support/diagnostics.h>
#include <lib/support/RealPathMgr.cpp>
#include "IntelGPUbinutils.hpp"
#include "CreateCFG.hpp"



//******************************************************************************
// macros
//******************************************************************************

#define DBG 1

#define INTEL_GPU_DEBUG_SECTION_NAME "Intel(R) OpenCL Device Debug"



//******************************************************************************
// private operations
//******************************************************************************

static size_t
file_size(int fd)
{
  struct stat sb;
  int retval = fstat(fd, &sb);
  if (retval == 0 && S_ISREG(sb.st_mode)) {
    return sb.st_size;
  }
  return 0;
}


// Automatically restart short reads.
// This protects against EINTR.
//
static size_t
read_all(int fd, void *buf, size_t count)
{
  ssize_t ret;
  size_t len;

  len = 0;
  while (len < count) {
    ret = read(fd, ((char *) buf) + len, count - len);
    if (ret == 0 || (ret < 0 && errno != EINTR)) {
      break;
    }
    if (ret > 0) {
      len += ret;
    }
  }

  return len;
}


static const char*
openclElfSectionType
(
	Elf64_Word sh_type
)
{
	switch (sh_type) {
    case SHT_OPENCL_SOURCE:
			return "SHT_OPENCL_SOURCE";
    case SHT_OPENCL_HEADER:
			return "SHT_OPENCL_HEADER";
    case SHT_OPENCL_LLVM_TEXT:
			return "SHT_OPENCL_LLVM_TEXT";
    case SHT_OPENCL_LLVM_BINARY:
			return "SHT_OPENCL_LLVM_BINARY";
    case SHT_OPENCL_LLVM_ARCHIVE:
			return "SHT_OPENCL_LLVM_ARCHIVE";
    case SHT_OPENCL_DEV_BINARY:
			return "SHT_OPENCL_DEV_BINARY";
    case SHT_OPENCL_OPTIONS:
			return "SHT_OPENCL_OPTIONS";
    case SHT_OPENCL_PCH:
			return "SHT_OPENCL_PCH";
    case SHT_OPENCL_DEV_DEBUG:
			return "SHT_OPENCL_DEV_DEBUG";
    case SHT_OPENCL_SPIRV:
			return "SHT_OPENCL_SPIRV";
    case SHT_OPENCL_NON_COHERENT_DEV_BINARY:
			return "SHT_OPENCL_NON_COHERENT_DEV_BINARY";
    case SHT_OPENCL_SPIRV_SC_IDS:
			return "SHT_OPENCL_SPIRV_SC_IDS";
    case SHT_OPENCL_SPIRV_SC_VALUES:
			return "SHT_OPENCL_SPIRV_SC_VALUES";
		default:
			return "unknown type";
	}
}


static bool
extract_kernelelfs
(
	std::vector<uint8_t> symbols,
	ElfFileVector *filevector
)
{
	bool extractSuccess = true;
	const uint8_t* ptr = symbols.data();
	const SProgramDebugDataHeaderIGC* header =
		reinterpret_cast<const SProgramDebugDataHeaderIGC*>(ptr);
	ptr += sizeof(SProgramDebugDataHeaderIGC);

	if (header->NumberOfKernels == 0) {
		extractSuccess = false;
	}
	
	for (uint32_t i = 0; i < header->NumberOfKernels; ++i) {
		const SKernelDebugDataHeaderIGC* kernel_header =
			reinterpret_cast<const SKernelDebugDataHeaderIGC*>(ptr);
		ptr += sizeof(SKernelDebugDataHeaderIGC);

		const char* kernel_name = reinterpret_cast<const char*>(ptr);
		char *file_name = (char*)malloc(sizeof(kernel_name));
		strcpy(file_name, kernel_name);
		strcat(file_name, ".gpubin");

		unsigned kernel_name_size_aligned = sizeof(uint32_t) *
			(1 + (kernel_header->KernelNameSize - 1) / sizeof(uint32_t));
		ptr += kernel_name_size_aligned;

		if (kernel_header->SizeVisaDbgInBytes > 0) {
			/*FILE *f_ptr = fopen(kernel_name, "wb");
			fwrite(ptr, kernel_header->SizeVisaDbgInBytes, 1, f_ptr);
			fclose(f_ptr);*/
			std::ifstream in(kernel_name);
			std::string file_contents((std::istreambuf_iterator<char>(in)), 
			    std::istreambuf_iterator<char>());


			ElfFile *elfFile = new ElfFile;
			int file_fd = open(file_name, O_RDONLY);
			size_t f_size = file_size(file_fd);
			char  *file_buffer = (char *) malloc(f_size);
			size_t bytes = read_all(file_fd, file_buffer, f_size);
			bool result = elfFile->open(file_buffer, f_size, file_name);

			filevector->push_back(elfFile);
			
			// start cfg generation
			Elf *elf = elfFile->getElf();
			file_buffer = elfFile->getMemory();
			ElfSectionVector *sections = elfGetSectionVector(elf);
			GElf_Ehdr ehdr_v;
			GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);

			if (ehdr) {
				for (auto si = sections->begin(); si != sections->end(); si++) {
					Elf_Scn *scn = *si;
					GElf_Shdr shdr_v;
					GElf_Shdr *shdr = gelf_getshdr(scn, &shdr_v);
					if (!shdr) continue;
					char *sectionData = elfSectionGetData(file_buffer, shdr);
					const char *section_name = elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name);
					if (strcmp(section_name, ".text") == 0) {
						std::vector<uint8_t> intelRawGenBinary(reinterpret_cast<uint8_t*>(sectionData), 
								reinterpret_cast<uint8_t*>(sectionData) + kernel_header->SizeVisaDbgInBytes);
						printCFGInDotGraph(intelRawGenBinary);
					}
				}
			}
			//end cfg generation
		} else {
			extractSuccess = false;
		}
	}
	return extractSuccess;
}


static bool
isCustomOpenCLBinary
(
	const char *section_name
)
{
  return (strcmp(section_name, ".SHT_OPENCL_DEV_DEBUG") == 0);
}



//******************************************************************************
// interface operations
//******************************************************************************

bool
findIntelGPUbins
(
	ElfFile *elfFile,
	ElfFileVector *filevector
)
{
	bool fileHasDebugSection = false;
	bool extractSuccess = false;

  Elf *elf = elfFile->getElf();
	char *file_buffer = elfFile->getMemory();
  ElfSectionVector *sections = elfGetSectionVector(elf);
  GElf_Ehdr ehdr_v;
  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);

  if (ehdr) {
    for (auto si = sections->begin(); si != sections->end(); si++) {
			Elf_Scn *scn = *si;
			GElf_Shdr shdr_v;
			GElf_Shdr *shdr = gelf_getshdr(scn, &shdr_v);
			if (!shdr) continue;
			char *sectionData = elfSectionGetData(file_buffer, shdr);
			const char *section_name = elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name);
			//std::cerr << "section name: " << section_name << ". section type: " << openclElfSectionType(shdr->sh_type) << std::endl;

			// extract debug section
			if ((shdr->sh_type == SHT_OPENCL_DEV_DEBUG && strcmp(section_name, INTEL_GPU_DEBUG_SECTION_NAME) == 0)
					|| isCustomOpenCLBinary(section_name)) {
				fileHasDebugSection = true;
				std::vector<uint8_t> debug_info(reinterpret_cast<uint8_t*>(sectionData), reinterpret_cast<uint8_t*>(sectionData) + shdr->sh_size);
				extractSuccess = extract_kernelelfs(debug_info, filevector);
				break;
			} /*else if (strcmp(section_name, ".text") == 0) {
				FILE *bin_ptr;
				bin_ptr = fopen("switch.text", "wb");
				fwrite(sectionData, shdr->sh_size, 1, bin_ptr);
				fclose(bin_ptr);
			}*/
    }
  }
	FILE *fptr;
	if (!fileHasDebugSection && (fptr = fopen("opencl_main.debuginfo", "rb"))) {
		fileHasDebugSection = true;
		fseek(fptr, 0L, SEEK_END);
		size_t debug_info_size = ftell(fptr);
		printf("debug_info_size: %zu\n", debug_info_size);
		rewind(fptr);
		std::vector<uint8_t> debug_info(debug_info_size);
		fread(debug_info.data(), debug_info_size, 1, fptr);
		extractSuccess = extract_kernelelfs(debug_info, filevector);
	}
  bool success = fileHasDebugSection && extractSuccess;
  return success; 
}
