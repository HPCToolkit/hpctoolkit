CC=gcc
PAPI_INCLUDES  := -I/projects/pkgs/papi/include
PAPI_LD        := -L/projects/pkgs/papi/lib -lpapi -lpthread

CFLAGS=-g $(PAPI_INCLUDES)
LDLIBS=$(PAPI_LD) -lm

#CFLAGS=-g $(PAPI_INCLUDE_OPTS_XD)
#LDLIBS=$(PAPI_LD_OPTS_XD) -lm

papi2: papi2.o tp.o tokenize.o
papi1: papi1.o t1.o tokenize.o

NO_EXT_FILES = $(shell ls | egrep -v '\..*$$')

.PHONY: cleanexec
cleanexec: $(NO_EXT_FILES)
	@for v in $^; do \
           if [ -x $$v ]; then \
              rm $$v; \
           fi; \
         done

.PHONY: showexec
showexec: $(NO_EXT_FILES)
	@for v in $^; do \
           if [ -x $$v ]; then \
              echo $$v; \
           fi; \
         done

.PHONY: clean
clean: cleanexec
	@rm -rf *.o *.so *.a

.PHONY: TT

TT:
	# possible executable files = $(NO_EXT_FILES)
	$(MAKE) showexec

