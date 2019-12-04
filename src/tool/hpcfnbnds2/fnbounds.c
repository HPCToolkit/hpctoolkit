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

int	is_dotso;
int	server_mode = 0;

int
main(int argc, char **argv, char **envp)
{
	int	i;
	char	**p;
	char	*ret;

	p = argv;
	p ++;
	for (i = 1; i < argc; i ++) {
	    if ( strcmp (*p, "-v") == 0 ) {
		verbose = 1;
		fprintf(stderr, "\nBegin hpcfnbounds2\n" );
		p++;
		continue;
	    }
	    if ( strcmp (*p, "-d") == 0 ) {
		scan_code = 0;
		p++;
		continue;
	    }
	    if ( strcmp (*p, "-D") == 0 ) {
		no_dwarf = 1;
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
		    fprintf (stderr, "fnbounds server invocation too few arguments\n" );
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
	    if (verbose) {
		fprintf(stderr, "\nBegin processing %s\n", *p);
	    }
	    xname = *p;
	    ret = get_funclist (*p);
	    if ( ret != NULL) {
		fprintf(stderr, "\nFailure processing %s: %s\n", *p, ret );
	    } else {
		if (verbose) {
		    fprintf(stderr, "\nSuccess processing %s\n", *p);
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
/// XXXX -- temp
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
	off_t off = lseek(fd, (off_t)0, SEEK_END);
	size_t sz = (size_t)off;
	char *addr = (char *) mmap((char *)0, sz, PROT_READ, MAP_PRIVATE, fd, (off_t)0);
	elf = (Elf *) addr;
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
	elf =(Elf *)addr; 
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
	int	i, ii;
	int	nsec;

	Elf *elf=(Elf*)addr;
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
	Elf64_Shdr *sh_table= (Elf64_Shdr *) (addr + ehdr64->e_shoff);
	char *sh_str = (char *) ( (char *)elf + sh_table[(int)ehdr64->e_shstrndx].sh_offset );
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
		char *fn = strdup(foo);
		add_function ( sh_table[i].sh_addr, fn);
		secstr[nsecstr] = fn;
		nsecstr ++;

		sprintf(foo, "end %s section", sn);
		fn = strdup(foo);
		add_function ((sh_table[i].sh_addr + sh_table[i].sh_size), fn);
		secstr[nsecstr] = fn;
		nsecstr ++;
	    }

	    // If this is a symbol table, crack it
	    if ((sh_table[i].sh_type==SHT_SYMTAB)
				|| (sh_table[i].sh_type==SHT_DYNSYM)) {
		// found the symbol table section
		uint32_t str_tbl_ndx = sh_table[i].sh_link;
		char *str_tbl = (char *) (addr + sh_table[str_tbl_ndx].sh_offset);
		uint32_t symbol_count = (sh_table[i].sh_size/sizeof(Elf64_Sym));
		if (verbose) {
		    fprintf (stderr, "Processing symbol table section %s is number %d, %d symbols\n",
			(sh_str + sh_table[i].sh_name), i, symbol_count);
		}
		Elf64_Sym* sym_tbl = (Elf64_Sym*) (addr + sh_table[i].sh_offset);
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
			add_function ( sym_tbl[ii].st_value, (str_tbl + sym_tbl[ii].st_name) );
		    }
		}
	    } else {
		continue;
	    }
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
	    printf("0x%lx    %s", farray[0].fadd, farray[0].fnam);
	    uint64_t lastaddr = farray[0].fadd;
	    np ++;

	    // now do the rest of the list
	    for (i=1; i<nfunc; i ++) {
		if (farray[i].fadd == lastaddr) {
		    // if at the last address, just add the alias string
		    printf(", %s", farray[i].fnam);
		} else {
		    // terminate previous entry, and start new one
		    printf("\n0x%lx    %s", farray[i].fadd, farray[i].fnam);
		    lastaddr = farray[i].fadd;
		    np ++;
		}
	    }
	}
	printf("\nnum symbols = %d, reference offset = 0x%lx, relocatable = %d\n", np,
		(is_dotso == 0 ? firstaddr: 0), is_dotso );
}

void
add_function(uint64_t faddr, char *fname)
{
	farray[nfunc].fadd = faddr;
	farray[nfunc].fnam = fname;
#if DEBUG
	fprintf(stderr, "Adding: #%6d --0x%08lx\t%s\n", nfunc, farray[nfunc].fadd, farray[nfunc].fnam);
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
	    "Usage: hpcfnbounds [options] object-file\n\n"
	    "\t-d\tdon't perform function discovery on stripped code\n"
	    "\t-D\tdon't attempt to process DWARF\n"
	    "\t-h\tprint this help message and exit\n"
	    "\t-s fdin fdout\trun in server mode\n"
#if 0
	    "\t-c\twrite output in C source code\n"
	    "\t-t\twrite output in text format (default)\n"
	    "If no format is specified, then text mode is used.\n"
#endif
	    "\t-v\tturn on verbose output in hpcfnbounds script\n\n" );
}
