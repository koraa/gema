
# $Id$

#	user/reference manual

TEXFILES = doc.tex overview.tex notation.tex function.tex conclude.tex

doc.dvi: ${TEXFILES}
	latex doc.tex

doc.ps: doc.dvi
	dvi2ps doc.dvi > doc.ps

view: doc.dvi
	xtex doc.dvi &

doc.html: ${TEXFILES}
	latex-to-html doc.tex doc.html

# ------------------------------------------------------------------

#	man page

gema.lpr: gema.1
	nroff -man gema.1 > $@

gema.ps: gema.1
	ptroff -t -man gema.1 > $@

gema.html: gema.1
	gema -f ../examples/man-html.dat gema.1 gema.html

# ------------------------------------------------------------------

# remove all generated files:
clean:
	rm -f *.aux doc.dvi doc.ps doc.toc doc.*log *.html *.bak \
		doc.IX* doc.LX gema.lpr gema.ps

# remove temporary files:
tidy:
	rm -f *.aux doc.toc doc.*log *.bak doc.IX*
