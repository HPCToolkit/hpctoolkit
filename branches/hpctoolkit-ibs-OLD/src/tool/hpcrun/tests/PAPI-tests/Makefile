CC := gcc -std=c99
papi_dir := /projects/hpc/pkgs/papi-3.6.0
#papi_dir := /projects/hpc/pkgs/papi-2008.02.15-pfm

CFLAGS := -g -I. -I$(papi_dir)/include
papi_ld := -L$(papi_dir)/lib -lpapi


.PHONY: tpu
tpu: papi-u
	perl -pe 's@\^PAPI\^@$(papi_dir)/lib@' tpu.in > tpu
	chmod +x tpu

papi-u: cycles.o papi-simple.o simple-handler.o
	$(CC) $^ -lm $(papi_ld) -o $@

papi-u-o : papi.unthreaded.o
	$(CC) $^ -lm $(papi_ld) -o $@


noext = $(filter-out $(wildcard *.*) Makefile,$(wildcard *))

.PHONY: clean
clean:
	@for f in $(noext); do \
            if [ -x $$f -a ! -d $$f ]; then \
               rm $$f; \
            fi; \
         done
	@rm -f *.o *.E *~ tpu

.PHONY: showclean
showclean:
	@for f in $(noext); do \
            if [ -x $$f -a ! -d $$f ]; then \
               echo would rm $$f; \
            fi; \
         done


%.E : %.c
	$(CC) $(CFLAGS) -E $< > $@

