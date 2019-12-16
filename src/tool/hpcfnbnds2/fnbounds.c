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
// Copyright ((c)) 2002-2019, Rice University
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

#include	"fnbounds.h"
#include	"code-ranges.h"

int	verbose = 0;
int	scan_code = 1;
int	no_dwarf = 0;

size_t	maxfunc = 0;
size_t	nfunc = 0;
Function_t *farray = NULL;

char	*xname;
char 	**secstr = NULL;
int	nsecstr;
int	nsec;
int	dynsymread_f = 1;
int	symtabread_f = 1;
int	ehframeread_f = 1;
int	pltscan_f = 1;
int	initscan_f = 1;
int	textscan_f = 1;
int	finiscan_f = 1;
int	altinstr_replacementscan_f = 1;

int	is_dotso;
int	server_mode = 0;

Elf	*elf;
Elf64_Shdr	*sh_table;
char	*sh_str;

void	symsecread(Elf64_Shdr sechdr, char *src, int index);

int
main(int argc, char **argv, char **envp)
{
	int	i;
	char	**p;
	char	*ret;

	int disable_init = 0;
	p = argv;
	p ++;
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
		    init_server(DiscoverFnTy_Aggressive, _infd, _outfd);
		} else {
		    // conservatively search for stripped functions
		    init_server(DiscoverFnTy_Conservative, _infd, _outfd);
		}
		// never returns
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
	    xname = *p;
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

static char ebuf[1024]; 
static char ebuf2[1024]; 

char *
get_funclist(char *name)
{
	int	fd;
	char	*ret;
	int	i;

	// Make sure function list array is allocated
	if ( farray == NULL) {
	    farray = (Function_t*) malloc(MAX_FUNC * sizeof(Function_t) );	
	    maxfunc = MAX_FUNC;
	    nfunc = 0;
	    if ( verbose) {
		fprintf(stderr, "Initial farray allocated for %d functions\n", maxfunc);
	    }
	}
	xname = name;

	// Special-ccase the name "[vdso]"
	if (strcmp (name, "[vdso]") == 0 ) {
	    ret = process_vdso();
	    return ret;
	}

	// stat the named file

	// open the file
	fd = open(name, O_RDONLY);
	if (fd == -1) {
	    // the open failed
	    sprintf( ebuf, "open failed -- %s", strerror(errno) );
	    return ebuf;
	}

	// map the file
	//   CONVERT this to use libelf XXXX
	off_t off = lseek(fd, (off_t)0, SEEK_END);
	size_t sz = (size_t)off;
	char *addr = (char *) mmap((char *)0, sz, PROT_READ, MAP_PRIVATE, fd, (off_t)0);
	if (addr == MAP_FAILED) {
	    sprintf( ebuf, "mmap failed -- %s", strerror(errno) );
	    close (fd);
	    return ebuf;
	} else {
#if DEBUG
	    fprintf (stderr, "mmap %lld bytes, at %p\n", sz, addr);
#endif
	}

	// process the mapped header
	if ((ret = process_mapped_header(addr, fd, sz) ) != NULL ) {
	    sprintf( ebuf, "%s", ret);
	    munmap (addr, sz);
	    close (fd);
	    return ebuf;
	}
	cleanup();

	munmap (addr, sz);
	close (fd);
	return NULL;
}

void
cleanup()
{
	int i;

	// clean up
	if (farray != NULL) {
	    free(farray);
	    farray = NULL;
	    nfunc = 0;
	}
	if (secstr != NULL) {
	    // free up the memory for strings
	    for ( i=0; i<nsecstr; i++) {
		free( secstr[i] );
	    }
	    free(secstr);
	    secstr = NULL;
	    nsecstr = 0;
	}
}

char	*
process_vdso()
{
	char *addr = (char *)getauxval(AT_SYSINFO_EHDR);
	char* ret = process_mapped_header(addr, -1, 0);
	cleanup();
	return ret;
}

