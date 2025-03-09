LATEX_CMD?=latex
MKIDX_CMD?=makeindex
BIBTEX_CMD?=bibtex
LATEX_COUNT?=8
MANUAL_FILE?=refman

all: $(MANUAL_FILE).dvi

ps: $(MANUAL_FILE).ps

pdf: $(MANUAL_FILE).pdf

ps_2on1: $(MANUAL_FILE).ps

pdf_2on1: $(MANUAL_FILE).pdf

$(MANUAL_FILE).ps: $(MANUAL_FILE).dvi
	dvips -o $(MANUAL_FILE).ps $(MANUAL_FILE).dvi

$(MANUAL_FILE).pdf: $(MANUAL_FILE).ps
	ps2pdf $(MANUAL_FILE).ps $(MANUAL_FILE).pdf

$(MANUAL_FILE).dvi: clean $(MANUAL_FILE).tex doxygen.sty
	echo "Running latex..."
	$(LATEX_CMD) $(MANUAL_FILE).tex
	echo "Running makeindex..."
	$(MKIDX_CMD) $(MANUAL_FILE).idx
	echo "Rerunning latex...."
	$(LATEX_CMD) $(MANUAL_FILE).tex
	latex_count=$(LATEX_COUNT) ; \
	while grep -E -s 'Rerun (LaTeX|to get cross-references right|to get bibliographical references right)' $(MANUAL_FILE).log && [ $$latex_count -gt 0 ] ;\
	    do \
	      echo "Rerunning latex...." ;\
	      $(LATEX_CMD) $(MANUAL_FILE).tex ; \
	      latex_count=`expr $$latex_count - 1` ;\
	    done
	$(MKIDX_CMD) $(MANUAL_FILE).idx
	$(LATEX_CMD) $(MANUAL_FILE).tex

$(MANUAL_FILE).ps: $(MANUAL_FILE).ps
	psnup -2 $(MANUAL_FILE).ps >$(MANUAL_FILE).ps

$(MANUAL_FILE).pdf: $(MANUAL_FILE).ps
	ps2pdf $(MANUAL_FILE).ps $(MANUAL_FILE).pdf

clean:
	rm -f *.ps *.dvi *.aux *.toc *.idx *.ind *.ilg *.log *.out *.brf *.blg *.bbl $(MANUAL_FILE).pdf
