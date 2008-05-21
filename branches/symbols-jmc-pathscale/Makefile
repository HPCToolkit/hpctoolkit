-include Makefile.conf

XED2_INCLUDE_OPTS := -I$(XED2_DIR)/include
XED2_LIBS := -L$(XED2_DIR)/lib -lxed

SPI = -I$(symtabAPI_inc) -I$(dynutil_inc)
SPL = -L$(symtabAPI_lib) -lsymtabAPI -ldwarf -lelf -lcommon


INCL += $(SPI) $(XED2_INCLUDE_OPTS)
LIBS += $(SPL) $(XED2_LIBS)

CXXFLAGS = $(INCL) -g

OBJS = symbols.o x86-process-ranges.o code-ranges.o function-entries.o intervals.o

.PHONY: build
build: stapi_syms sts
stapi_syms: $(OBJS)
	g++ -o $@ $(OBJS) $(LIBS)

symbols.o: symbols.cpp
	g++ $(CXXFLAGS) -c $< 

x86-process-ranges.o: x86-process-ranges.cpp
	g++ $(CXXFLAGS) -c $< 

code-ranges.o: code-ranges.cpp
	g++ $(CXXFLAGS) -c $< 

function-entries.o: function-entries.cpp
	g++ $(CXXFLAGS) -c $< 

cxxp = $(patsubst %/,%,$(dir $(shell ldd ./stapi_syms | awk '/libstdc\+\+/ {print $$3}')))

sts:
	@echo "#! /bin/bash" > $@
	@echo >> $@
	@echo 'LD_LIBRARY_PATH=$(cxxp):$(symtabAPI_lib):$(XED2_DIR)/lib ./stapi_syms "$$@"' >> $@
	chmod +x sts

.PHONY: install
install: build
	install -d $(PREFIX)/libexec
	cp stapi_syms $(PREFIX)/libexec

.PHONY: clean
clean:
	rm -f *.o stapi_syms csprof_syms sts *.e

.PHONY:TT
TT:
#	@echo xed2 includes; ls $(XED2_DIR)/include
	@echo cxxp = $(cxxp)
