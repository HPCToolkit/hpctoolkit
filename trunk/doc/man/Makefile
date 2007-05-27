
TARGS = \
	hpctoolkit.html hpctoolkit.1hpctoolkit \
	bloop.html bloop.1hpctoolkit \
	hpcquick.html hpcquick.1hpctoolkit \
	hpcview.html hpcview.1hpctoolkit \
	hpcprof.html hpcprof.1hpctoolkit \
	hpcviewer.html hpcviewer.1hpctoolkit

all: ${TARGS}

clean:
	rm ${TARGS}

LATEX2MAN = ~/Develop/HPCToolkitOSS.trunk/HPCToolkit/tools/latex2man/latex2man
LATEX2MAN_TRANS = hpctoolkit-latex2man.trans

%.1hpctoolkit: %.tex
	$(LATEX2MAN) -t $(LATEX2MAN_TRANS) $< $@

%.html: %.tex
	$(LATEX2MAN) -H -t $(LATEX2MAN_TRANS) $< $@
