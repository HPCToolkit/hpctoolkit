CC := gcc
CATCC := cc

PAPI_INC = $(PAPI_INCLUDE_OPTS)
PAPI_LIBS = $(PAPI_POST_LINK_OPTS)

CFLAGS := -g $(PAPI_INC) -I.. -I../monitor/src

LD       := ld
LDFLAGS  := -r -o csprof-static.o

.PHONY : clean



tprof.o: t_mon_ovr.o tdriv.o t_proc.o papi_pulse.o libc_ovr.o t_sample_callstack.o t_segv_handler.o prim_unw.o
	$(LD) -r -o $@ $^

prim_unw.o: prim_unw.c
	$(CATCC) -c -g $<

clean:
	@rm -rf *.o

%.o: %.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@
