# Project: 	Documentation Tools
# Descr:	Latex -->  MAN-page (groff -man), Makefile
# Author:	Dr. Jürgen Vollmer, Juergen.Vollmer@informatik-vollmer.de
# $Id: Makefile,v 1.106 2008/11/11 07:17:29 vollmer Exp $

BASE		= latex2man

#-------- user configuration section
BIN_DIR		= $(HOME)/bin
MAN_DIR		= $(HOME)/man
HTML_DIR	= $(HOME)/public_html
INFO_DIR	= $(HOME)/info
TEX_DIR		= $(HOME)/tex/inputs
CFG_DIR		= $(HOME)/tex/inputs
#-------- end of user configuration section

DATE		= `date +%Y-%m-%d`
DATE2		= `date +%Y/%m/%d`
TAR_BAK		= $(BASE)-$(DATE)-backup.tar.gz
TAR_SRC		= $(BASE)-$(DATE)-source.tar.gz
TAR_DIST	= $(BASE)-$(VERSION).tar.gz
EX		= --exclude "*.o" --exclude core --exclude "*.tar.gz" --exclude "*~" \
		  --exclude "*.a" --exclude "*.php"
EXrcs		= $(EX) --exclude "*/RCS/*"  --exclude "*/RCS"

VERSION		= 1.23
V_DATE		= 2008/11/11
latex2man	= ./latex2man

D		= $

HOMEPAGE	= $(HOME)/Privat/informatik-vollmer/software

.PHONY:    all clean realclean diff backup src-tar
#.SUFFIXES: .tex .html .ps .info .texi .txt

all: $(BASE).1 $(BASE).html  $(BASE).texi $(BASE).info $(BASE).txt \
		$(BASE).pdf
	$(MAKE) clean

%.ps: %.tex
	latex $*.tex
	latex $*.tex
	dvips -o $*.ps $*.dvi

%.pdf: %.tex
	rm -f /tmp/$*.tex /tmp/$*.pdf /tmp/$*.aux  /tmp/$*.toc /tmp/$*.log /tmp/$*.sty
	(cp $*.tex $*.sty /tmp  && \
	 cd /tmp         	&& \
	 pdflatex $*.tex 	&& \
	 pdflatex $*.tex);
	mv /tmp/$*.pdf .

%.info: %.texi
	makeinfo $*.texi

%.txt: %.1
	groff -t -man -Tlatin1 $*.1      |	\
		sed -e 's/.//g' 	\
		    -e 's/7|//g' 	\
		    -e 's/[6-9]//g' 	\
		    -e 's///g'      	\
		    -e 's/­$$/- /'      > $*.txt

%.1: %.tex
	$(latex2man) -M $*.tex $*.1

%.html: %.tex
	$(latex2man) -H $*.tex $*.html

%.texi: %.tex
	$(latex2man) -T $*.tex $*.texi

CHANGES.html: CHANGES
	sed -e's/&/\&amp;/g' -e's/>/\&gt;/g' -e's/</\&lt;/g' \
		< CHANGES > CHANGES.html

$(BASE).php: $(BASE).php.src Makefile CHANGES.html
	 sed < $(BASE).php.src 				\
	     -e "s|@VERSION@|$(VERSION)|g"		\
	     -e "s|@DATE@|$(V_DATE)|g"			\
	     -e "/@CHANGELOG@/r CHANGES.html"		\
	     -e "/^@.*@$$/d"				\
	     -e "s|$(BASE).src|$(BASE)|g"		\
	     -e "s|$(D)\(Id: $(BASE),v .*\) $(D)|\1|g" |\
	 sed  -e "/$(D)Id: CHANGES,v/d"			\
	 > $(BASE).php
	rm -f CHANGES.html

latex2man-html.php: latex2man.html Makefile
	@ (echo '<?php';						\
	   echo ' $$prefix    = "..";';					\
           echo ' $$logo      = "abakus.jpg";';				\
	   echo ' $$logo_text = "russischer Abakus";';			\
	   echo ' $$language  = "en";';					\
	   echo ' include "$$prefix/scripts/header.inc"';		\
	   echo '?>';							\
	   sed < latex2man.html 					\
	    -e "1,/<body*/d"						\
            -e "/<h4 align=center>/d"					\
	    -e "s/ align=center//g"					\
	    -e "/<\/body>/d"						\
	    -e "/<\/html>/d";						\
	   echo '<?php include "$$prefix/scripts/footer.inc"?>';	\
	  ) > latex2man-html.php

latex2man-text.php: latex2man.txt Makefile
	@ (echo '<?php';							\
	   echo ' $$prefix    = "..";';						\
           echo ' $$logo      = "abakus.jpg";';					\
	   echo ' $$logo_text = "russischer Abakus";';				\
           echo ' $$langauge  = "en";';						\
	   echo ' include "$$prefix/scripts/header.inc"';			\
	   echo '?>';								\
	   echo '<h1>LaTeX2MAN<br><small>plain text / UNIX-man</small></h1>';	\
	   echo '<pre';							        \
	   echo '     style="border-style:ridge;border-width:2px;">';		\
	   cat  latex2man.txt |						        \
                sed -e's/&/\&amp;/g' -e's/>/\&gt;/g' -e's/</\&lt;/g' ;	 	\
	   echo '</pre>';							\
	   echo '<?php include "$$prefix/scripts/footer.inc"?>';		\
	  ) > latex2man-text.php

