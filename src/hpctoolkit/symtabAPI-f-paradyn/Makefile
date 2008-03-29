-include Makefile.conf

export ROOT=$(shell pwd)
export PLATFORM=x86_64-unknown-linux2.4
export DYNINST_ROOT=$(ROOT)/symtabAPI

.PHONY: build
build: prep
	cd $(DYNINST_ROOT)/src/core; $(MAKE)

.PHONY: T
T:
	# ROOT = $(ROOT)
	# DYNINST_ROOT = $(DYNINST_ROOT)
	# PREFIX = $(PREFIX)
	@echo symtab stuff; ls $(DYNINST_ROOT)/src/$(PLATFORM)/lib/*.so
	@echo libdwarf stuff; ls $(libdwarf)/*.so
	@echo libelf stuff; ls $(libelf)/*

.PHONY: clean
clean:
	cd $(DYNINST_ROOT)/src/core; $(MAKE) -f $(ROOT)/mf.ext.mk clean
	rm -rf $(DYNINST_ROOT)/src/$(PLATFORM)

.PHONY: install
install: build
	install -d $(PREFIX)/lib
	cp $(DYNINST_ROOT)/src/$(PLATFORM)/lib/*.so   $(PREFIX)/lib
	cp $(libdwarf)/*.so                           $(PREFIX)/lib
	cp $(libelf)/*.so.0                           $(PREFIX)/lib

.DEFAULT:
	cd $(DYNINST_ROOT)/src/core; $(MAKE) -f $(ROOT)/mf.ext.mk $(MAKECMDGOALS)

.PHONY: prep
prep: $(libelf)/libelf.so

$(libelf)/libelf.so: $(libelf)/libelf.so.0
	cd $(libelf); ln -s libelf.so.0 libelf.so
