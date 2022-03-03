GEMA = $(shell which gema 2> /dev/null || echo "../src/gema")

# $Id$

all: doc.pdf doc.ps gema.pdf gema.ps doc.html gema.html gema.lpr

%.pdf: %.ps
	ps2pdf $<

#	user/reference manual

TEXFILES = doc.tex overview.tex notation.tex function.tex conclude.tex

doc.dvi: $(TEXFILES)
	latex doc.tex || rm -f doc.dvi

doc.ps: doc.dvi
	dvi2ps .dvi > $@ || dvips doc.dvi -o $@

view: doc.dvi
	xtex doc.dvi &

doc.html: $(TEXFILES) latex-to-html
	./latex-to-html doc.tex $@

latex-to-html: $(GEMA)
	$(GEMA) -line -literal './' -p '/*/latex.dat=../examples/latex.dat' \
		-in ../examples/latex.sh -out $@
	chmod a+x $@

# ------------------------------------------------------------------

#	man page

gema.lpr: gema.1
	nroff -man gema.1 > $@

gema.ps: gema.1
	ptroff -t -man gema.1 > $@ || groff -Tps -man gema.1 > $@

gema.html: gema.1 $(GEMA)
	$(GEMA) -f ../examples/man-html.dat gema.1 $@

# ------------------------------------------------------------------

../src/gema:
	cd ../src; ./gemabuild

# ------------------------------------------------------------------

# remove all generated files:
clean:
	rm -f *.aux doc.dvi doc.ps doc.toc doc.*log *.html *.bak \
		doc.IX* doc.LX gema.lpr gema.ps  gema.pdf doc.pdf

# remove temporary files:
tidy:
	rm -f *.aux doc.toc doc.*log *.bak doc.IX*
