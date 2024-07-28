// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// funclist.c - program to print the function list from its load-object arguments

#include  "fnbounds.h"
#include  "scan.h"

#include "../../common/hpctoolkit-version.h"
#include "../messages/messages.h"

#include <errno.h>
#include <stdio.h>
#include <sys/auxv.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define MAX_FUNC          (65536)
#define TB_SIZE                 (512)
#define MAX_SYM_SIZE        (TB_SIZE)

// external strings containing the function sources
//
static char elfGenericErr[] = "libelf error in fnbounds:";

static FnboundsResponse process_vdso();
static FnboundsResponse process_mapped_header(Elf *e, const char* xname);
static int     func_cmp(const void *a, const void *b);


// Methods for the various sources of functions
static uint64_t        dynsymread(FnboundsResponse*, Elf *e, GElf_Shdr sh, const char* xname);
static uint64_t        symtabread(FnboundsResponse*, Elf *e, GElf_Shdr sh, const char* xname);
static uint64_t  symsecread(FnboundsResponse*, Elf *e, GElf_Shdr sechdr);

FnboundsResponse
fnb_get_funclist(const char *name)
{
  int fd;
  Elf   *e;
  FnboundsResponse ret = {
    .entries = NULL,
    .num_entries = 0,
    .max_entries = 0,
    .is_relocatable = 0,
    .reference_offset = 0,
  };

  // Special-case the name "[vdso]"
  if (strcmp (name, "[vdso]") == 0 ) {
    return process_vdso();
  }

  // open the file
  fd = open(name, O_RDONLY);
  if (fd == -1) {
    // the open failed
    ETMSG(FNBOUNDS, "Server failure processing %s: open failed -- %s", name, strerror(errno) );
    return ret;
  }

  if (elf_version(EV_CURRENT) == EV_NONE) {
    ETMSG(FNBOUNDS, "Server failure processing %s: %s", name, elf_errmsg(-1));
    close (fd);
    return ret;
  }

  e = elf_begin(fd,ELF_C_READ, NULL);
  if (e == NULL) {
    ETMSG(FNBOUNDS, "Server failure processing %s: %s", name, elf_errmsg(-1));
    close (fd);
    return ret;
  }

  // process the mapped header
  // ret points to either a char error buffer or is NULL,
  // in which case just return it to indicate success
  //
  ret = process_mapped_header(e, name);

  (void) elf_end(e);
  close (fd);
  return ret;
}

FnboundsResponse
process_vdso()
{
  Elf *e = (Elf *)getauxval(AT_SYSINFO_EHDR);
#if 1
  Elf_Kind ek = elf_kind(e);
  if (ek != ELF_K_ELF) {
    ETMSG(FNBOUNDS, "Server failure processing [vdso]: Warning, vdso is not elf_kind");
    return (FnboundsResponse){NULL, 0, 0, 0, 0};
  } else {
    ETMSG(FNBOUNDS, "Server failure processing [vdso]: Warning, vdso IS elf_kind");
    return (FnboundsResponse){NULL, 0, 0, 0, 0};
  }
#endif
  return process_mapped_header(e, "[vdso]");
}

