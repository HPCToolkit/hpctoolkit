extern int default_papi_count(void);
extern void papi_skip_thread_init(int flag);

extern void papi_setup(void (*handler)(int EventSet, void *pc, long long ovec, void *context));
extern void papi_over(void);
