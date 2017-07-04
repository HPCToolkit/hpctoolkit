#include <lib/prof-lean/vdso.h>

Symtab *
symtabOpenVDSO()
{
  Symtab * the_symtab = NULL;

  char *mem_image = (char *) vdso_segment_addr();
  size_t vdso_size = vdso_segment_len();

  if (Symtab::openFile(the_symtab, mem_image, vdso_size,
		       VDSO_SEGMENT_NAME_SHORT)) {
    return the_symtab;
  }
  return NULL;
}