// process_mapped_header -- verify the header, and read the functions
FnboundsResponse
process_mapped_header(Elf *lelf, const char* xname)
{
  int i;
  size_t secHeadStringIndex,nsec;
  GElf_Shdr secHead;
  Elf_Scn *section;
  GElf_Ehdr ehdr;
  char *secName;
  size_t j,jn;
  GElf_Phdr progHeader;
  ehRecord_t ehInfo;
  uint64_t rf;
  char elfclass;

  FnboundsResponse response = {
    .entries = NULL,
    .num_entries = 0,
    .max_entries = 0,
    .is_relocatable = 0,
    .reference_offset = 0,
  };

  // verify the header is as it should be

  if (gelf_getehdr(lelf, &ehdr) == NULL) {
    ETMSG(FNBOUNDS, "Server failure processing %s: %s %s", xname, elfGenericErr, elf_errmsg(-1));
    return response;
  }

  if (    (ehdr.e_ident[EI_MAG0] != ELFMAG0) ||
          (ehdr.e_ident[EI_MAG1] != ELFMAG1) ||
          (ehdr.e_ident[EI_MAG2] != ELFMAG2) ||
          (ehdr.e_ident[EI_MAG3] != ELFMAG3) ) {
    ETMSG(FNBOUNDS, "Server failure processing %s: incorrect elf header magic numbers", xname);
    return response;
  }
  elfclass = ehdr.e_ident[EI_CLASS];

  if( elfclass != ELFCLASS64 ) {
    ETMSG(FNBOUNDS, "Server failure processing %s: incorrect elfclass -- 0x%x", xname, elfclass);
    return response;
  }

  if (ehdr.e_type == ET_EXEC) {
    response.is_relocatable = 0;
  }
  else if (ehdr.e_type == ET_DYN) {
    response.is_relocatable = 1;
  } else {
    // we should not see this
    ETMSG(FNBOUNDS, "Server failure processing %s: Filetype is neither ET_EXEC nor ET_DYN", xname);
    return response;
  }

  //
  // determine the load address, it's the first v_addr of a program
  // header marked PT_LOAD, with the execute flag set
  //
  if (elf_getphdrnum(lelf,&jn) != 0) {
    ETMSG(FNBOUNDS, "Server failure processing %s: %s %s", xname, elfGenericErr, elf_errmsg(-1));
    return response;
  }
  for (j=0; j<jn; j++) {
    if (gelf_getphdr(lelf,j,&progHeader) != &progHeader) {
      ETMSG(FNBOUNDS, "Server failure processing %s: %s %s", xname, elfGenericErr, elf_errmsg(-1));
      return response;
    }
    if ( (progHeader.p_type == PT_LOAD) && ((progHeader.p_flags & PF_X) == PF_X ) ) {
      response.reference_offset = progHeader.p_vaddr;
      break;
    }
  }

  if (j == jn) {
    ETMSG(FNBOUNDS, "Server failure processing %s: no executable PT_LOAD program header found of %ld headers", xname, jn);
    return response;
  }

  if (elf_getshdrnum(lelf, &nsec) != 0) {
    ETMSG(FNBOUNDS, "Server failure processing %s: %s %s", xname, elfGenericErr, elf_errmsg(-1));
    return response;
  }

  if (elf_getshdrstrndx(lelf, &secHeadStringIndex) != 0) {
    ETMSG(FNBOUNDS, "Server failure processing %s: %s %s", xname, elfGenericErr, elf_errmsg(-1));
    return response;
  }

  // Allocate an initial FunctionVector to start filling
  // This is a "safe" initialization.  If it causes extra memory to be
  // mapped (i.e. memory usage increases markedly, it could be removed.
  response.entries = calloc(MAX_FUNC, sizeof response.entries[0]);
  response.max_entries = MAX_FUNC;
  TMSG(FNBOUNDS_EXT, "Initial farray allocated for %ld functions", response.max_entries);

  // initialize ehRecord
  ehInfo.ehHdrSection = NULL;
  ehInfo.ehFrameSection = NULL;
  ehInfo.textSection = NULL;
  ehInfo.dataSection = NULL;
  ehInfo.ehHdrIndex = 0;
  ehInfo.ehFrameIndex = 0;

  section = NULL;

  bool symTabPresent = false;
  bool dynSymIssue = false;
  //
  // This is the main loop for traversing the sections.
  // NB section numbering starts at 1, not 0.
  //
  for(i=1; i < nsec; i++) {
    section = elf_nextscn(lelf, section);
    if (section == NULL) {
      ETMSG(FNBOUNDS, "Server failure processing %s: section count mismatch, expected %d but got %d", xname, (uint32_t)nsec, i);
      free(response.entries);
      response.entries = NULL;
      response.max_entries = 0;
      return response;
    }

    if (gelf_getshdr(section, &secHead) != &secHead) {
      ETMSG(FNBOUNDS, "Server failure processing %s: %s %s", xname, elfGenericErr, elf_errmsg(-1));
      free(response.entries);
      response.entries = NULL;
      response.max_entries = 0;
      return response;
    }
    secName = elf_strptr(lelf, secHeadStringIndex, secHead.sh_name);
    if (secName == NULL) {
      ETMSG(FNBOUNDS, "Server failure processing %s: %s %s", xname, elfGenericErr, elf_errmsg(-1));
      free(response.entries);
      response.entries = NULL;
      response.max_entries = 0;
      return response;
    }
    if (secHead.sh_flags == (SHF_ALLOC|SHF_EXECINSTR)) {
      fnb_add_function (&response, (void*)secHead.sh_addr);
      fnb_add_function(&response, (void*)(secHead.sh_addr+secHead.sh_size));
    }

    if (secHead.sh_type == SHT_SYMTAB) {
      rf = symtabread(&response, lelf, secHead, xname);
      // only skip eh_frame if symtabread was successful
      if (rf && !dynSymIssue){
        symTabPresent = true;
      }
    }
    else if (secHead.sh_type == SHT_DYNSYM) {
      rf = dynsymread(&response, lelf, secHead, xname);
      // force eh_frame read if something went wrong
      if (!rf) {
        dynSymIssue = true;
        symTabPresent = false;
      }
    }

    else if (secHead.sh_type == SHT_PROGBITS) {
      if (!strcmp(secName,".plt")) {
        fnb_pltscan(&response, lelf, secHead, xname);
      }
      else if (!strcmp(secName,".plt.sec")) {
        fnb_pltsecscan(&response, lelf, secHead, xname);
      }
      else if (!strcmp(secName,".init")) {
        fnb_initscan(&response, lelf, secHead, xname);
      }
      else if (!strcmp(secName,".text")) {
        fnb_textscan(&response, lelf, secHead, xname);
        ehInfo.textSection = section;  // may be needed for eh_frame scan
      }
      else if (!strcmp(secName,".data")) {
        fnb_textscan(&response, lelf, secHead, xname);
        ehInfo.dataSection = section;  // may be needed for eh_frame scan
      }
      else if (!strcmp(secName,".fini")) {
        fnb_finiscan(&response, lelf, secHead, xname);
      }
      else if (!strcmp(secName,".eh_frame_hdr")) {
        ehInfo.ehHdrIndex = elf_ndxscn(section);
        ehInfo.ehHdrSection = section;
      }
      else if (!strcmp(secName,".eh_frame")) {
        ehInfo.ehFrameIndex = elf_ndxscn(section);
        ehInfo.ehFrameSection = section;
      }
      else if (!strcmp(secName,".altinstr_replacement")) {
        fnb_altinstr_replacementscan(&response, lelf, secHead, xname);
      }
    }
  }
  //
  // any eh_frame scans are done after traversing the sections,
  // because various of them may be needed for relative addressing
  // Only scan the eh_frame if no symtab available.  However this will
  // cause any personality/landing functions to be missed; to get these,
  // always call ehframescan.  If there was an error with another section,
  // scan the eh_frame regardless.  This is registered in symTabPresent.
  //
  // errors are signaled from within ehframescan, so we dont check for them
  // again here.  effectively we might have gotten plenty of good addresses
  // from the scan, even if there were errors.
  //
  if (!symTabPresent) {
    (void)fnb_ehframescan(&response, lelf, &ehInfo, xname);
  }

  // We have the complete function table, now sort it and remove duplicates
  qsort(response.entries, response.num_entries, sizeof response.entries[0], func_cmp);
  size_t oldnfunc = response.num_entries;
  if (response.num_entries > 0) {
    size_t lastunique = 0;
    for (size_t i = 1; i < response.num_entries; ++i) {
      if (response.entries[lastunique] != response.entries[i]) {
        lastunique++;
        response.entries[lastunique] = response.entries[i];
      }
    }
    response.num_entries = lastunique + 1;
  }
  TMSG(FNBOUNDS, "%s = %ld (%ld) -- %s", strrchr(xname, '/'), response.num_entries, oldnfunc, xname);

  return response;
}

