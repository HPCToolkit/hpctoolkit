#
#  Make html and text files from texinfo.
#
#  make -- use makeinfo to convert .texi to html and text.
#  make install -- copy .html files to ../www/
#

MAKEINFO = makeinfo

BASE_FILES = software-instructions

README = README.Install
README_SOURCE = software-instructions.texi

HTML_FILES = $(addsuffix .html,$(BASE_FILES))
TEMP_FILES = $(addsuffix .temp,$(BASE_FILES))

FILTER = ./filter-html.pl

#------------------------------------------------------------

.PHONY: all install clean

# Don't automatically delete intermediate files.
.SECONDARY: $(TEMP_FILES)

all: $(HTML_FILES) $(README)

install: all
	for f in $(HTML_FILES) ; do  \
		cp -f "$$f" "../www/$$f" ;  \
	done
	cp -f $(README) ../../$(README)

$(README): $(README_SOURCE)
	$(MAKEINFO) --plaintext  $< -o $@

%.temp: %.texi
	$(MAKEINFO) --html --no-split $< -o $@

%.html: %.temp
	$(FILTER) $< >$@

clean:
	rm -f $(TEMP_FILES) $(HTML_FILES) $(README)