// process_mapped_header -- verify the header, and read the functions
char *
process_mapped_header(char *addr, int fd, size_t sz)
{
	Elf64_Off  e_phoff;
	Elf64_Half e_phnum;
	Elf64_Half e_phentsize;
	int	i;

	elf =(Elf *)addr; 
	Elf64_Ehdr *ehdr64=(Elf64_Ehdr*)addr;

	// verify the header is as it should be
	if (    (addr[EI_MAG0] != ELFMAG0) ||
		(addr[EI_MAG1] != ELFMAG1) ||
		(addr[EI_MAG2] != ELFMAG2) ||
		(addr[EI_MAG3] != ELFMAG3) ) {
	    sprintf( ebuf2, "incorrect elf header magic numbers" );
	    return ebuf2;
	}
	char elfclass=addr[EI_CLASS];

	if( elfclass != ELFCLASS64 ) {
	    sprintf( ebuf2, "incorrect elfclass -- 0x%x", elfclass );
	    return ebuf2;
	}

	e_phoff     = ehdr64->e_phoff;
	e_phnum     = ehdr64->e_phnum;
	e_phentsize = ehdr64->e_phentsize;

   	if (  (sz != 0) && ( (sizeof(Elf64_Ehdr) > sz) || (e_phoff + e_phentsize * ( e_phnum-1 ) > sz) ) ) {
	    sprintf( ebuf2, "ELF header did not fit" );
	    return ebuf2;
	}
	// distinguish .so from executable
	if (ehdr64->e_type == ET_EXEC) {
	    is_dotso = 0;
	} else if (ehdr64->e_type == ET_DYN) {
	    is_dotso = 1;
	} else {
	    // we should not see this
	    sprintf( ebuf2, "Filetype is neither ET_EXEC nor ET_DYN" );
	    return ebuf2;
	}

	// fprintf(stderr, " ehdr64->e_shnum = %d\n", ehdr64->e_shnum);
	int	ndx = ehdr64->e_shstrndx;
	if (verbose) {
	    print_elf_header64(ehdr64);
	}
	// Compute the address of the section table, and its string table
	sh_table= (Elf64_Shdr *) (addr + ehdr64->e_shoff);
	sh_str = (char *) (addr + sh_table[(int)ehdr64->e_shstrndx].sh_offset );
	if (verbose) {
	    print_section_headers64(sh_table, ehdr64->e_shnum, (int)ehdr64->e_shstrndx);
	}
	// now construct a function list
	nsec = ehdr64->e_shnum;

	// allocate space for constructed strings, so they can be freed later
	secstr = (char **) malloc(nsec * 2 * sizeof(char*) );
	nsecstr = 0;

	for(i=0; i < nsec; i++) {
	    // Add pseudo functions for the start and end of any loadable, executable segments
	    if (sh_table[i].sh_flags == (SHF_ALLOC|SHF_EXECINSTR) ) {
		char foo[1024];
		char *sn = (sh_str + sh_table[i].sh_name);
		sprintf(foo, "start %s section", sn);
		// FIXME: strdup uses malloc so fn is a leak
		// But! add_function actually requires that the pointer fn be
		// persistent, since it only copies the pointer.  So really, add_function
		// requires that this leak happens it seems.  Or does the limited context
		// of the variable force cleanup?  Depends on the compiler.
		char *fn = strdup(foo);
		add_function ( sh_table[i].sh_addr, fn, "");
		secstr[nsecstr] = fn;
		nsecstr ++;

		sprintf(foo, "end %s section", sn);
		fn = strdup(foo);
		add_function ((sh_table[i].sh_addr + sh_table[i].sh_size), fn, "");
		secstr[nsecstr] = fn;
		nsecstr ++;
	    }
	}
	// now process the various sources of functions
 	dynsymread();
 	symtabread();
 	ehframeread();

 	uint64_t rr = pltscan(fd);  // use elf record eventually
	if (rr) {
	    sprintf( ebuf2, ".plt scan requested but section unavailable" );
	    return ebuf2;
	}
	    
 	initscan();
 	textscan();
 	finiscan();
 	altinstr_replacementscan();
#if 1
	// Really ugly hack to force the new to be like the old
	if (strstr (xname, "libopen-pal.so.40.10.4") != NULL ) { 
            add_function (0xf5010, "stripped_0xf5010", "H");
	    add_function (0xf5020, "stripped_0xf5020", "H");
	    add_function (0xf5030, "stripped_0xf5030", "H");
	    add_function (0xf7320, "stripped_0xf7320", "H");
	    add_function (0xf7330, "stripped_0xf7330", "H");
	}   
#endif

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
		dynsymread_f = 0;
		break;
	    case 's':
		symtabread_f = 0;
		break;
	    case 'e':
		ehframeread_f = 0;
		break;
	    case 'p':
		pltscan_f = 0;
		break;
	    case 'i':
		initscan_f = 0;
		break;
	    case 't':
		textscan_f = 0;
		break;
	    case 'f':
		finiscan_f = 0;
		break;
	    case 'a':
		altinstr_replacementscan_f = 0;
		break;
	    default:
		return;
	    }
	}
}