// Routines to read the elf sections

uint64_t
dynsymread(FnboundsResponse* resp, Elf *e, GElf_Shdr sechdr, const char* xname)
{
  uint64_t rf;

  if (!fnb_doSectionScan(e, sechdr, xname)) {
    return false;
  }

  rf = symsecread (resp, e, sechdr);

  return rf;

}

uint64_t
symtabread(FnboundsResponse* resp, Elf *e, GElf_Shdr sechdr, const char* xname)
{
  uint64_t rf;

  if (!fnb_doSectionScan(e, sechdr, xname)) {
      return false;
  }

  rf = symsecread (resp, e, sechdr);

  return rf;

}

uint64_t
symsecread(FnboundsResponse* resp, Elf *e, GElf_Shdr secHead)
{
  Elf_Data *data;
  char *symName;
  uint64_t count;
  GElf_Sym curSym;
  Elf_Scn *section;
  uint64_t ii,symType;
  // char *marmite;

  section = gelf_offscn(e,secHead.sh_offset);  // back read section from header offset
  if (section == NULL) {
    ETMSG(FNBOUNDS, "%s %s", elfGenericErr, elf_errmsg(-1));
    return false;
  }
  data = elf_getdata(section, NULL);           // use it to get the data
  if (data == NULL) {
    ETMSG(FNBOUNDS, "%s %s", elfGenericErr, elf_errmsg(-1));
    return false;
  }


  count = (secHead.sh_size)/(secHead.sh_entsize);
  for (ii=0; ii<count; ii++) {
    if (gelf_getsym(data, ii, &curSym) != &curSym) {
      ETMSG(FNBOUNDS, "%s %s", elfGenericErr, elf_errmsg(-1));
      return false;
    }
    symName = elf_strptr(e, secHead.sh_link, curSym.st_name);
    if (symName == NULL) {
      ETMSG(FNBOUNDS, "%s %s", elfGenericErr, elf_errmsg(-1));
      return false;
    }
    symType = GELF_ST_TYPE(curSym.st_info);

    if ( (symType == STT_FUNC) && (curSym.st_value != 0) ) {
      fnb_add_function(resp, (void*)curSym.st_value);
      // this hack in case the symName was going away with the
      // closed elf *, but that doesn't seem to be happening.
      // marmite = strdup(symName);
      // fnb_add_function(curSym.st_value, marmite, src, FR_YES);
    }
  }

  return true;

}

