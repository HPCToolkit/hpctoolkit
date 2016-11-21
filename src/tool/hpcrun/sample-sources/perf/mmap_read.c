
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include <signal.h>

#include <sys/mman.h>

#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <sys/prctl.h>

#include <linux/perf_event.h>


#include "perf_barrier.h"

#if defined(__x86_64__) || defined(__i386__) ||defined(__arm__)
#include <asm/perf_regs.h>
#endif


static int print_regs(int quiet,long long abi,long long reg_mask,
                unsigned char *data) {

        int return_offset=0;
        int i;

        for(i=0;i<64;i++) {
                if (reg_mask&1ULL<<i) {
                        return_offset+=8;
                }
        }

        return return_offset;
}

static int 
handle_struct_read_format(unsigned char *sample,
		     	  int read_format,
			  int quiet) {

	int offset=0,i;

	if (read_format & PERF_FORMAT_GROUP) {
		long long nr,time_enabled,time_running;

		memcpy(&nr,&sample[offset],sizeof(long long));
		offset+=8;

		if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) {
			memcpy(&time_enabled,&sample[offset],sizeof(long long));
			offset+=8;
		}
		if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) {
			memcpy(&time_running,&sample[offset],sizeof(long long));
			offset+=8;
		}

		for(i=0;i<nr;i++) {
			long long value, id;

			memcpy(&value,&sample[offset],sizeof(long long));
			offset+=8;

			if (read_format & PERF_FORMAT_ID) {
				memcpy(&id,&sample[offset],sizeof(long long));
				offset+=8;
			}

		}
	}
	else {

		long long value,time_enabled,time_running,id;

		memcpy(&value,&sample[offset],sizeof(long long));
		offset+=8;

		if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) {
			memcpy(&time_enabled,&sample[offset],sizeof(long long));
			offset+=8;
		}
		if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) {
			memcpy(&time_running,&sample[offset],sizeof(long long));
			offset+=8;
		}
		if (read_format & PERF_FORMAT_ID) {
			memcpy(&id,&sample[offset],sizeof(long long));
			offset+=8;
		}
	}

	return offset;
}


