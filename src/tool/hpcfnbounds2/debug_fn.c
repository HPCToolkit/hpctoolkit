// -*-Mode: C++;-*-

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
/*  debug.c -- print out various elements of the data structures */

//******************************************************************************
// include files 
//******************************************************************************

#include	"fnbounds.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
print_elf_header64
(
 GElf_Ehdr *elf_header
)
{

  fprintf(stderr, "========================================\n");
  fprintf(stderr, "\t\tELF Header: %s\n", xname);

  /* Storage capacity class */
  fprintf(stderr, "Storage class\t= ");
  switch(elf_header->e_ident[EI_CLASS]) {
  case ELFCLASS32:
    fprintf(stderr, "32-bit objects\n");
    break;

  case ELFCLASS64:
    fprintf(stderr, "64-bit objects\n");
    break;

  default:
    fprintf(stderr, "INVALID CLASS\n");
    break;
  }

  /* Data Format */
  fprintf(stderr, "Data format\t= ");
  switch(elf_header->e_ident[EI_DATA]) {
  case ELFDATA2LSB:
    fprintf(stderr, "2's complement, little endian\n");
    break;

  case ELFDATA2MSB:
    fprintf(stderr, "2's complement, big endian\n");
    break;

  default:
    fprintf(stderr, "INVALID Format\n");
    break;
  }

  /* OS ABI */
  fprintf(stderr, "OS ABI\t\t= ");
  switch(elf_header->e_ident[EI_OSABI]) {
  case ELFOSABI_SYSV:
    fprintf(stderr, "UNIX System V ABI\n");
    break;

  case ELFOSABI_HPUX:
    fprintf(stderr, "HP-UX\n");
    break;

  case ELFOSABI_NETBSD:
    fprintf(stderr, "NetBSD\n");
    break;

  case ELFOSABI_LINUX:
    fprintf(stderr, "Linux\n");
    break;

  case ELFOSABI_SOLARIS:
    fprintf(stderr, "Sun Solaris\n");
    break;

  case ELFOSABI_AIX:
    fprintf(stderr, "IBM AIX\n");
    break;

  case ELFOSABI_IRIX:
    fprintf(stderr, "SGI Irix\n");
    break;

  case ELFOSABI_FREEBSD:
    fprintf(stderr, "FreeBSD\n");
    break;

  case ELFOSABI_TRU64:
    fprintf(stderr, "Compaq TRU64 UNIX\n");
    break;

  case ELFOSABI_MODESTO:
    fprintf(stderr, "Novell Modesto\n");
    break;

  case ELFOSABI_OPENBSD:
    fprintf(stderr, "OpenBSD\n");
    break;

  case ELFOSABI_ARM_AEABI:
    fprintf(stderr, "ARM EABI\n");
    break;

  case ELFOSABI_ARM:
    fprintf(stderr, "ARM\n");
    break;

  case ELFOSABI_STANDALONE:
    fprintf(stderr, "Standalone (embedded) app\n");
    break;

  default:
    fprintf(stderr, "Unknown (0x%x)\n", elf_header->e_ident[EI_OSABI]);
    break;
  }

  /* ELF filetype */
  fprintf(stderr, "Filetype \t= ");
  switch(elf_header->e_type) {
  case ET_NONE:
    fprintf(stderr, "N/A (0x0)\n");
    break;

  case ET_REL:
    fprintf(stderr, "Relocatable\n");
    break;

  case ET_EXEC:
    fprintf(stderr, "Executable\n");
    break;

  case ET_DYN:
    fprintf(stderr, "Shared Object\n");
    break;

  default:
    fprintf(stderr, "Unknown (0x%x)\n", elf_header->e_type);
    break;
  }

  /* ELF Machine-id */
  fprintf(stderr, "Machine\t\t= ");
  switch(elf_header->e_machine) {
  case EM_NONE:
    fprintf(stderr, "None (0x0)\n");
    break;

  case EM_386:
    fprintf(stderr, "INTEL x86 (0x%x)\n", EM_386);
    break;

  case EM_X86_64:
    fprintf(stderr, "AMD x86_64 (0x%x)\n", EM_X86_64);
    break;

  case EM_AARCH64:
    fprintf(stderr, "AARCH64 (0x%x)\n", EM_AARCH64);
    break;

  default:
    fprintf(stderr, " 0x%x\n", elf_header->e_machine);
    break;
  }

  /* Entry point */
  fprintf(stderr, "Entry point\t= 0x%08lx\n", elf_header->e_entry);

  /* ELF header size in bytes */
  fprintf(stderr, "ELF header size\t= 0x%08x\n", elf_header->e_ehsize);

  /* Program Header */
  fprintf(stderr, "Program Header\t= ");
  fprintf(stderr, "0x%08lx\n", elf_header->e_phoff);		/* start */
  fprintf(stderr, "\t\t  %d entries\n", elf_header->e_phnum);	/* num entry */
  fprintf(stderr, "\t\t  %d bytes\n", elf_header->e_phentsize);	/* size/entry */

  /* Section header starts at */
  fprintf(stderr, "Section Header\t= ");
  fprintf(stderr, "0x%08lx\n", elf_header->e_shoff);		/* start */
  fprintf(stderr, "\t\t  %d entries\n", elf_header->e_shnum);	/* num entry */
  fprintf(stderr, "\t\t  %d bytes\n", elf_header->e_shentsize);	/* size/entry */
  fprintf(stderr, "\t\t  0x%08x (string table offset)\n", elf_header->e_shstrndx);

  /* File flags (Machine specific)*/
  fprintf(stderr, "File flags \t= 0x%08x  ", elf_header->e_flags);

  /* ELF file flags are machine specific.
   * INTEL implements NO flags.
   * ARM implements a few.
   * Add support below to parse ELF file flags on ARM
   * 	 	 	 	 */
  int32_t ef = elf_header->e_flags;
  if (ef & EF_ARM_RELEXEC)
    fprintf(stderr, ",RELEXEC ");

  if (ef & EF_ARM_HASENTRY)
    fprintf(stderr, ",HASENTRY ");

  if (ef & EF_ARM_INTERWORK)
    fprintf(stderr, ",INTERWORK ");

  if (ef & EF_ARM_APCS_26)
    fprintf(stderr, ",APCS_26 ");

  if (ef & EF_ARM_APCS_FLOAT)
    fprintf(stderr, ",APCS_FLOAT ");

  if (ef & EF_ARM_PIC)
    fprintf(stderr, ",PIC ");

  if (ef & EF_ARM_ALIGN8)
    fprintf(stderr, ",ALIGN8 ");

  if (ef & EF_ARM_NEW_ABI)
    fprintf(stderr, ",NEW_ABI ");

  if (ef & EF_ARM_OLD_ABI)
    fprintf(stderr, ",OLD_ABI ");

  if (ef & EF_ARM_SOFT_FLOAT)
    fprintf(stderr, ",SOFT_FLOAT ");

  if (ef & EF_ARM_VFP_FLOAT)
    fprintf(stderr, ",VFP_FLOAT ");

  if (ef & EF_ARM_MAVERICK_FLOAT)
    fprintf(stderr, ",MAVERICK_FLOAT ");

  fprintf(stderr, "\n");

  /* MSB of flags conatins ARM EABI version */
  fprintf(stderr, "ARM EABI\t= Version %d\n", (ef & EF_ARM_EABIMASK)>>24);

  fprintf(stderr, "\n");	/* End of ELF header */
}

