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
// funclist.c - program to print the function list from its load-object arguments

#include  "fnbounds.h"
#include  "code-ranges.h"
#include  "server.h"
#include  "scan.h"

int verbose = 0;
int scan_code = 1;
int no_dwarf = 0;

size_t  maxfunc = 0;
size_t  nfunc = 0;
Function_t *farray = NULL;


int dynsymread_f = SC_DONE;
int symtabread_f = SC_DONE;
int ehframeread_f = SC_DONE;
int pltscan_f = SC_DONE;
int initscan_f = SC_DONE;
int textscan_f = SC_DONE;
int finiscan_f = SC_DONE;
int altinstr_replacementscan_f = SC_DONE;

int is_dotso;
int server_mode = 0;

static char ebuf[1024]; 
static char ebuf2[1024]; 

uint64_t refOffset;
uint64_t sr;
char  *xname;

// external strings containing the function sources
//
const char __null_[] = {""};
const char __p_[] = {"p"};
const char __d_[] = {"d"};
const char __s_[] = {"s"};
const char __i_[] = {"i"};
const char __t_[] = {"t"};
const char __f_[] = {"f"};
const char __a_[] = {"a"};
const char __e_[] = {"e"};
static char elfGenericErr[] = {"libelf error in fnbounds:"};

int
main(int argc, char **argv, char **envp)
{
  int i;
  char  **p;
  char  *ret;

  int disable_init = 0;
  p = argv;
  p++;
  for (i = 1; i < argc; i ++) {
    if ( strcmp (*p, "-v") == 0 ) {
       verbose = 1;
       fprintf(stderr, "\nBegin execution of hpcfnbounds2\n" );
       p++;
       continue;
     }
     if ( strcmp (*p, "-d") == 0 ) {
       // treat as an alias for "-n pitfa"
       disable_sources ("pitfa");
       scan_code = 0;
       p++;
       continue;
     }
#if 0
     if ( strcmp (*p, "-D") == 0 ) {
       no_dwarf = 1;
       p++;
       continue;
     }
#endif
     if ( strcmp (*p, "-n") == 0 ) {
       p++;
       disable_sources(*p);
       i++;
       p++;
       continue;
     }
     if ( strcmp (*p, "-h") == 0 ) {
       usage();
       exit(0);
     }
      if ( strcmp (*p, "-s") == 0 ) {
       // code to inialize as a server  "-s <infd> <outfd>"
       if ((i+2) > argc) {
         // argument count is wrong
         fprintf (stderr, "hpcfnbounds2 server invocation too few arguments\n" );
         exit(1);
       }
       p++;
       int _infd = atoi (*p);
       p++;
       int _outfd = atoi (*p);
       server_mode = 1;
       if (scan_code == 1) {
         // aggressively search for stripped functions
         sr = init_server(DiscoverFnTy_Aggressive, _infd, _outfd);
       } else {
         // conservatively search for stripped functions
         sr = init_server(DiscoverFnTy_Conservative, _infd, _outfd);
       }
       // init_server returns 0 when done.  If it has finished, us too.
       // if an error in init_server is reported, we report it here too.
       return sr;
      }
      // any other arguments must be the name of a load objects to process
      // First, check to see if environment varirable HPCFNBOUNDS_NO_USE is set
      if (disable_init == 0 ) {
        char *str = getenv( "HPCFNBOUNDS_NO_USE" );
        if (str != NULL) {
          disable_sources (str);
        }
        disable_init = 1;
      }

      // Process the next argument
      if (verbose) {
        fprintf(stderr, "\nBegin processing load-object %s\n", *p);
      }
      ret = get_funclist (*p);
      if ( ret != NULL) {
        fprintf(stderr, "\nFailure processing load-object %s: %s\n", *p, ret );
      } else {
        if (verbose) {
          fprintf(stderr, "\nSuccess processing load-object %s\n", *p);
        }
      }
      p ++;
  }

  return 0;
}

