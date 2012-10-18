#include <pthread.h>
#include <stdio.h>

#ifdef HPCTOOLKIT_DEBUG
#include <messages/messages.h>
#else 
#define TMSG(...)
#endif



/************************************************************
 * external declarations 
 ***********************************************************/

extern void idle_metric_blame_shift_idle(void);
extern void idle_metric_blame_shift_work(void);
extern void idle_metric_register_blame_source(void);

#define IDLE() idle_metric_blame_shift_idle()
#define WORKING() idle_metric_blame_shift_work()



/************************************************************
 * forward declarations 
 ***********************************************************/

int __real_pthread_yield();
int __real_pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int __real__xlsmpSyncWorkShareItemL2(long x, long y, long z, long other);
int __real__xlsmpSyncRegionItemL2Fence(long x, long y, long z);
int __real__xlsmpBarrier_TPO(long x, long y);
int __real__xlsmpWSDoSetup_TPO(long a1, long a2, long a3, long a4,
                               long a5, long a6, long a7, long a8);

// _xlsmp_DynamicChunkCall
int __real__xlsmp_DynamicChunkCall(long a1, long a2, long a3, long a4,
                                   long a5);



/************************************************************
 * private operations
 ***********************************************************/

static void __attribute__ ((constructor))
init_xlomp_plugin(void) 
{
  idle_metric_register_blame_source();
}



/************************************************************
 * interface operations
 ***********************************************************/

int
__wrap__xlsmpBarrier_TPO(long x, long y)
{
  int ret;
  TMSG(IDLE, "enter barrier");
  IDLE();
  ret = __real__xlsmpBarrier_TPO(x, y);
  WORKING();
  TMSG(IDLE, "exit barrier");
  return ret;
}


int
__wrap__xlsmpWSDoSetup_TPO(long a1, long a2, long a3, long a4,
                           long a5, long a6, long a7, long a8)
{
  int ret;
  TMSG(IDLE, "enter dosetup");
  IDLE();
  ret =  __real__xlsmpWSDoSetup_TPO(a1, a2, a3, a4,
                                    a5, a6, a7, a8);
  WORKING();
  TMSG(IDLE, "exit dosetup");
  return ret;
}


int
__wrap__xlsmp_DynamicChunkCall(long a1, long a2, long a3, long a4,
                               long a5)
{
  int ret;
#ifdef BLAME_DYN_CHUNK
  TMSG(IDLE, "enter chunk");
  WORKING();
#endif
  ret =  __real__xlsmp_DynamicChunkCall(a1, a2, a3, a4,
                                        a5);
#ifdef BLAME_DYN_CHUNK
  IDLE();
  TMSG(IDLE, "exit chunk");
#endif
  return ret;
}


int 
__wrap__xlsmpSyncWorkShareItemL2(long x, long y, long z, long other)
{
   int ret;
   TMSG(IDLE, "enter sync");
   IDLE();
   ret = __real__xlsmpSyncWorkShareItemL2(x,y,z,other);
   WORKING();
   TMSG(IDLE, "exit sync");
   return ret;
}


int 
__wrap__xlsmpSyncRegionItemL2Fence(long x, long y, long z)
{
   int ret;
   TMSG(IDLE, "enter fence");
   IDLE();
   ret = __real__xlsmpSyncRegionItemL2Fence(x,y,z);
   WORKING();
   TMSG(IDLE, "exit fence");
   return ret;
}


int 
__wrap_pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
   int ret;
   TMSG(IDLE, "enter cond");
   IDLE();
   ret = __real_pthread_cond_wait(c, m);
   WORKING();	
   TMSG(IDLE, "exit cond");
   return ret;
}


int 
__wrap_pthread_yield()
{
   int ret;
   TMSG(IDLE, "enter yield");
   IDLE();
   ret = __real_pthread_yield();
   WORKING();	
   TMSG(IDLE, "exit yield");
   return ret;
}
