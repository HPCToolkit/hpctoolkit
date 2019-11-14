// funclist.h - program to print the function list from its load-object arguments

#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<string.h>
#include	<sys/errno.h>
#include	<sys/mman.h>
#include	<elf.h>
#include	<libelf.h>
#include	<gelf.h>
#include	<sys/auxv.h>
#include	"code-ranges.h"


char	*get_funclist(char *);
char	*process_vdso();
char	*process_mapped_header(char *addr, int fd, size_t sz);
void	print_funcs();
void	send_funcs();
void	add_function(uint64_t, char *);
int	func_cmp(const void *a, const void *b);
void	usage();
void	cleanup();

void	init_server(DiscoverFnTy, int, int);

extern	int	server_mode;
extern	int	verbose;
extern	int	scan_code;
extern	int	no_dwarf;;
extern	int	is_dotso;

typedef struct Function {
	uint64_t	fadd;
	char	*fnam;
} Function_t;

// Define initial maximum function count
#define MAX_FUNC        65536

extern	Function_t *farray;
extern	size_t     maxfunc;
extern	size_t     nfunc;

// Debug print routines
void	print_elf_header64(Elf64_Ehdr *elf_header);
void	print_section_headers64(Elf64_Shdr *sh_table, int nsec, int strsec);

Elf	*elf;

