#include <sys/auxv.h>

#include "procmaps.cpp"


size_t 
lm_segment_length
(
  void *addr
)
{
  char *line = NULL;
  size_t len = 0;

  FILE* loadmap = fopen("/proc/self/maps", "r");

  for(; getline(&line, &len, loadmap) != -1;) {
    lm_seg_t s;
    lm_segment_parse(&s, line);
    if (lm_segment_contains(&s, addr)) return (((long) s.end_address) - (long) (s.start_address));
  }
  return 0;
}


Symtab *
symtabOpenVDSO()
{
  Symtab * the_symtab = NULL;
  
  unsigned long vdso_addr = getauxval(AT_SYSINFO_EHDR);
  char *mem_image = (char *) vdso_addr;
  size_t vdso_size = lm_segment_length(mem_image);

  if (Symtab::openFile(the_symtab, mem_image, vdso_size, "[vdso]")) {
    return the_symtab;
  }
  return NULL;
}