char *
get_funclist(char *name)
{
  int fd;
  char  *ret = NULL;
  uint64_t k;
  Elf   *e;
  
  refOffset = 0;

  // Make sure function list array is allocated
  // And that initial values are set to something reasonable in case of early exit
  //
  if ( farray == NULL) {
    farray = (Function_t*) valloc(MAX_FUNC * sizeof(Function_t) );  
    maxfunc = MAX_FUNC;
    nfunc = 0;
    //
    // This is a "safe" initialization.  If it causes extra memory to be
    // mapped (i.e. memory usage increases markedly, it could be removed.
    //
    for (k = 0; k < maxfunc; k++) {
      farray[k].fadd = 0ull;
      farray[k].fnam = NULL;
      farray[k].src = SC_FNTYPE_NONE;
      farray[k].fr_fnam = FR_NO;
    }
    if ( verbose) {
      fprintf(stderr, "Initial farray allocated for %ld functions\n", maxfunc);
    }
  }
  xname = name;

  // Special-case the name "[vdso]"
  if (strcmp (name, "[vdso]") == 0 ) {
    ret = process_vdso();
    cleanup();
    return ret;
  }

  // open the file
  fd = open(name, O_RDONLY);
  if (fd == -1) {
    // the open failed
    sprintf( ebuf, "open failed -- %s", strerror(errno) );
    cleanup();  // ensure farray freed in case of error
    return ebuf;
  }

  if (elf_version(EV_CURRENT) == EV_NONE) {
    sprintf( ebuf, "%s", elf_errmsg(-1));
    close (fd);
    return ebuf;
  }

  e = elf_begin(fd,ELF_C_READ, NULL);
  if (e == NULL) {
    sprintf( ebuf, "%s", elf_errmsg(-1));
    close (fd);
    cleanup();  // ensure farray freed in case of error
    return ebuf;
  }
  
  // process the mapped header
  // ret points to either a char error buffer or is NULL,
  // in which case just return it to indicate success
  //
  ret = process_mapped_header(e);

  cleanup();
  (void) elf_end(e);
  close (fd);
  return ret;
}

void
cleanup()
{
  uint64_t i;
  //
  // clean up
  //
  if (farray != NULL) {
    for (i=0; i<nfunc; i++) {
      if (farray[i].fr_fnam == FR_YES) {
        free(farray[i].fnam);
      }
    }
    free(farray);
    farray = NULL;
    nfunc = 0;
 }

  refOffset = 0;
}

char  *
process_vdso()
{
  Elf *e = (Elf *)getauxval(AT_SYSINFO_EHDR);
#if 1
  Elf_Kind ek = elf_kind(e);
  if (ek != ELF_K_ELF) {
    sprintf( ebuf2, "Warning, vdso is not elf_kind" );
    return ebuf2;
  } else {
    sprintf( ebuf2, "Warning, vdso IS elf_kind" );
    return ebuf2;
  }
#endif
  char* ret = process_mapped_header(e);
  return ret;
}

