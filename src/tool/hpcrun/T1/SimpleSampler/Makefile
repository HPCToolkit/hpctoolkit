CC := gcc -std=gnu99

monitor_dir := $(HOME)/csprof-install
monitor_includes := -I$(monitor_dir)/include
monitor_libs := -L$(monitor_dir)/lib -lmonitor

CFLAGS := -fPIC $(monitor_includes)

component_libs := $(monitor_libs)

lib_mods := monitor-upcalls \
            sample_source \
            sample_handler \
            thread_data

objs := $(addsuffix .o,$(lib_mods)) 

simple.so : $(objs)
	gcc -Wl,-Bsymbolic -shared -o $@ $^ $(component_libs)

%.E : %.c
	gcc $(CFLAGS) -E $< > $@

noext = $(filter-out $(wildcard *.*) makefile Makefile,$(wildcard *))

.PHONY:runtest-t1
runtest-t1: simple.so
	monitor-run -i simple.so ../t1

.PHONY: clean
clean: 
	@for f in $(noext); do \
            if [ -x $$f -a ! -d $$f ]; then \
               rm $$f; \
            fi; \
         done
	@rm -rf *.o *.E *~ *.so *.nm.c *.nm.so

.PHONY: showclean
showclean: 
	@for f in $(noext); do \
            if [ -x $$f -a ! -d $$f ]; then \
               echo would rm $$f; \
            fi; \
         done
	@echo would rm -rf *.o *.E *~ *.so *.nm.c *.nm.so

