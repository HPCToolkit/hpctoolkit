#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>

#define MAX_CMD_VAL 8192
#define MAX_FILE_NAME 1024
int main(int argc, char **argv)
{
  if(argc < 2) {
    printf("please specify the name of measurement directory\n");
    exit(1);
  }

  char *name = (char *)malloc(strlen(argv[1])+5);
  sprintf(name, "%s-cpu", argv[1]);
  
  unlink(name);
  mkdir(name, 0755);
  

  DIR *dir;
  struct dirent *ent;
  dir = opendir(argv[1]);
  if(dir == NULL) {
    printf("measurement directory not exist\n");
    exit(1);
  }
  char *pos;
  while ((ent = readdir(dir)) != NULL) {
    if((pos=strstr(ent->d_name, "cpuhpcrun")) || (pos=strstr(ent->d_name, "cpuhpctrace"))) {
      char tmp[MAX_FILE_NAME];
      strncpy(tmp, ent->d_name, pos-ent->d_name);
      char nfn[MAX_FILE_NAME];
      if(strstr(ent->d_name, "cpuhpcrun")) 
        sprintf(nfn, "%shpcrun", tmp);
      else
        sprintf(nfn, "%shpctrace", tmp);
      char cmd[MAX_CMD_VAL];
      sprintf(cmd, "cp %s/%s %s/%s", argv[1], ent->d_name, name, nfn);
      system (cmd);
    }
  }
  return 0;
}
