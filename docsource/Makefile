# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
SPHINXPROJ    = daqhatLibrary
SOURCEDIR     = .
BUILDDIR      = _build

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

c:
	cd doxygen && doxygen

install:
	# copy the output to the docs dir in the daqhats tree
	rm -rf ../docs/*
	mv -f _build/html/* ../docs
	test -f _build/latex/$(SPHINXPROJ).pdf && cp -f _build/latex/$(SPHINXPROJ).pdf ../docs/_static
	touch ../docs/.nojekyll

.PHONY: help Makefile c install

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