void
fnb_add_function(FnboundsResponse* resp, void* faddr)
{
  void** of;
  uint64_t k;

  resp->entries[resp->num_entries] = faddr;
  resp->num_entries ++;
  if (resp->num_entries >= resp->max_entries) {
    // current table is full; double its size
    resp->max_entries = 2*resp->max_entries;
    of = resp->entries;
    resp->entries = realloc(resp->entries, resp->max_entries * sizeof resp->entries[0] );
    TMSG(FNBOUNDS_EXT, "Increasing farray size to %ld functions %s",
        resp->max_entries, (of == resp->entries ? "(not moved)" : "(moved)") );
    if (resp->entries == NULL) {
      hpcrun_abort("hpcrun: unable to increase function table to %ld functions; exiting", resp->max_entries);
    }
    //
    // initialize the new part of the table.
    //
    for (k = resp->num_entries; k < resp->max_entries; k++) {
      resp->entries[k] = 0ull;
    }
  }
}

int
func_cmp(const void *a, const void *b)
{
  int ret = 0;

  uint64_t fp1 = *((const uint64_t *) a);
  uint64_t fp2 = *((const uint64_t *) b);
  if (fp1 > fp2 ) {
    ret = 1;
  } else if (fp1 < fp2 ) {
    ret = -1;
  }
  return ret;
}
