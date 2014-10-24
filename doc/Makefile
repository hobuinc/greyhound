RST2PDF=rst2pdf

all: administration clientDevelopment

administration:
	$(RST2PDF) -b 0 -s text.style --stylesheet-path . administration.rst

clientDevelopment:
	$(RST2PDF) -b 0 -s text.style --stylesheet-path . clientDevelopment.rst

clean:
	-rm -rf *.pdf

