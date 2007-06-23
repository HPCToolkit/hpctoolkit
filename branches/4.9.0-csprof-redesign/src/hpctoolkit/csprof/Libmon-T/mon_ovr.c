#include <monitor.h>
#include <linux/unistd.h>
#define MSG(str) write(2,str"\n",sizeof(str)+1)

static int (*the_main)(int, char **, char **);

static int faux_main(int n, char **argv, char **env){
  MSG("calling regular main f faux main");
  return (*the_main)(n,argv,env);
}

void monitor_init_process(struct monitor_start_main_args *m){
  the_main = m->main;
  m->main = &faux_main;
}