// dump program headers
void
print_program_headers64
(
 Elf *e
)
{
  uint64_t j;
  size_t jn;
  GElf_Phdr progHeader;

  fprintf(stderr, "========================================");
  fprintf(stderr, "========================================\n");
  fprintf(stderr, "\t\tProgram Headers: %s\n", xname);
  fprintf(stderr, " idx type       flags      offset     virt-addr          phys-addr          file-size  mem-size   algn\n");
  elf_getphdrnum(e,&jn);
  for (j=0; j<(uint64_t)jn; j++) {
    if (gelf_getphdr(e,j,&progHeader) != &progHeader) {
      fprintf(stderr,"error calling gelf_getphdr: %s\n", elf_errmsg(-1));
      return;
    }
    if (progHeader.p_type == PT_LOAD) {
      refOffset = progHeader.p_vaddr;
    }   
    fprintf(stderr, "%4ld ", j);
    fprintf(stderr, "0x%08x ", progHeader.p_type);
    fprintf(stderr, "0x%08x ", progHeader.p_flags);
    fprintf(stderr, "0x%08lx ", progHeader.p_offset);
    fprintf(stderr, "0x%016lx ", progHeader.p_vaddr);
    fprintf(stderr, "0x%016lx ", progHeader.p_paddr);
    fprintf(stderr, "0x%08lx ", progHeader.p_filesz);
    fprintf(stderr, "0x%08lx ", progHeader.p_memsz);
    fprintf(stderr, "0x%08lx ", progHeader.p_align);
    fprintf(stderr, "\n");
  }

  fprintf(stderr, "\n");	/* end of program header table */
}

// dump section header info
void
print_section_headers64
(
 Elf *e
)
{
  Elf_Scn *section;
  GElf_Shdr secHead;
  char *secName;
  size_t secHeadStringIndex;

  if (elf_getshdrstrndx(e, &secHeadStringIndex) != 0) {
      fprintf(stderr,"error calling elf_getshdrstrndx: %s\n", elf_errmsg(-1));
      return;
    }
  section = NULL;

  fprintf(stderr, "========================================");
  fprintf(stderr, "========================================\n");
  fprintf(stderr, "\t\tSection Headers: %s\n", xname);
  fprintf(stderr, " idx offset     load-addr          size       algn"
        " flags      type       section\n");

  do {
    section = elf_nextscn(e, section);
    if (section == NULL) break;

    if (gelf_getshdr(section, &secHead) != &secHead) {
      fprintf(stderr,"error calling gelf_getshdr: %s\n", elf_errmsg(-1));
      return;
    }

    secName = elf_strptr(e, secHeadStringIndex, secHead.sh_name);
    if (secName == NULL) {
      fprintf(stderr,"error calling elf_strptr: %s\n", elf_errmsg(-1));
      return;
    }

    fprintf(stderr, " %03ld ", (uintmax_t)elf_ndxscn(section));
    fprintf(stderr, "0x%08lx ", secHead.sh_offset);
    fprintf(stderr, "0x%016lx ", secHead.sh_addr);
    fprintf(stderr, "0x%08lx ", secHead.sh_size);
    fprintf(stderr, "%4ld ", secHead.sh_addralign);
    fprintf(stderr, "0x%08lx ", secHead.sh_flags);
    fprintf(stderr, "0x%08x ", secHead.sh_type);
    fprintf(stderr, "%s\t", secName );
    fprintf(stderr, "\n");

  } while (section != NULL);

  fprintf(stderr, "\n");	/* end of section header table */
}