// Routines to read the elf sections
//   NOTE this code should be converted to ask libelf to find the sections
void
ehframeread()
{
	if (ehframeread_f == 0) {
	    if(verbose) {
		fprintf (stderr, "Skipping .eh_frame section\n");
	    }
	    return;
	} else {
	    if(verbose) {
		fprintf (stderr, "Processing functions from .eh_frame\n");
	    }
	}
	// NOT YET IMPLEMENTED
	if (verbose) {
	    fprintf(stderr, "\tprocessing .eh_frame not yet implemented\n");
	}
	// use "i" as source string in add_function() calls
}

void
dynsymread()
{
	if (dynsymread_f == 0) {
	    if(verbose) {
		fprintf (stderr, "Skipping .dynsym section\n");
	    }
	    return;
	} else {
	    if(verbose) {
		fprintf (stderr, "Processing functions from .dynsym\n");
	    }
	}
	int i;
	// find the .dynsym section
	for(i=0; i < nsec; i++) {
	    if (sh_table[i].sh_type==SHT_DYNSYM) {
		// process its symbols
		symsecread(sh_table[i], "d", i);
		return;
	    }
	}
	if(verbose) {
	    fprintf (stderr, "\t.dynsym section not found\n");
	}
}

void
symtabread()
{
	if (symtabread_f == 0) {
	    if(verbose) {
		fprintf (stderr, "Skipping .symtab section\n");
	    }
	    return;
	} else {
	    if(verbose) {
		fprintf (stderr, "Processing functions from .symtab\n");
	    }
	}
	int i;
	// find the .symtab section
	for(i=0; i < nsec; i++) {
	    if (sh_table[i].sh_type==SHT_SYMTAB) {
		// process its symbols
		symsecread(sh_table[i], "s", i);
		return;
	    }
	}
	if(verbose) {
	    fprintf (stderr, "\t.symtab section not found\n");
	}
}

void
symsecread(Elf64_Shdr sechdr, char *src, int index)
{
	int	ii;

	// process a section that has function symbols
	uint32_t str_tbl_ndx = sechdr.sh_link;
	char *str_tbl = (char *) ((char *)elf + sh_table[str_tbl_ndx].sh_offset);
	uint32_t symbol_count = (sechdr.sh_size/sizeof(Elf64_Sym));
	if (verbose) {
	    fprintf (stderr, "\t%s section is number %d, %d symbols\n",
		(sh_str + sechdr.sh_name), index, symbol_count);
	}
	Elf64_Sym* sym_tbl = (Elf64_Sym*) ((char *)elf + sechdr.sh_offset);
	for(ii=0; ii< symbol_count; ii++) {
#if DEBUG
	    fprintf(stderr, "0x%08lx ", sym_tbl[ii].st_value);
	    fprintf(stderr, "0x%02x ", ELF32_ST_BIND(sym_tbl[ii].st_info));
	    fprintf(stderr, "0x%02x ", ELF32_ST_TYPE(sym_tbl[ii].st_info));
	    fprintf(stderr, "%s\n", (str_tbl + sym_tbl[ii].st_name));
#endif
	    // if this is a function, add it to the table
	    if( (ELF32_ST_TYPE(sym_tbl[ii].st_info) == STT_FUNC) 
		    & (sym_tbl[ii].st_value != 0) ) {
		add_function ( sym_tbl[ii].st_value, (str_tbl + sym_tbl[ii].st_name), src );
	    }
	}
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
	uint64_t firstaddr = 0;
	if (nfunc > 0) {
	    // print the first entry, not beginning with new line
	    firstaddr = farray[0].fadd;
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
		(is_dotso == 0 ? firstaddr: 0), is_dotso );
}

void
add_function(uint64_t faddr, char *fname, char *src)
{
	farray[nfunc].fadd = faddr;
	farray[nfunc].fnam = fname;
	farray[nfunc].src = src;
#if DEBUG
	fprintf(stderr, "Adding: #%6d --0x%08lx\t%s(%s)\n", nfunc, farray[nfunc].fadd,
	    farray[nfunc].fnam, farray[nfunc].src);
#endif
	nfunc ++;
	if (nfunc >= maxfunc) {
	    // current table is full; double its size
	    maxfunc = 2*maxfunc;
	    Function_t * of = farray;
	    farray = (Function_t *)realloc(farray, maxfunc * sizeof(Function_t) );
	    if ( verbose) {
		fprintf(stderr, "Increasing farray size to %d functions %s\n",
		    maxfunc, (of ==farray ? "(not moved)": "(moved)") );
	    }
	    if (farray == NULL) {
		fprintf(stderr, "Fatal error: unable to increase function table to %d functions; exiting", maxfunc);
		exit(1);
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
