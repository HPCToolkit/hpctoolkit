#include "pmsg.h"
#include "intervals.h"

#ifndef NULL
#define NULL 0
#endif

/******************************************************************************
 * interface operations 
 *****************************************************************************/

interval_status 
build_intervals(char *ins, unsigned int len)
{
   interval_status stat;

   unwind_interval *ui = new_ui(ins, RA_BP_FRAME, 0, 16, BP_SAVED, 0, 0, NULL);
   ui->endaddr = ins + len;

   stat.first_undecoded_ins = NULL;
   stat.errcode = 0;
   stat.first = ui;

   return stat; 
}
