//
// utility (unix specific) to make a given filename readable
//

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void
pubread(char *fn)
{
  struct stat ss;

  int r = stat(fn,&ss);
  mode_t m = ss.st_mode;
  r = chmod(fn,m | S_IRGRP | S_IROTH);
}