homepage: latex2man.pdf latex2man.txt latex2man.php latex2man-html.php \
	  latex2man-text.php latex2man.info
	cp -fp $(TAR_DIST) latex2man.pdf latex2man*.php latex2man.info $(HOMEPAGE)
	cp -fp CHANGES $(HOMEPAGE)/latex2man-CHANGES
	rm -f *.php

install: realclean all
	@echo
	@if [ ! -f $(INFO_DIR)/dir ]; 					\
	then 								\
	  (echo ;							\
	   echo "";							\
	   echo "File: dir	Node: Top	This is the top of the INFO tree";	\
	   echo "* Menu: " )						\
	   > $(INFO_DIR)/dir ;						\
	fi
	cp -f $(BASE)		  $(BIN_DIR)
	cp -f $(BASE).sty	  $(TEX_DIR)
	cp -f $(BASE).cfg	  $(CFG_DIR)
	cp -f $(BASE).1   	  $(MAN_DIR)/man1
	cp -f $(BASE).html     	  $(HTML_DIR)
	cp -f $(BASE).info	  $(INFO_DIR)
	install-info $(BASE).info $(INFO_DIR)/dir
	$(MAKE) clean

uninstall:
	rm -f $(BIN_DIR)/$(BASE)
	rm -f $(TEX_DIR)/$(BASE).sty
	rm -f $(CFG_DIR)/$(BASE).cfg
	rm -f $(MAN_DIR)/man1/$(BASE).1
	rm -f $(HTML_DIR)/$(BASE).html
	rm -f $(INFO_DIR)/$(BASE).info
	- install-info --quiet --remove $(BASE).info $(INFO_DIR)/dir

src-tar:
	$(MAKE) realclean all
	@cd ..; tar $(EXrcs)  -czvf $(BASE)/$(TAR_SRC) $(BASE)

dist:
	rm -f THIS-IS-VERSION-*
	@- co -l latex2man latex2man.tex latex2man.sty README
	@rm -f .xxx; cp latex2man .xxx
	@sed < .xxx -e 's/$VERSION = .*/$VERSION = "$(VERSION)";/' > latex2man
	@rm -f .xxx; cp latex2man.tex .xxx
	@sed < .xxx -e 's/^\\setVersion{[0-9]*\(\.[0-9]*\)*}/\\setVersion{$(VERSION)}/'  \
		    -e 's|^\\setDate{[0-9]*/[0-9]*/[0-9]*}|\\setDate{$(V_DATE)}|'   \
		     > latex2man.tex
	@rm -f .xxx; cp latex2man.sty .xxx
	@sed < .xxx -e 's/^%% VERSION:.*/%% VERSION: $(VERSION)/' 		        \
                    -e "s|^\(\\\\ProvidesPackage{latex2man}\).*|\1[$(DATE2) v$(VERSION)]|"\
             > latex2man.sty
	rm -f .xxx; cp README .xxx
	@sed < .xxx -e 's/^VERSION:.*/VERSION: $(VERSION)/' > README
	@rm -f .xxx
	ci -u -m"Distribution" latex2man latex2man.tex latex2man.sty README
	- $(MAKE) realclean all
	@echo "VERSION $(VERSION) DATE=$(V_DATE)" > THIS-IS-VERSION-$(VERSION)
	@cd ..; tar $(EXrcs)  -czvf $(BASE)/$(TAR_DIST)	\
			$(BASE)/latex2man		\
			$(BASE)/latex2man.tex		\
			$(BASE)/latex2man.cfg		\
			$(BASE)/latex2man.sty 		\
			$(BASE)/latex2man.trans		\
			$(BASE)/latex2man.1		\
			$(BASE)/latex2man.txt		\
			$(BASE)/latex2man.html		\
			$(BASE)/latex2man.texi		\
			$(BASE)/latex2man.info		\
			$(BASE)/latex2man.pdf		\
			$(BASE)/Makefile 		\
			$(BASE)/README	 		\
			$(BASE)/INSTALL 		\
			$(BASE)/CHANGES 		\
			$(BASE)/THIS-IS-VERSION-$(VERSION)

backup:
	@ cd ..; tar $(EX) -czvf /tmp/$(TAR_BAK) $(BASE)
	@ copy-to-backup -al -d $(BASE) /tmp/$(TAR_BAK)
	@ rm /tmp/$(TAR_BAK)

clean:
	rm -f *~ *.aux *.log *.dvi .xxx *.out

realclean: clean
	rm -f *.ps *.1 *.html *.texi *.info *.txt *.pdf

diff:
	@echo
	@echo "Check for differences with RCS"
	@echo
	for i in RCS/* ; do [ -f `basename $$i ,v` ] && rcsdiff -q $$i; done
	@echo

