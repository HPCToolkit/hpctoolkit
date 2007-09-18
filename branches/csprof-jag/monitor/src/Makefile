# $Id: Makefile,v 1.14 2007/02/15 18:10:51 mucci Exp $
TARGETS=$(SONAME) monitor-config

.PHONY: all
all: $(TARGETS)

$(SONAME): monitor_preload.o
	$(CC) $(CFLAGS) $(SHRLDFLAGS) $< -ldl -Wl,-soname -Wl,$(SOVERSION) $(LDFLAGS) -o $@ && \
	ln -sf $(SONAME) $(SOVERSION) && \
	ln -sf $(SOVERSION) $(SOBASE) 

monitor_preload.o: monitor_preload.c monitor_preload.h monitor.h
	$(CC_PTHR) $(CFLAGS) $(SHRCFLAGS) -c $<

monitor-config: monitor-config.in
	@sed -e 's\PREFIX\$(PREFIX)\g; s\EXECPREFIX\$(EXECPREFIX)\g; s\INCDIR\$(INCDIR)\g; s\LIBDIR\$(LIBDIR)\g; s\MANDIR\$(MANDIR)\g' < $< > $@
	@chmod +x monitor-config

install:

uninstall:
