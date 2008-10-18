#include <stdio.h>
#include <limits.h>
#include "executable-path.h"

int harness(const char *filename, const char *path_list);

int main(int argc, char **argv)
{
  char path[PATH_MAX];
  const char *ld_library_path =  ".:/lib64:/usr/lib";
  char *self = "test-executable-path";
  int result = 0;

  /* relative path name */
  result |= harness(self, ld_library_path);

  /* absolute path name */
  if (realpath(self, path)) harness(path, ld_library_path);
  else {
    result = 1;
    printf("realpath failed to compute absolute path for this executable!\n");
  }
  result |= harness("libm.so", ld_library_path);
  result |= harness("libdl.so", ld_library_path);
  return result ? -1: 0;
}

int harness(const char *filename, const char *path_list)
{
  char executable[PATH_MAX];
  char *result = executable_path(filename, path_list, executable);
  printf("search for executable '%s' using path '%s', result = '%s'\n", 
	 filename, path_list, result);
  return (result == NULL);
}
