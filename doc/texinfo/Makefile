#
#  Make html and text files from texinfo.
#
#  make -- use makeinfo to convert .texi to html and text.
#  make install -- copy .html files to ../www/
#

MAKEINFO = makeinfo

BASE_FILES = software-instructions spack-issues

HTML_FILES = $(addsuffix .html,$(BASE_FILES))
TEMP_FILES = $(addsuffix .temp,$(BASE_FILES))
TEXT_FILES = $(addsuffix .txt,$(BASE_FILES))

README_INSTALL = ../../README.Install
README_ISSUES =  ../../spack/spack-issues.txt

FILTER = ./filter-html.pl

#------------------------------------------------------------

.PHONY: all install clean

# Don't automatically delete intermediate files.
.SECONDARY: $(TEMP_FILES)

all: $(HTML_FILES) $(TEXT_FILES)

install: all
	for f in $(HTML_FILES) ; do  \
		cp -f "$$f" "../www/$$f" ;  \
	done
	cp -f software-instructions.txt $(README_INSTALL)
	cp -f spack-issues.txt $(README_ISSUES)

%.temp: %.texi
	$(MAKEINFO) --html --no-split $< -o $@

%.html: %.temp
	$(FILTER) $< >$@

%.txt: %.texi
	$(MAKEINFO) --plaintext  $< -o $@

clean:
	rm -f $(TEMP_FILES) $(HTML_FILES) $(TEXT_FILES)