long long perf_mmap_read( void *our_mmap, int mmap_size,
			long long prev_head,
			int sample_type, int read_format, long long reg_mask,
			int quiet, int *events_read,
			int raw_type, unsigned char *data ) {

	struct perf_event_mmap_page *control_page = our_mmap;
	long long head,offset;
	int i,size;
	long long bytesize,prev_head_wrap;

	void *data_mmap=our_mmap+getpagesize();

	if (mmap_size==0) return 0;

	if (control_page==NULL) {
		return -1;
	}

	head=control_page->data_head;
	rmb(); /* Must always follow read of data_head */

	size=head-prev_head;

	bytesize=mmap_size*getpagesize();

	if ((data == NULL) || (size>bytesize)) {
		return -1;
	}

	prev_head_wrap=prev_head%bytesize;

	memcpy(data,(unsigned char*)data_mmap + prev_head_wrap,
		bytesize-prev_head_wrap);

	memcpy(data+(bytesize-prev_head_wrap),(unsigned char *)data_mmap,
		prev_head_wrap);

	struct perf_event_header *event;


	offset=0;
	if (events_read) *events_read=0;

	while(offset<size) {

		event = ( struct perf_event_header * ) & data[offset];
		offset+=8; /* skip header */

		/***********************/
		/* Print event Details */
		/***********************/

		switch(event->type) {

		/* Lost */
		case PERF_RECORD_LOST: {
			long long id,lost;
			memcpy(&id,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&lost,&data[offset],sizeof(long long));
			offset+=8;
			}
			break;

		/* COMM */
		case PERF_RECORD_COMM: {
			int pid,tid,string_size;

			memcpy(&pid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&tid,&data[offset],sizeof(int));
			offset+=4;

			/* FIXME: sample_id handling? */

			/* two ints plus the 64-bit header */
			string_size=event->size-16;
			offset+=string_size;
			}
			break;

		/* Fork */
		case PERF_RECORD_FORK: {
			int pid,ppid,tid,ptid;
			long long fork_time;

			memcpy(&pid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&ppid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&tid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&ptid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&fork_time,&data[offset],sizeof(long long));
			offset+=8;
			}
			break;

		/* mmap */
		case PERF_RECORD_MMAP: {
			int pid,tid,string_size;
			long long address,len,pgoff;

			memcpy(&pid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&tid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&address,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&len,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&pgoff,&data[offset],sizeof(long long));
			offset+=8;

			string_size=event->size-40;
			offset+=string_size;

			}
			break;

		/* mmap2 */
		case PERF_RECORD_MMAP2: {
			int pid,tid,string_size;
			long long address,len,pgoff;
			int major,minor;
			long long ino,ino_generation;
			int prot,flags;

			memcpy(&pid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&tid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&address,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&len,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&pgoff,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&major,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&minor,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&ino,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&ino_generation,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&prot,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&flags,&data[offset],sizeof(int));
			offset+=4;

			string_size=event->size-72;
			offset+=string_size;

			}
			break;

		/* Exit */
		case PERF_RECORD_EXIT: {
			int pid,ppid,tid,ptid;
			long long fork_time;

			memcpy(&pid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&ppid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&tid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&ptid,&data[offset],sizeof(int));
			offset+=4;
			memcpy(&fork_time,&data[offset],sizeof(long long));
			offset+=8;
			}
			break;

		/* Throttle/Unthrottle */
		case PERF_RECORD_THROTTLE:
		case PERF_RECORD_UNTHROTTLE: {
			long long throttle_time,id,stream_id;

			memcpy(&throttle_time,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&id,&data[offset],sizeof(long long));
			offset+=8;
			memcpy(&stream_id,&data[offset],sizeof(long long));
			offset+=8;

			}
			break;

		/* Sample */
		case PERF_RECORD_SAMPLE:
			if (sample_type & PERF_SAMPLE_IP) {
				long long ip;
				memcpy(&ip,&data[offset],sizeof(long long));
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_TID) {
				int pid, tid;
				memcpy(&pid,&data[offset],sizeof(int));
				memcpy(&tid,&data[offset+4],sizeof(int));

				offset+=8;
			}

			if (sample_type & PERF_SAMPLE_TIME) {
				long long time;
				memcpy(&time,&data[offset],sizeof(long long));
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_ADDR) {
				long long addr;
				memcpy(&addr,&data[offset],sizeof(long long));
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_ID) {
				long long sample_id;
				memcpy(&sample_id,&data[offset],sizeof(long long));
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_STREAM_ID) {
				long long sample_stream_id;
				memcpy(&sample_stream_id,&data[offset],sizeof(long long));
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_CPU) {
				int cpu, res;
				memcpy(&cpu,&data[offset],sizeof(int));
				memcpy(&res,&data[offset+4],sizeof(int));
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_PERIOD) {
				long long period;
				memcpy(&period,&data[offset],sizeof(long long));
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_READ) {
				int length;

				length=handle_struct_read_format(&data[offset],
						read_format,
						quiet);
				if (length>=0) offset+=length;
			}
			if (sample_type & PERF_SAMPLE_CALLCHAIN) {
				long long nr,ip;
				memcpy(&nr,&data[offset],sizeof(long long));
				offset+=8;

				for(i=0;i<nr;i++) {
					memcpy(&ip,&data[offset],sizeof(long long));
					offset+=8;
				}
			}
			if (sample_type & PERF_SAMPLE_RAW) {
				int size;

				memcpy(&size,&data[offset],sizeof(int));
				offset+=4;

				offset+=size;
			}

			if (sample_type & PERF_SAMPLE_BRANCH_STACK) {
				long long bnr;
				memcpy(&bnr,&data[offset],sizeof(long long));
				offset+=8;

				for(i=0;i<bnr;i++) {
					long long from,to,flags;

					/* From value */
					memcpy(&from,&data[offset],sizeof(long long));
					offset+=8;

					/* To Value */
					memcpy(&to,&data[offset],sizeof(long long));
					offset+=8;
					if (!quiet) {
		 			}

					/* Flags */
					memcpy(&flags,&data[offset],sizeof(long long));
					offset+=8;
	      			}
	   		}

			if (sample_type & PERF_SAMPLE_REGS_USER) {
				long long abi;

				memcpy(&abi,&data[offset],sizeof(long long));
				offset+=8;

				offset+=print_regs(quiet,abi,reg_mask,
						&data[offset]);

			}
#if 0
			if (sample_type & PERF_SAMPLE_REGS_INTR) {
				long long abi;

				memcpy(&abi,&data[offset],sizeof(long long));
				offset+=8;

				offset+=print_regs(quiet,abi,reg_mask,
						&data[offset]);

			}
#endif
			if (sample_type & PERF_SAMPLE_STACK_USER) {
				long long size,dyn_size;

				memcpy(&size,&data[offset],sizeof(long long));
				offset+=8;

				offset+=size;

				memcpy(&dyn_size,&data[offset],sizeof(long long));
				offset+=8;
			}

			if (sample_type & PERF_SAMPLE_WEIGHT) {
				long long weight;

				memcpy(&weight,&data[offset],sizeof(long long));
				offset+=8;
			}

			if (sample_type & PERF_SAMPLE_DATA_SRC) {
				long long src;

				memcpy(&src,&data[offset],sizeof(long long));
				offset+=8;

			}

			if (sample_type & PERF_SAMPLE_IDENTIFIER) {
				long long abi;

				memcpy(&abi,&data[offset],sizeof(long long));
				offset+=8;
			}

#if 0
			if (sample_type & PERF_SAMPLE_TRANSACTION) {
				long long abi;

				memcpy(&abi,&data[offset],sizeof(long long));
				offset+=8;

			}
			break;
#endif

#if 0
		/* AUX */
		case PERF_RECORD_AUX: {
			long long aux_offset,aux_size,flags;
			long long sample_id;

			memcpy(&aux_offset,&data[offset],sizeof(long long));
			offset+=8;

			memcpy(&aux_size,&data[offset],sizeof(long long));
			offset+=8;

			memcpy(&flags,&data[offset],sizeof(long long));
			offset+=8;

			memcpy(&sample_id,&data[offset],sizeof(long long));
			offset+=8;

			}
			break;

		/* itrace start */
		case PERF_RECORD_ITRACE_START: {
			int pid,tid;

			memcpy(&pid,&data[offset],sizeof(int));
			offset+=4;

			memcpy(&tid,&data[offset],sizeof(int));
			offset+=4;
			}
			break;

		/* lost samples PEBS */
		case PERF_RECORD_LOST_SAMPLES: {
			long long lost,sample_id;

			memcpy(&lost,&data[offset],sizeof(long long));
			offset+=8;

			memcpy(&sample_id,&data[offset],sizeof(long long));
			offset+=8;
			}
			break;
#endif
		/* context switch */
#if 0
		case PERF_RECORD_SWITCH: {
			long long sample_id;

			memcpy(&sample_id,&data[offset],sizeof(long long));
			offset+=8;
			}
			break;

		/* context switch cpu-wide*/
		case PERF_RECORD_SWITCH_CPU_WIDE: {
			int prev_pid,prev_tid;
			long long sample_id;

			memcpy(&prev_pid,&data[offset],sizeof(int));
			offset+=4;

			memcpy(&prev_tid,&data[offset],sizeof(int));
			offset+=4;

			memcpy(&sample_id,&data[offset],sizeof(long long));
			offset+=8;
			}
			break;
		default:
#endif

		}
		if (events_read) (*events_read)++;
	}

	control_page->data_tail=head;

	return head;

}