// process_mapped_header -- verify the header, and read the functions
char *
process_mapped_header(Elf *lelf)
{
  int i;
  size_t secHeadStringIndex,nsec;
  GElf_Shdr secHead;
  Elf_Scn *section;
  GElf_Ehdr ehdr;
  char *secName;
  char foo[TB_SIZE];
  char *fn;
  size_t j,jn;
  GElf_Phdr progHeader;
  ehRecord_t ehInfo;
  uint64_t rf;
  uint32_t symTabPresent;
  uint32_t dynSymIssue;
  char elfclass;

  // verify the header is as it should be

  if (gelf_getehdr(lelf, &ehdr) == NULL) {
    sprintf(ebuf2,"%s %s\n", elfGenericErr, elf_errmsg(-1));
    return ebuf2;
  }

  if (    (ehdr.e_ident[EI_MAG0] != ELFMAG0) ||
          (ehdr.e_ident[EI_MAG1] != ELFMAG1) ||
          (ehdr.e_ident[EI_MAG2] != ELFMAG2) ||
          (ehdr.e_ident[EI_MAG3] != ELFMAG3) ) {
    sprintf( ebuf2, "incorrect elf header magic numbers" );
    return ebuf2;
  }
  elfclass = ehdr.e_ident[EI_CLASS];

  if( elfclass != ELFCLASS64 ) {
    sprintf( ebuf2, "incorrect elfclass -- 0x%x", elfclass );
    return ebuf2;
  }

  if (verbose) {
    print_elf_header64(&ehdr);
    print_program_headers64(lelf);
    print_section_headers64(lelf);
  }

  if (ehdr.e_type == ET_EXEC) {
    is_dotso = 0;
  }
  else if (ehdr.e_type == ET_DYN) {
    is_dotso = 1;
  } else {
    // we should not see this
    sprintf( ebuf2, "Filetype is neither ET_EXEC nor ET_DYN" );
    return ebuf2;
  }

  //
  // determine the load address, it's the first v_addr of a program
  // header marked PT_LOAD, with the execute flag set
  //
  if (elf_getphdrnum(lelf,&jn) != 0) {
    sprintf(ebuf2,"%s %s\n", elfGenericErr, elf_errmsg(-1));
    return ebuf2;
  }
  for (j=0; j<jn; j++) {
    if (gelf_getphdr(lelf,j,&progHeader) != &progHeader) {
      sprintf(ebuf2,"%s %s\n", elfGenericErr, elf_errmsg(-1));
      return ebuf2;
    }
    if ( (progHeader.p_type == PT_LOAD) && ((progHeader.p_flags & PF_X) == PF_X ) ) {
      refOffset = progHeader.p_vaddr;
      break;
    }
  }

  if (j == jn) {
    sprintf( ebuf2, "no executable PT_LOAD program header found of %ld headers\n",jn);
    return ebuf2;
  }

  if (elf_getshdrnum(lelf, &nsec) != 0) {
    sprintf(ebuf2,"%s %s\n", elfGenericErr, elf_errmsg(-1));
    return ebuf2;
  }

  if (elf_getshdrstrndx(lelf, &secHeadStringIndex) != 0) {
    sprintf(ebuf2,"%s %s\n", elfGenericErr, elf_errmsg(-1));
    return ebuf2;
  }

  // initialize ehRecord
  ehInfo.ehHdrSection = NULL;
  ehInfo.ehFrameSection = NULL;
  ehInfo.textSection = NULL;
  ehInfo.dataSection = NULL;
  ehInfo.ehHdrIndex = 0;
  ehInfo.ehFrameIndex = 0;

  section = NULL;

  symTabPresent = FR_NO;
  dynSymIssue = FR_NO;
  //
  // This is the main loop for traversing the sections.
  // NB section numbering starts at 1, not 0.
  //
  for(i=1; i < nsec; i++) {
    section = elf_nextscn(lelf, section);
    if (section == NULL) {
      sprintf( ebuf2, "section count mismatch, expected %d but got %d\n", (uint32_t)nsec, i);
      return ebuf2;
    }

    if (gelf_getshdr(section, &secHead) != &secHead) {
      sprintf(ebuf2,"%s %s\n", elfGenericErr, elf_errmsg(-1));
      return ebuf2;
    }
    secName = elf_strptr(lelf, secHeadStringIndex, secHead.sh_name);
    if (secName == NULL) {
      sprintf(ebuf2,"%s %s\n", elfGenericErr, elf_errmsg(-1));
      return ebuf2;
    }
    if (secHead.sh_flags == (SHF_ALLOC|SHF_EXECINSTR)) { 

      sprintf(foo, "start %s section", secName);
      fn = strdup(foo);
      add_function (secHead.sh_addr, fn, SC_FNTYPE_NONE, FR_YES);

      sprintf(foo, "end %s section", secName);
      fn = strdup(foo);
      add_function(secHead.sh_addr+secHead.sh_size, fn, SC_FNTYPE_NONE, FR_YES);
    }
      
    if (secHead.sh_type == SHT_SYMTAB) {
      rf = symtabread(lelf, secHead);
      // only skip eh_frame if symtabread was successful
      if ((rf == SC_DONE) && (symtabread_f == SC_DONE) && (dynSymIssue == FR_NO)){
        symTabPresent = FR_YES;
      }
    }
    else if (secHead.sh_type == SHT_DYNSYM) {
      rf = dynsymread(lelf, secHead);
      // force eh_frame read if something went wrong
      if ((rf == SC_SKIP) && (dynsymread_f == SC_DONE)) {
        dynSymIssue = FR_YES;
        symTabPresent = FR_NO;
      }
    }

    else if (secHead.sh_type == SHT_PROGBITS) {
      if (!strcmp(secName,".plt")) {
        pltscan(lelf, secHead); 
      }
      else if (!strcmp(secName,".init")) {
        initscan(lelf, secHead); 
      }
      else if (!strcmp(secName,".text")) {
        textscan(lelf, secHead); 
        ehInfo.textSection = section;  // may be needed for eh_frame scan
      }
      else if (!strcmp(secName,".data")) {
        textscan(lelf, secHead); 
        ehInfo.dataSection = section;  // may be needed for eh_frame scan
      }
      else if (!strcmp(secName,".fini")) {
        finiscan(lelf, secHead); 
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
        altinstr_replacementscan(lelf, secHead); 
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
  if (symTabPresent == FR_NO) {
    (void)ehframescan(lelf, &ehInfo);  
  }

  if (verbose) {
    fprintf(stderr, "\n");
  }
#if 0
  // Print the function list, unsorted
  printf ( "\n\n\tFunction list, unsorted:\n");
     for (i=0; i<nfunc; i ++) {
       printf("0x%08lx\t%s\n", farray[i].fadd, farray[i].fnam);
     }
#endif

  // We have the complete function table, now sort it
  qsort( (void *)farray, nfunc, sizeof(Function_t), &func_cmp );

  // output the result
  if (server_mode != 0) {
    // send list to server
    send_funcs();
  } else {
    // Print the function list
    print_funcs();
  }
  return NULL;
}

void
disable_sources(char *str)
{
  if (verbose) {
    fprintf(stderr, "Disabling sources \"%s\"\n", str);
  }
  int i;
  for (i = 0; ; i++) {
    switch (str[i]) {
      case 'd':
        dynsymread_f = SC_SKIP;
        break;
      case 's':
        symtabread_f = SC_SKIP;
        break;
      case 'e':
        ehframeread_f = SC_SKIP;
        break;
      case 'p':
        pltscan_f = SC_SKIP;
        break;
      case 'i':
        initscan_f = SC_SKIP;
        break;
      case 't':
        textscan_f = SC_SKIP;
        break;
      case 'f':
        finiscan_f = SC_SKIP;
        break;
      case 'a':
        altinstr_replacementscan_f = SC_SKIP;
        break;
      default:
        return;
      }
  }
}

// Routines to read the elf sections

uint64_t
dynsymread(Elf *e, GElf_Shdr sechdr)
{
  uint64_t rf;

  if (skipSectionScan(e, sechdr, dynsymread_f) == SC_SKIP) {
    return SC_SKIP;
  }

  rf = symsecread (e, sechdr, SC_FNTYPE_DYNSYM);

  return rf;

}

uint64_t 
symtabread(Elf *e, GElf_Shdr sechdr)
{
  uint64_t rf;

  if (skipSectionScan(e, sechdr, symtabread_f) == SC_SKIP) {
      return SC_SKIP;
  }

  rf = symsecread (e, sechdr, SC_FNTYPE_SYMTAB);

  return rf;

}

uint64_t
symsecread(Elf *e, GElf_Shdr secHead, char *src)
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
    fprintf(stderr, "%s %s\n", elfGenericErr, elf_errmsg(-1));
    return SC_SKIP;
  }
  data = elf_getdata(section, NULL);           // use it to get the data
  if (data == NULL) {
    fprintf(stderr, "%s %s\n", elfGenericErr, elf_errmsg(-1));
    return SC_SKIP;
  }


  count = (secHead.sh_size)/(secHead.sh_entsize);
  for (ii=0; ii<count; ii++) {
    if (gelf_getsym(data, ii, &curSym) != &curSym) {
      fprintf(stderr, "%s %s\n", elfGenericErr, elf_errmsg(-1));
      return SC_SKIP;
    }
    symName = elf_strptr(e, secHead.sh_link, curSym.st_name);
    if (symName == NULL) {
      fprintf(stderr, "%s %s\n", elfGenericErr, elf_errmsg(-1));
      return SC_SKIP;
    }
    symType = GELF_ST_TYPE(curSym.st_info);

    if ( (symType == STT_FUNC) && (curSym.st_value != 0) ) {
      add_function(curSym.st_value, symName, src, FR_NO);
      // this hack in case the symName was going away with the
      // closed elf *, but that doesn't seem to be happening.
      // marmite = strdup(symName);
      // add_function(curSym.st_value, marmite, src, FR_YES);
    }
  }

  return SC_DONE;

}

void
print_funcs()
{
  int i;
#if 0
  printf ( "\n\n\tFunction list, sorted by address:\n");
#endif

  // Print the function list
  int np = 0;
  if (nfunc > 0) {
    // print the first entry, not beginning with new line
    printf("0x%lx    %s(%s)", farray[0].fadd, farray[0].fnam, farray[0].src);
    uint64_t lastaddr = farray[0].fadd;
    np ++;

    // now do the rest of the list
    for (i=1; i<nfunc; i ++) {
      if (farray[i].fadd == lastaddr) {
        // if at the last address, just add the alias string
        printf(", %s(%s)", farray[i].fnam, farray[i].src);
      } else {
        // terminate previous entry, and start new one
        printf("\n0x%lx    %s(%s)", farray[i].fadd, farray[i].fnam, farray[i].src);
        lastaddr = farray[i].fadd;
        np ++;
      }
    }
  }
  printf("\nnum symbols = %d, reference offset = 0x%lx, relocatable = %d\n", np,
      refOffset, is_dotso );
}

void
add_function(uint64_t faddr, char *fname, char *src, uint8_t freeFlag)
{
  Function_t * of;
  uint64_t k;

  farray[nfunc].fadd = faddr;
  farray[nfunc].fnam = fname;
  farray[nfunc].src = src;
  farray[nfunc].fr_fnam = freeFlag;
  //
  // on freeFlag: FR_YES means fname was malloc'd elsewhere and needs freeing
  // after we're done with the list.  FR_NO means it's a symbol *  from an elf *, so 
  // libelf will take care of it.
  //
#if DEBUG
  fprintf(stderr, "Adding: #%6d --0x%08lx\t%s(%s)\n", nfunc, farray[nfunc].fadd,
      farray[nfunc].fnam, farray[nfunc].src);
#endif
  nfunc ++;
  if (nfunc >= maxfunc) {
    // current table is full; double its size
    maxfunc = 2*maxfunc;
    of = farray;
    farray = (Function_t *)realloc(farray, maxfunc * sizeof(Function_t) );
    if ( verbose) {
      fprintf(stderr, "Increasing farray size to %ld functions %s\n",
         maxfunc, (of == farray ? "(not moved)" : "(moved)") );
    }
    if (farray == NULL) {
      fprintf(stderr, "Fatal error: unable to increase function table to %ld functions; exiting", maxfunc);
      exit(1);
    }
    //
    // initialize the new part of the table.
    //
    for (k = nfunc; k < maxfunc; k++) {
      farray[k].fadd = 0ull;
      farray[k].fnam = NULL;
      farray[k].src = SC_FNTYPE_NONE;
      farray[k].fr_fnam = FR_NO;
    }
  }
}

int
func_cmp(const void *a, const void *b)
{
  int ret;

  Function_t fp1 = *((Function_t *) a); 
  Function_t fp2 = *((Function_t *) b); 
  if (fp1.fadd > fp2.fadd ) {
    ret = 1;
  } else if (fp1.fadd < fp2.fadd ) {
    ret = -1;
  } else {
    ret = strcmp (fp1.fnam, fp2.fnam);
  }
#if 0
  fprintf(stderr, "%d = compare (%s, 0x08lx) with (%s, 0x08lx)\n",
      ret, fp1.fnam, fp1.fadd, fp2.fnam, fp2.fadd );
#endif
  return ret;
}

void
usage()
{
  fprintf(stderr, 
      "Usage: hpcfnbounds [options] object-file\n    options are:\n"
      "\t-v\tturn on verbose output in hpcfnbounds\n"
      "\t-n <str>\tdon't use functions sources as listed in <str>\n"
      "\t    characters in <str> are interpreted as follows:\n"
      "\t\td -- skip reading.dynsym section\n"
      "\t\ts -- skip reading.symtab section\n"
      "\t\te -- skip reading.eh_frame section\n"
      "\t\tp -- skip scanning instructions from .plt section\n"
      "\t\ti -- skip scanning instructions from .init section\n"
      "\t\tt -- skip scanning instructions from .text section\n"
      "\t\tf -- skip scanning instructions from .fini section\n"
      "\t\ta -- skip scanning instructions from .altinstr_replacement section\n"
      "\t     also can be specified with environment variable HPCFNBOUNDS_NO_USE\n"
      "\t-d\tdon't perform function discovery on stripped code\n"
      "\t\t    eguivalent to -n pitfa\n"
      "\t-s fdin fdout\trun in server mode\n"
      "\t-h\tprint this help message and exit\n"
#if 0
      "\t-D\tdon't attempt to process DWARF\n"
      "\t-c\twrite output in C source code\n"
      "\t-t\twrite output in text format (default)\n"
      "If no format is specified, then text mode is used.\n"
#endif
      "\n"
  );
}
